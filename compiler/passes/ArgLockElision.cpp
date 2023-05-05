#include <alaska/Passes.h>
#include <alaska/Translations.h>
#include <alaska/Utils.h>

using namespace llvm;

llvm::Value *RedundantArgumentLockElisionPass::getRootAllocation(llvm::Value *cur) {
  while (1) {
    if (auto gep = dyn_cast<GetElementPtrInst>(cur)) {
      cur = gep->getPointerOperand();
    } else {
      break;
    }
  }

  return cur;
}

llvm::PreservedAnalyses RedundantArgumentLockElisionPass::run(
    Module &M, ModuleAnalysisManager &AM) {
  // Which arguments does each function lock internally? (Array of true and false for each index of
  // the args)
  std::map<llvm::Function *, std::vector<bool>> locked_arguments;

  for (auto &F : M) {
    if (F.empty()) continue;
    // Which arguments do each function lock?
    auto locks = alaska::extractTranslations(F);
    std::vector<bool> locked;
    locked.reserve(F.arg_size());
    for (unsigned i = 0; i < F.arg_size(); i++)
      locked.push_back(false);

    for (auto &l : locks) {
      auto lockedAllocation = getRootAllocation(l->getHandle());
      for (unsigned i = 0; i < F.arg_size(); i++) {
        auto *arg = F.getArg(i);
        if (arg == lockedAllocation) {
          locked[i] = true;
        }
      }
    }

    locked_arguments[&F] = std::move(locked);
  }

  for (auto &[func, locked] : locked_arguments) {
    alaska::println("function: ", func->getName());
    alaska::print("\targs:");
    for (auto b : locked) {
      alaska::print(" ", b);
    }
    alaska::println();

    for (auto *user : func->users()) {
      if (auto *call = dyn_cast<CallInst>(user)) {
        if (call->getCalledFunction() != func) continue;

        auto *callFunction = call->getFunction();

        auto fLocks = alaska::extractTranslations(*callFunction);
        alaska::print("\t use:");

        for (unsigned i = 0; i < locked.size(); i++) {
          bool alreadyLocked = false;

          auto argRoot = getRootAllocation(call->getOperand(i));

          // alaska::println(i, "lock ", *argRoot);

          for (auto &fl : fLocks) {
            auto *flRoot = getRootAllocation(fl->getHandle());
            if (flRoot == argRoot) {
              if (fl->liveInstructions.count(call) != 0) {
                alreadyLocked = true;
                break;
              }
            }
          }

          alaska::print(" ", alreadyLocked);
        }
        alaska::println();
      }
    }
  }

  return PreservedAnalyses::none();
}
