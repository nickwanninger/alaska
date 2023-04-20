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

  // TODO: instead of getting this from a global variable, it would be interesting to see how versioning functions with
  // an additional argument containing the root chain might improve performance for "short" functions.  Then, when you
  // call a function it does not have to look up the address of the thread-local pointer. Effectively, we would take
  // this code:
  //
  //     void write_ptr(int *x) {
  //       root_chain = ...
  //       *x = 0;
  //     }
  //     and convert it to this code:
  //     void write_ptr(int *x) {
  //       root_chain = ...
  //       write_ptr.rc(x, root_chain);
  //     }
  //     void write_ptr.rc(int *x, root_chain *rc) {
  //       *x = 0;
  //     }
  //
  // That way, functions would simply take the pointer to the current root chain as a function, which would reduce
  // prelude overhead significantly on ARM. If the old function was called without passing the root chain, it would
  // simply load the location and call into the versioned function with that pointer.


  Head->setThreadLocal(true);
  Head->setLinkage(GlobalValue::ExternalWeakLinkage);
  Head->setInitializer(NULL);

  for (auto &F : M) {
    // Ignore functions with no bodies
    if (F.empty()) continue;
    // Extract all the locks from the function
    auto locks = alaska::extractLocks(F);
    // If the function had no locks, don't do anything
    if (locks.empty()) continue;

    // Given a lock, which cell does it belong to? (eagerly)
    std::map<alaska::Lock *, long> lock_cells;
    // Interference - a mapping from locks to the locks it is alive along side of.
    std::map<alaska::Lock *, std::set<alaska::Lock *>> interference;

    // Loop over each lock, then over it's live instructions. For each live instruction see
    // which other locks are live in those instructions. This is horrible and slow. (but works)
    for (auto &lock : locks) {
      lock_cells[lock.get()] = -1;
      // A lock interferes with itself
      interference[lock.get()].insert(lock.get());
      for (auto inst : lock->liveInstructions) {
        for (auto &other : locks) {
          if (other != lock && other->isLive(inst)) {
            // interference!
            interference[lock.get()].insert(other.get());
            interference[other.get()].insert(lock.get());
          }
        }
      }
    }

    long cell_count = 0;  // the maximum level of interference
    for (auto &[_, i] : interference) {
      if (cell_count < (long)i.size()) cell_count = (long)i.size();
    }

    fprintf(stderr, "%3ld cells required for %zu locks in %s\n", cell_count, locks.size(), F.getName().data());

    // The algorithm we use is a simple greedy algo. We don't need to do a fancy graph
    // coloring allocation here, as we don't need to worry about register spilling
    // (we can just make more "registers" instead of spilling). All cells are equal as well,
    // so there aren't any restrictions on which lock can get which cell.
    for (auto &[lock, intr] : interference) {
      long available_cell = -1;
      std::set<long> unavail;  // which cells are not available?
      for (auto other : intr) {
        long cell = lock_cells[other];
        if (cell != -1) {
          unavail.insert(cell);
        }
      }
      // find a cell that hasn't been used by any of the interfering locks.
      for (long current_cell = 0; current_cell < cell_count; current_cell++) {
        if (unavail.count(current_cell) == 0) {
          available_cell = current_cell;
          break;
        }
      }
      lock_cells[lock] = available_cell;
    }

    // Create the type that will go on the stack.
    EltTys.clear();
    EltTys.push_back(stackEntryTy);
    // enough spots for each pointerType
    for (long i = 0; i < cell_count; i++) {
      EltTys.push_back(pointerType);
    }
    auto *concreteStackEntryTy = StructType::create(EltTys, ("alaska_stackentry." + F.getName()).str());

    IRBuilder<> atEntry(F.front().getFirstNonPHI());
    auto *stackEntry = atEntry.CreateAlloca(concreteStackEntryTy, nullptr, "lockStackFrame");
    atEntry.CreateStore(ConstantAggregateZero::get(concreteStackEntryTy), stackEntry, true);


    auto *EntryCountPtr = CreateGEP(M.getContext(), atEntry, stackEntryTy, stackEntry, 1, "lockStackFrame.count");
    // cur->count = N;
    atEntry.CreateStore(ConstantInt::get(i64Ty, cell_count), EntryCountPtr, true);
    // cur->prev = head;
    auto *CurrentHead = atEntry.CreateLoad(stackEntryTy->getPointerTo(), Head, "lockStackCurrentHead");
    atEntry.CreateStore(CurrentHead, stackEntry, true);
    // head = cur;
    atEntry.CreateStore(stackEntry, Head, true);

    int ind = 0;
    for (auto &lock : locks) {
      char buf[512];
      snprintf(buf, 512, "lockCell.%d", ind);
      auto cell = CreateGEP(M.getContext(), atEntry, concreteStackEntryTy, stackEntry, lock_cells[lock.get()] + 1, buf);
      ind++;

      auto handle = lock->getHandle();

      IRBuilder<> b(lock->lock);
      b.SetInsertPoint(lock->lock);
      b.CreateStore(handle, cell, true);

      // Insert a store right after all the unlocks to say "we're done with this handle".
      // for (auto unlock : lock->unlocks) {
      //   b.SetInsertPoint(unlock->getNextNode());
      //   b.CreateStore(llvm::ConstantPointerNull::get(dyn_cast<PointerType>(handle->getType())), cell, true);
      // }
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
