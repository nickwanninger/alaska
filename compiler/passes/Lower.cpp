#include <alaska/Passes.h>
#include <alaska/Translations.h>
#include <alaska/Utils.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <set>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/GetElementPtrTypeIterator.h>
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/PointerUnion.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/User.h"
#include "llvm/Support/Casting.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace llvm;


std::vector<llvm::CallBase *> collectCalls(llvm::Module &M, const char *name) {
  std::vector<llvm::CallBase *> calls;

  if (auto func = M.getFunction(name)) {
    for (auto user : func->users()) {
      if (auto call = dyn_cast<CallBase>(user)) {
        calls.push_back(call);
      }
    }
  }

  return calls;
}

bool collectOffsets(GetElementPtrInst *gep, const DataLayout &DL, unsigned BitWidth,
    std::unordered_map<Value *, APInt> &VariableOffsets, APInt &ConstantOffset) {
  auto CollectConstantOffset = [&](APInt Index, uint64_t Size) {
    Index = Index.sextOrTrunc(BitWidth);
    APInt IndexedSize = APInt(BitWidth, Size);
    ConstantOffset += Index * IndexedSize;
  };

  llvm::gep_type_iterator GTI = llvm::gep_type_begin(gep);

  for (unsigned I = 1, E = gep->getNumOperands(); I != E; ++I, ++GTI) {
    // Scalable vectors are multiplied by a runtime constant.
    bool ScalableType = isa<VectorType>(GTI.getIndexedType());

    Value *V = GTI.getOperand();
    StructType *STy = GTI.getStructTypeOrNull();
    // Handle ConstantInt if possible.
    if (auto ConstOffset = dyn_cast<ConstantInt>(V)) {
      // If the type is scalable and the constant is not zero (vscale * n * 0 =
      // 0) bailout.
      // TODO: If the runtime value is accessible at any point before DWARF
      // emission, then we could potentially keep a forward reference to it
      // in the debug value to be filled in later.
      if (ScalableType) return false;
      // Handle a struct index, which adds its field offset to the pointer.
      if (STy) {
        unsigned ElementIdx = ConstOffset->getZExtValue();
        const StructLayout *SL = DL.getStructLayout(STy);
        // Element offset is in bytes.
        CollectConstantOffset(APInt(BitWidth, SL->getElementOffset(ElementIdx)), 1);
        continue;
      }
      CollectConstantOffset(ConstOffset->getValue(), DL.getTypeAllocSize(GTI.getIndexedType()));
      continue;
    }

    if (STy) {
      return false;
    }

    if (ScalableType) {
      return false;
    }

    APInt IndexedSize = APInt(BitWidth, DL.getTypeAllocSize(GTI.getIndexedType()));
    // Insert an initial offset of 0 for V iff none exists already, then
    // increment the offset by IndexedSize.
    // checks if Indexed Size is zero LLVM9 APIINT
    if (!IndexedSize) {
    } else {
      VariableOffsets.insert({V, APInt(BitWidth, 0)});
      VariableOffsets[V] += IndexedSize;
    }
  }
  return true;
}



static inline void trim_inplace(std::string &s) {
  size_t b = 0, e = s.size();
  while (b < e && std::isspace(static_cast<unsigned char>(s[b])))
    ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
    --e;
  if (b == 0 && e == s.size()) return;
  s.assign(s, b, e - b);
}

static bool parse_profile_file(const char *path, std::map<std::string, char> &out) {
  std::ifstream in(path);
  if (!in.is_open()) {
    std::perror(path);
    return false;
  }

  std::string line;
  size_t line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;

    // Remove comments
    if (auto pos = line.find('#'); pos != std::string::npos) line.erase(pos);
    trim_inplace(line);
    if (line.empty()) continue;

    // First non-space char is the value
    char value = 0;
    size_t i = 0;
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])))
      ++i;
    if (i >= line.size()) continue;
    value = line[i++];

    // Skip spaces before key
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])))
      ++i;

    // Rest of the line is the key
    std::string key;
    if (i < line.size()) key = line.substr(i);
    trim_inplace(key);

    if (key.empty()) {
      llvm::errs() << "alaska: warning: " << path << ":" << line_no
                   << ": missing key after value\n";
      continue;
    }

    alaska::println("alaska: profile: '", value, "' for '", key, "'");
    out[key] = value;
  }

  return true;
}

llvm::PreservedAnalyses AlaskaLowerPass::run(llvm::Module &M, llvm::ModuleAnalysisManager &AM) {
  std::set<llvm::Instruction *> to_delete;

  // Lower GC rooting.
  if (auto func = M.getFunction("alaska.safepoint_poll")) {
    auto pollFunc = M.getOrInsertFunction("alaska_safepoint", func->getFunctionType());
    for (auto call : collectCalls(M, "alaska.safepoint_poll")) {
      call->setCalledFunction(pollFunc);
    }
  }

  // Lower alaska.root
  for (auto *call : collectCalls(M, "alaska.root")) {
    IRBuilder<> b(call);  // insert after the call
    call->replaceAllUsesWith(call->getArgOperand(0));
    to_delete.insert(call);
    if (auto *invoke = dyn_cast<llvm::InvokeInst>(call)) {
      auto *landing_pad = invoke->getLandingPadInst();
      to_delete.insert(landing_pad);
    }
  }




  // function which is (u64, u64, u64) -> void
  auto trackHitMiss = M.getOrInsertFunction("__alaska_track_hitmiss",
      FunctionType::get(Type::getVoidTy(M.getContext()),
          {PointerType::get(M.getContext(), 0), Type::getInt64Ty(M.getContext()),
              Type::getInt64Ty(M.getContext())},
          false));

  std::map<std::string, char> profilePoints;
  if (const char *profilePath = getenv("ALASKA_HPROF"); profilePath != nullptr) {
    parse_profile_file(profilePath, profilePoints);
  }

  bool addProfilerData = getenv("ALASKA_PROFON") != nullptr;

  // Lower alaska.translate
  if (auto func = M.getFunction("alaska.translate")) {
    auto translateFunc = M.getOrInsertFunction("alaska_translate", func->getFunctionType());
    auto translateFuncUncond =
        M.getOrInsertFunction("alaska_translate_uncond", func->getFunctionType());
    auto translateFuncNop = M.getOrInsertFunction("alaska_translate_nop", func->getFunctionType());

    auto translateEscapeFunc =
        M.getOrInsertFunction("alaska_translate_escape", func->getFunctionType());

    for (auto *call : collectCalls(M, "alaska.translate")) {
      IRBuilder<> b(call);  // insert after the call


      std::string ValueStr = "";
      llvm::raw_string_ostream OS(ValueStr);
      OS << call->getParent()->getParent()->getName();
      OS << ":";
      OS << call->getParent()->getName();
      OS << ":";
      call->getOperand(0)->printAsOperand(OS);
      std::string profilePoint = OS.str();

      auto translateFuncForThisSite = translateFunc;
      // Look through the profile data (if we have any) for what kind of translate function to use.
      auto it = profilePoints.find(profilePoint);
      if (it != profilePoints.end()) {
        char value = it->second;
        alaska::println("Found profile data '", value, "' for ", profilePoint);
        // alaska::println("   that is... ", *call->getOperand(0));
        switch (value) {
          case 'H':
            translateFuncForThisSite = translateFuncUncond;
            break;
          case 'P':
            translateFuncForThisSite = translateFuncNop;
            break;
          default:
            break;
        }
      } else {
        // alaska::println("No profile data for ", profilePoint);
      }

      // If the return value is only used in calls, run the more expensive "escape" variant
      bool onlyCalls = true;
      for (auto *user : call->users()) {
        if (isa<CallInst>(user)) {
          // Good!
        } else {
          // onlyCalls = false;
        }
      }
      onlyCalls = false;
      // Ensure the call has a debug location if the function has debug info,
      // otherwise the verifier rejects inlinable calls without !dbg.
      if (!call->getDebugLoc()) {
        if (auto *SP = call->getFunction()->getSubprogram()) {
          call->setDebugLoc(DILocation::get(M.getContext(), SP->getLine(), 0, SP));
        }
      }

      if (onlyCalls) {
        call->setCalledFunction(translateEscapeFunc);
      } else {
        call->setCalledFunction(translateFuncForThisSite);
      }



      if (addProfilerData) {
        // alaska::println("Inserted alaska.translate to ", *call->getOperand(0));
        // after, insert a call to trackHitMiss with a random ID, original pointer, translated
        // pointer place the builder after the call
        b.SetInsertPoint(call->getNextNode());

        auto id = b.CreateGlobalStringPtr(profilePoint.c_str());
        auto originalPtr =
            b.CreatePtrToInt(call->getArgOperand(0), Type::getInt64Ty(M.getContext()));
        auto translatedPtr = b.CreatePtrToInt(call, Type::getInt64Ty(M.getContext()));
        b.CreateCall(trackHitMiss, {id, originalPtr, translatedPtr});
      }
    }
  }



  // Lower alaska.release
  for (auto call : collectCalls(M, "alaska.release")) {
    to_delete.insert(call);
  }


  // Lower alaska.derive
  for (auto call : collectCalls(M, "alaska.derive")) {
    // Derive is just a marker to indicate that a GEP happened on a translated value.
    // In the end, we just replace it's uses with the GEP.

    IRBuilder<> b(call);  // insert after the call
    auto base = call->getArgOperand(0);
    auto offset = dyn_cast<llvm::GetElementPtrInst>(call->getArgOperand(1));

    std::vector<llvm::Value *> inds(offset->idx_begin(), offset->idx_end());
    auto gep = b.CreateGEP(offset->getSourceElementType(), base, inds, "", offset->isInBounds());
    call->replaceAllUsesWith(gep);

    to_delete.insert(call);

    // bool originalHasOtherOsers = false;
    // for (auto user : offset->users()) {
    //   if (user != call) {
    //     originalHasOtherOsers = true;
    //     break;
    //   }
    // }
    // // if the original gep has no users, delete it too.
    // if (originalHasOtherOsers) {
    //   to_delete.insert(offset);
    // } else {
    //   alaska::println("Warning: alaska.derive's GEP has other uses; not deleting:", *offset);
    // }
  }

  for (auto inst_to_delete : to_delete) {
    inst_to_delete->eraseFromParent();
  }


  //

#ifdef ALASKA_VERIFY_PASS
  for (auto &F : M) {
    llvm::verifyFunction(F);
  }
#endif

  return PreservedAnalyses::none();
}
