#include <Passes.h>
#include <Locks.h>
#include <Utils.h>
#include <llvm/Transforms/Utils/EscapeEnumerator.h>

using namespace llvm;

GetElementPtrInst *CreateGEP(
    LLVMContext &Context, IRBuilder<> &B, Type *Ty, Value *BasePtr, int Idx, const char *Name) {
  Value *Indices[] = {ConstantInt::get(Type::getInt32Ty(Context), 0), ConstantInt::get(Type::getInt32Ty(Context), Idx)};
  Value *Val = B.CreateGEP(Ty, BasePtr, Indices, Name);

  assert(isa<GetElementPtrInst>(Val) && "Unexpected folded constant");

  return dyn_cast<GetElementPtrInst>(Val);
}

GetElementPtrInst *CreateGEP(
    LLVMContext &Context, IRBuilder<> &B, Type *Ty, Value *BasePtr, int Idx, int Idx2, const char *Name) {
  Value *Indices[] = {ConstantInt::get(Type::getInt32Ty(Context), 0), ConstantInt::get(Type::getInt32Ty(Context), Idx),
      ConstantInt::get(Type::getInt32Ty(Context), Idx2)};
  Value *Val = B.CreateGEP(Ty, BasePtr, Indices, Name);

  assert(isa<GetElementPtrInst>(Val) && "Unexpected folded constant");

  return dyn_cast<GetElementPtrInst>(Val);
}

PreservedAnalyses LockTrackerPass::run(Module &M, ModuleAnalysisManager &AM) {
  std::vector<Type *> EltTys;

  auto pointerType = llvm::PointerType::get(M.getContext(), 0);
  auto i64Ty = IntegerType::get(M.getContext(), 64);
  auto stackEntryTy = StructType::create(M.getContext(), "alaska_stackentry");
  EltTys.clear();
  EltTys.push_back(PointerType::getUnqual(stackEntryTy));  // ptr
  EltTys.push_back(i64Ty);                                 // i64
  stackEntryTy->setBody(EltTys);

  // Get the root chain if it already exists.
  auto Head = M.getGlobalVariable("alaska_lock_root_chain");
  if (Head == nullptr) {
    Head = new GlobalVariable(M, pointerType, false, GlobalValue::ExternalWeakLinkage,
        Constant::getNullValue(pointerType), "alaska_lock_root_chain", nullptr, llvm::GlobalValue::InitialExecTLSModel);
  }
  Head->setThreadLocal(true);
  Head->setLinkage(GlobalValue::ExternalWeakLinkage);
  Head->setInitializer(NULL);


  for (auto &F : M) {
    if (F.empty()) continue;
    auto l = alaska::extractLocks(F);
    if (l.empty()) continue;
    EltTys.clear();
    EltTys.push_back(stackEntryTy);
    // enough spots for each pointerType
    for (size_t i = 0; i < l.size(); i++) {
      EltTys.push_back(pointerType);
    }
    auto *concreteStackEntryTy = StructType::create(EltTys, ("alaska_stackentry." + F.getName()).str());

    IRBuilder<> atEntry(F.front().getFirstNonPHI());
    auto *stackEntry = atEntry.CreateAlloca(concreteStackEntryTy, nullptr, "lockStackFrame");
    atEntry.CreateStore(ConstantAggregateZero::get(concreteStackEntryTy), stackEntry, true);


    auto *EntryCountPtr = CreateGEP(M.getContext(), atEntry, stackEntryTy, stackEntry, 1, "lockStackFrame.count");
    // cur->count = N;
    atEntry.CreateStore(ConstantInt::get(i64Ty, l.size()), EntryCountPtr, true);
    // cur->prev = head;
    auto *CurrentHead = atEntry.CreateLoad(stackEntryTy->getPointerTo(), Head, "lockStackCurrentHead");
    atEntry.CreateStore(CurrentHead, stackEntry, true);
    // head = cur;
    atEntry.CreateStore(stackEntry, Head, true);

    int ind = 0;
    for (auto &lock : l) {
      char buf[512];
      snprintf(buf, 512, "lockCell.%d", ind);
      auto cell = CreateGEP(M.getContext(), atEntry, concreteStackEntryTy, stackEntry, ind + 1, buf);
      ind++;

      auto handle = lock->getHandle();

      IRBuilder<> b(lock->lock);
      b.SetInsertPoint(lock->lock);
      b.CreateStore(handle, cell, true);

      // Insert a store right after all the unlocks to say "we're done with this handle".
      for (auto unlock : lock->unlocks) {
        b.SetInsertPoint(unlock->getNextNode());
        b.CreateStore(llvm::ConstantPointerNull::get(dyn_cast<PointerType>(handle->getType())), cell, true);
      }
    }

    // For each instruction that escapes...
    EscapeEnumerator EE(F, "alaska_cleanup", /*HandleExceptions=*/true, nullptr);
    while (IRBuilder<> *AtExit = EE.Next()) {
      // Pop the entry from the shadow stack. Don't reuse CurrentHead from
      // AtEntry, since that would make the value live for the entire function.
      Value *SavedHead = AtExit->CreateLoad(stackEntryTy->getPointerTo(), stackEntry, "alaska_savedhead");
      AtExit->CreateStore(SavedHead, Head);
      // AtExit->CreateStore(CurrentHead, Head);
    }
  }

  return PreservedAnalyses::none();
}