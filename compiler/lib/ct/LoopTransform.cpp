/*
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the
 * United States National  Science Foundation and the Department of Energy.
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2019, Souradip Ghosh <sgh@u.northwestern.edu>
 * Copyright (c) 2019, Simone Campanoni <simonec@eecs.northwestern.edu>
 * Copyright (c) 2019, Peter A. Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2019, The V3VEE Project  <http://www.v3vee.org>
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Souradip Ghosh <sgh@u.northwestern.edu>
 *          Simone Campanoni <simonec@eecs.northwestern.edu>
 *          Peter A. Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <ct/LoopTransform.hpp>

LoopTransform::LoopTransform(Loop *L, Function *F, LoopInfo *LI, DominatorTree *DT, ScalarEvolution *SE,
    TargetTransformInfo *TTI, AssumptionCache *AC, OptimizationRemarkEmitter *ORE, uint64_t Gran, Loop *OutL) {
  // Will assume wrapper pass info is valid

  if (L == nullptr || F == nullptr) abort();  // Serious

  if (L->getHeader() == nullptr) abort();  // Serious

  // Set state
  this->L = L;
  this->F = F;
  this->Granularity = Gran;
  this->OutL = OutL;

  // Set wrapper pass state
  this->LI = LI;
  this->DT = DT;
  this->SE = SE;
  this->AC = AC;
  this->ORE = ORE;

  // Set other data structures
  this->CallbackLocations = set<Instruction *>();

  // Handle and transform subloops first before transforming
  // the current loop --- necessary for LLVM constraints --- the
  // wrapper pass state must be updated before transforming an
  // outer loop
  _transformSubLoops();

  // Calculate loop level analysis (same-depth)
  this->LoopIDFA = new LatencyDFA(L, EXPECTED, MEDCON, true);
  this->LoopIDFA->ComputeDFA();
  this->LoopIDFA->PrintBBLatencies();
  this->LoopIDFA->PrintInstLatencies();
}


/*
 * _transformSubLoops
 *
 * In order to analyze and transform the current loop, all other subloops
 * must be handled first --- if depth-first transformation is ignored, the
 * transformations based on LLS, intervals, etc. will be inaccurate
 *
 * Create a LoopTransform object for each subloop --- recurse to the
 * innermost loop, and return --- save all callback locations from subloops
 * in the current LoopTransform object
 */

void LoopTransform::_transformSubLoops() {
  vector<Loop *> SubLoops = L->getSubLoopsVector();
  for (auto SL : SubLoops) {
    auto SLT = new LoopTransform(SL, F, LI, DT, SE, TTI, AC, ORE, GRAN, OutL);
    SLT->Transform();

    for (auto I : *(SLT->GetCallbackLocations()))
      CallbackLocations.insert(I);
  }

  return;
}


/*
 * Transform
 *
 * Main driver for loop transformations --- three step process
 *
 * 1) Calculate the loop extension stats/count --- this will determine how
 *    we are going to transform the loop and/or determine where callback
 *    locations will be placed. The loop transformation policy is set prior
 *    in the _calculateLoopExtensionStats method.
 *
 * 2) Perform the appropriate loop transformation based on the extension
 *    statistics --- either extend (i.e. unroll) the loop by the extension
 *    count (the callback location would be at the end of the unrolled loop),
 *    build a branch at the top of the loop body that will be taken at an
 *    interval determined by the extension count (the callback location would
 *    be at this branch), or manually place callback locations via interval
 *    analysis (via the LatencyDFA class)
 *
 * 3) Insert guards --- top guard and/or bottom guards for the loop. Top guards
 *    are placed at outermost loops (depth=1), and bottom guards are placed
 *    at loops of any depth --- NOTE --- biased branches place top guards within
 *    the BuildBiasedBranch branch method as it is much simpler
 */

void LoopTransform::Transform() {
  // Get LLS, LatencyDFA results, extension count statistics
  uint64_t LLS = (LoopIDFA->GetLoopLatencySize()), MarginOffset = 0,
           ExtensionCount = _calculateLoopExtensionStats(LLS, &MarginOffset);

  switch (Extend) {
    case TransformOption::EXTEND: {
      // Unroll the loop by an unroll factor = ExtensionCount, collect
      // all other callback locations that were generated as a byproduct
      // of the unrolling process --- callback locations are marked internally
      Instruction *Counter = nullptr;
      ExtendLoopResult ELP = ExtendLoop(ExtensionCount, Counter);

      switch (ELP) {
        case ELSuccess:  // Proceed with unrolled loop handling
        {
          _collectUnrolledCallbackLocations();
          _designateTopGuardViaPredecessors();
          // No bottom guard designated --- ExtendLoop marks the last instruction
          // of the loop's latch for injection --- that way, if a loop breaks out
          // of its execution, it will drop into the latch and execute the callback
          // set up in the latch either way --- BRANCH and MANUAL are not set up
          // in this way

          return;
        }
        case ELFail:  // Unrolling failed --- revert to biased branch handling
        {
          _designateBottomGuard(ExtensionCount, Counter);
          return;
        }
        case ELFull:  // No more loop --- no more handling
        default:
          return;
      }
    }
    case TransformOption::BRANCH: {
      // Inject code to build the iteration counter and the branch to
      // take based on the iteration count for the loop --- build at
      // the top of the loop --- callback locations are marked internally
      Instruction *InsertionPoint = L->getHeader()->getFirstNonPHI();
      Instruction *Counter = nullptr;
      BuildBiasedBranch(InsertionPoint, ExtensionCount, Counter);

      // No top guard explicitly designated --- done internally in
      // BuildBiasedBranch --- much simpler this way
      _designateBottomGuard(ExtensionCount, Counter);

      return;
    }
    case TransformOption::MANUAL: {
      // Find callback locations via interval analysis (using the LatencyDFA
      // object built for top-level/same-depth interval analysis)
      auto ManualCallbackLocations = LoopIDFA->BuildIntervalsFromZero();
      for (auto CL : *ManualCallbackLocations) {
        Utils::SetCallbackMetadata(CL, MANUAL_MD);
        CallbackLocations.insert(CL);
      }

      // Set guards
      _designateTopGuardViaPredecessors();
      _designateBottomGuard(ExtensionCount, nullptr);

      return;
    }
    default:
      abort();
  }
}


/*
 * _collectUnrolledCallbackLocations
 *
 * This is a hack to solve an issue generated from unrolling loops --- if
 * a loop is unrolled the original callback locations is the only instruction
 * that ends up being marked --- the duplicate locations generated as a
 * result of unrolling are missed. This method extracts those callback
 * locations by parsing the metadata of the unrolled loop
 */

void LoopTransform::_collectUnrolledCallbackLocations() {
  set<Instruction *> CBMDInstructions;

  // NOTE --- set union probably more proper, but also probably
  // much more expensive --- just insert
  if (Utils::HasCallbackMetadata(L, CBMDInstructions)) {
    for (auto I : CBMDInstructions)
      CallbackLocations.insert(I);
  }

  return;
}


/*
 * _buildCallbackBlock
 *
 * Builds the basic block that will contain an invocation of the callback
 * function --- taken with potential top or bottom guards, biased branches, etc.
 */

Instruction *LoopTransform::_buildCallbackBlock(CmpInst *CI, Instruction *InsertionPoint, const string MD) {
  // Create new basic block at the insertion point --- splits insertion point's
  // parent block
  Instruction *NewBlockTerminator = SplitBlockAndInsertIfThen(CI, InsertionPoint, false, nullptr, DT, LI, nullptr);

  BasicBlock *NewBlock = NewBlockTerminator->getParent();
  if (NewBlockTerminator == nullptr) return nullptr;

  // Get unique predecessor and successor for new block
  BasicBlock *NewSingleSucc = NewBlock->getUniqueSuccessor();
  BasicBlock *NewSinglePred = NewBlock->getUniquePredecessor();
  if (NewSinglePred == nullptr || NewSingleSucc == nullptr) return nullptr;

  // Set metadata, mark the branch instruction for the new block
  // as a callback location
  Utils::SetCallbackMetadata(NewBlockTerminator, MD);
  CallbackLocations.insert(NewBlockTerminator);

  return NewBlockTerminator;
}


/*
 * _buildIterator --- PARTIALLY DEPRECATED (only used for bottom guards
 * if bottom guards need to build a counter from scratch)
 *
 * Injections an interation counter into the loop --- using a PHINode that acts
 * as a pseudo-induction variable and an add instruction that acts as an iterator
 *
 * No optimization is performed here --- if an iteration counter in the form of
 * another PHINode already exists, we still create another one --- i.e. no
 * analysis is performed to reuse a potential iterator
 *
 */

void LoopTransform::_buildIterator(PHINode *&NewPHI, Instruction *&Iterator) {
  BasicBlock *Header = L->getHeader();
  Instruction *InsertionPoint = Header->getFirstNonPHI();
  if (InsertionPoint == nullptr) return;

  // Set up builders
  IRBuilder<> Builder{InsertionPoint};
  Type *IntTy = Type::getInt32Ty(F->getContext());

  // Build the PHINode that handles the iterator value on each
  // iteration of the loop, create the instruction to iterate
  Value *Increment = ConstantInt::get(IntTy, 1);
  PHINode *TopPHI = Builder.CreatePHI(IntTy, 0);
  Value *IteratorVal = Builder.CreateAdd(TopPHI, Increment);

  // Set values
  NewPHI = TopPHI;
  Iterator = dyn_cast<Instruction>(IteratorVal);

  return;
}


/*
 * _setIteratorPHI
 *
 * Set the incoming blocks and incoming values for the PHINode responsible
 * for the iteration scheme --- used in conjunction with _buildIterator
 */

void LoopTransform::_setIteratorPHI(Loop *LL, PHINode *ThePHI, Value *Init, Value *Iterator) {
  for (auto PredBB : predecessors(LL->getHeader())) {
    // Handle backedges
    if (LL->contains(PredBB))  // Shortcut --- for header
      ThePHI->addIncoming(Iterator, PredBB);
    else
      ThePHI->addIncoming(Init, PredBB);
  }

  return;
}


/*
 * _designateTopGuardViaPredecessors
 *
 * A guard is injected at the start of every outermost loop (depth=1)
 * in order to handle a potentially large miss in executions of
 * the callback function --- it's possible that we're just a tiny bit
 * early right before entering the loop --- however, since loop analysis
 * resets at accumulated latency=0, we could generate a large miss (up
 * to 2x). We want to prevent this by injecting a callback at the start
 * of the loop
 *
 * We do NOT inject in nested loops --- the tradeoff in preventing a
 * large miss is offset by the fact that each nested loop will be
 * executing the callback if there is a top guard in place in every
 * iteration --- which does not scale as the granularity increases.
 * In other words, the overhead of entering early increases with the
 * granularity because each nested loop has an invocation to the
 * callback every iteration no matter what
 *
 * Additionally, nested loops tend to be smaller, and an expansion
 * factor is applied when transforming nested loops
 *
 * Heuristic:
 *
 * Insert top guard --- we want to execute the callback right before the
 * terminator of each predecessor ONLY if its terminator is an unconditional
 * branch into the loop body.
 *
 * HIGH LEVEL:
 *
 * %1:                              %1:
 * ...                              ...
 * br (cond) %.loop                 tail call void callback() // Callback location
 *                                  br (cond) %.loop
 * %.loop
 * ...                              %.loop
 * ...                              ...
 * br ... %.loop ...                ...
 *                                  br ... %.loop ...
 * ...
 *                                  ...
 * %.other
 * ...                              %.other
 * ...                              ...
 *                                  ...
 *
 *
 * If it's a conditional branch --- we must inject a new basic block containing
 * the invocation to the callback because injecting the callback before the
 * conditional branch in the predecessor block itself may execute prematurely
 * if the execution jumps to a non-loop block
 *
 * HIGH LEVEL:
 *
 * %1:                              %1:
 * ...                              ...
 * br (cond) %.loop %.other         br (cond) %.topguard %.other
 *
 * %.loop                           %.topguard:
 * ...                              tail call void callback() // Callback location
 * ...                              br %.loop
 * br ... %.loop ...
 *                                  %.loop
 * ...                              ...
 *                                  ...
 * %.other                          br ... %.loop ...
 * ...
 * ...                              ...
 *
 *                                  %.other
 *                                  ...
 *                                  ...
 *
 * TODO --- develop a ratio between LLS for nested loops vs LLS for
 * outermost loop --- if the ratio is high, inject the top guard
 * in the innermost loop, else inject in the outermost loops
 *
 * TODO --- scale for depth > 2
 */

void LoopTransform::_designateTopGuardViaPredecessors() {
  // Remove top guards in recursive functions --- not handling
  // this case leads to a storm of callbacks at runtime
  if (!(Utils::DoesNotDirectlyRecurse(F))) return;

  // Handle at outermost loop (for now)
  if (L->getLoopDepth() != 1) return;

  // Acquire the header to the loop
  BasicBlock *Header = L->getHeader();

  // Find all predecessors of the header, determine if their terminators
  // match our criteria
  for (auto PredBB : predecessors(Header)) {
    // Factor out backedges, etc.
    if ((LoopIDFA->IsBackEdge(PredBB, Header)) || L->contains(PredBB)) continue;

    // Get terminator, sanity check
    Instruction *PredTerminator = PredBB->getTerminator();
    if (PredTerminator == nullptr) continue;

    // Determine if the terminator is an unconditional branch
    BranchInst *BrTerminator = dyn_cast<BranchInst>(PredTerminator);
    if (BrTerminator == nullptr) continue;

    if (BrTerminator->isConditional()) {
      CmpInst *Cond = dyn_cast<CmpInst>(BrTerminator->getCondition());
      if (Cond == nullptr) continue;

      Instruction *InsertionPoint = &(Header->front());
      Instruction *NewBlockTerminator = _buildCallbackBlock(Cond, InsertionPoint, TOP_MD);
      CallbackLocations.insert(NewBlockTerminator);
    } else {
      // Add this point to the CallbackLocations list
      Utils::SetCallbackMetadata(BrTerminator, TOP_MD);
      CallbackLocations.insert(BrTerminator);
    }
  }

  return;
}


/*
 * _designateBottomGuard, _buildBottomGuard
 *
 * A guard is injected at every exit block for a loop --- at any depth
 * to handle the case when we exit a loop, but we have not iterated
 * to the point where we're ready to execute the callback yet. However,
 * since interval calculations outside the loop start at accumulated
 * latency=0, there may be a large miss (up to 2x). We want to prevent
 * this from occurring via this guard that would execute if we reach
 * a particular threshold in the iteration scheme
 *
 * HIGH LEVEL:
 * Build an intermediate block for callback injections on each block
 * exit
 *
 * %2:                      %2:
 * ...                      ...
 * br %3                    br %3
 *
 * %3:                      %3:
 * ...                      ...
 * br (cond) %4 %6          br (cond) %4 %.ivcheck2
 *
 * %4:                      %4:
 * ...                      ...
 * br (IV) %2 %5            br (IV) %2 %.ivcheck1
 *
 * %5:                      %.ivcheck1: // requires a iteration couter
 * %a = ...                 br (iter geq threshold) %.guard1 %5
 * return %a
 *                          %.guard1:
 * %6:                      tail call void callback() // Callback location
 * %b = ...                 br %5
 * return %b
 *                          %5:
 *                          %a = ...
 *                          return %a
 *
 *                          %.ivcheck2:
 *                          br (iter geq threshold) %.guard2 %6
 *
 *                          %.guard2:
 *                          tail call void callback() // Callback location
 *                          br %6
 *
 *                          %6:
 *                          %b = ...
 *                          return %b
 *
 *
 * _designateBottomGuardViaExits sets the iteration scheme for each exit edge
 * that exists in the loop, and _buildBottomGuard injects the block with the
 * appropriate threshold checks, and/or marks the appropriate callback locations
 *
 */

void LoopTransform::_designateBottomGuard(uint64_t ExtensionCount, Instruction *Counter) {
  // Get exit blocks
  SmallVector<std::pair<BasicBlock *, BasicBlock *>, 8> ExitEdges;
  L->getExitEdges(ExitEdges);

  set<BasicBlock *> VisitedExits;

  for (auto EE : ExitEdges) {
    // Edge --- source, exit block
    BasicBlock *Source = EE.first, *Exit = EE.second;

    // Necessary to prevent exits from getting multiple
    // bottom guards even though the exit edges could
    // be different
    if (VisitedExits.find(Exit) != VisitedExits.end()) continue;

    VisitedExits.insert(Exit);

    Instruction *SourceTerminator = Source->getTerminator();
    if ((SourceTerminator == nullptr) || (!(isa<BranchInst>(SourceTerminator)))) continue;  // Sanity check

    // We need to build a counter if a counter is not
    // provided to this method --- i.e. branch injection
    // versus other policies
    if (Counter == nullptr) {
      PHINode *IteratorPHI = nullptr;
      Instruction *Iterator = nullptr;

      _buildIterator(IteratorPHI, Iterator);
      if ((IteratorPHI == nullptr) || (Iterator == nullptr)) continue;

      Type *IntTy = Type::getInt32Ty(F->getContext());
      _setIteratorPHI(L, IteratorPHI, ConstantInt::get(IntTy, 0), Iterator);
      _buildBottomGuard(Source, Exit, Iterator, ExtensionCount, false);
    } else {
      _buildBottomGuard(Source, Exit, Counter, ExtensionCount, true);
    }
  }

  return;
}


void LoopTransform::_buildBottomGuard(
    BasicBlock *Source, BasicBlock *Exit, Instruction *Iterator, uint64_t ExtensionCount, bool NeedToLoadCounter) {
  // Get terminator of the source block --- inside loop
  BranchInst *SourceTerminator = dyn_cast<BranchInst>(Source->getTerminator());
  if (SourceTerminator == nullptr) return;

  // If the source terminator is unconditional --- it's an isolated chain
  // in the CFG --- mark a callback location before the terminator and return
  if (SourceTerminator->isUnconditional()) {
    Utils::SetCallbackMetadata(SourceTerminator, BOTTOM_MD);
    CallbackLocations.insert(SourceTerminator);
    return;
  }

  // Now working with a conditional branch terminator --- get the compare
  // instruction from the source block --- for building the callback block
  CmpInst *SourceCond = dyn_cast<CmpInst>(SourceTerminator->getCondition());
  if (SourceCond == nullptr) return;

  // Build the callback block --- NOTE --- a byproduct of the BasicBlockUtils
  // API is that the unique successor to the callback block will be used to
  // inject the threshold check for the bottom guard
  Instruction *InsertionPoint = Exit->getFirstNonPHI(),
              *CallbackBlockTerminator = _buildCallbackBlock(SourceCond, InsertionPoint, BOTTOM_MD);
  if (CallbackBlockTerminator == nullptr) return;

  BasicBlock *CallbackBlock = CallbackBlockTerminator->getParent(),
             *ChecksBlock = CallbackBlock->getUniquePredecessor();  // Use this block to perform
                                                                    // the threshold checks


  // Build the threshold checks --- replace the branch condition of the
  // ChecksBlock with one that calculates if the iteration count is past
  // the threshold for this particular loop

  // Set up IRBuilder
  Instruction *ChecksTerminator = ChecksBlock->getTerminator();
  BranchInst *ChecksBranch = dyn_cast<BranchInst>(ChecksTerminator);
  if (ChecksBranch == nullptr) return;

  IRBuilder<> ChecksBuilder{ChecksBranch};
  Type *IntTy = Type::getInt32Ty(F->getContext());

  // Build the compare instruction --- use threshold = 0.8
  Value *Threshold = ConstantInt::get(IntTy, (ExtensionCount * 0.8));

  // Load instruction necessary if we're using rudimentary
  // branch injection counter
  Instruction *IteratorToCmp = nullptr;
  if (NeedToLoadCounter) {
    LoadInst *LoadCounter = ChecksBuilder.CreateLoad(Threshold->getType(), Iterator);
    LoadCounter->setAlignment(Align(4));
    IteratorToCmp = LoadCounter;
  } else {
    IteratorToCmp = Iterator;
  }

  Value *ThresholdCheck = ChecksBuilder.CreateICmp(ICmpInst::ICMP_SGT, IteratorToCmp, Threshold);

  // Change condition of checks branch instruction
  ChecksBranch->setCondition(ThresholdCheck);

  // Set metadata
  Utils::SetCallbackMetadata(ChecksBranch, BOTTOM_MD);

  return;
}


/* ----------------------------------------


/*
 * BuildBiasedBranch
 *
 * NOTE --- THIS IS A LAST MINUTE CHANGE
 *
 * Implementation of across loop counters via loads and stores and
 * use mem2reg to optimize
 *
 */

void LoopTransform::BuildBiasedBranch(Instruction *InsertionPoint, uint64_t ExtensionCount, Instruction *&Counter) {
  // Nothing to do if ExtensionCount is 0
  if (!ExtensionCount) return;

  if (InsertionPoint == nullptr) abort();  // Serious


  // CallbackLocations.insert(InsertionPoint);
  // return;

  // Set up init values
  Type *IntTy = Type::getInt32Ty(F->getContext());
  // auto *PtrTy = Type::getInt32PtrTy(F->getContext());

  Value *ZeroValue = ConstantInt::get(IntTy, 0);
  // Value *Alignment = ConstantInt::get(IntTy, 4);
  Value *ResetValue = ConstantInt::get(IntTy, ExtensionCount);
  Value *Increment = ConstantInt::get(IntTy, 1), *StoreInitValue = ZeroValue;

#if LOOP_GUARD
  // If we want to insert a loop guard, then we should set the init value
  // of the top level Phi node to ExtensionCount - 1, so the code subsequently
  // increments and executes the callback before any loop operations execute
  // NOTE --- we want to guard only outermost loops

  // This is much simpler than _designateTopGuardViaPredecessors, given that
  // this method can forge the guard via the PHINodes for the branch

  if ((L->getLoopDepth() == 1) && (Utils::DoesNotDirectlyRecurse(F)))
    StoreInitValue = ConstantInt::get(IntTy, ExtensionCount - 1);
#endif

  // Initialise counter at the top of the function body
  BasicBlock *FunctionEntry = &(F->getEntryBlock());
  Instruction *EntryInsertionPoint = FunctionEntry->getFirstNonPHI();
  IRBuilder<> EntryBuilder{EntryInsertionPoint};

  // Build alloca, store initial value
  AllocaInst *TheCounter = EntryBuilder.CreateAlloca(IntTy);
  TheCounter->setAlignment(Align(4));
  StoreInst *InitStore = EntryBuilder.CreateStore(StoreInitValue, TheCounter);
  InitStore->setAlignment(Align(4));

  // Create manual increment --- load the value, increment
  Instruction *IncrementInsertionPoint = L->getHeader()->getFirstNonPHI();
  IRBuilder<> IncrementBuilder{IncrementInsertionPoint};

  LoadInst *LoadCounter = IncrementBuilder.CreateLoad(TheCounter->getAllocatedType(), TheCounter);
  LoadCounter->setAlignment(Align(4));
  Value *Iterator = IncrementBuilder.CreateAdd(LoadCounter, Increment);
  CmpInst *CI = static_cast<CmpInst *>(IncrementBuilder.CreateICmp(ICmpInst::ICMP_EQ, Iterator, ResetValue));

  // Generate new block (for loop for injection of nk_time_hook_fire)
  Instruction *NewBlockTerminator = _buildCallbackBlock(CI, InsertionPoint, (BIASED_MD + to_string(L->getLoopDepth())));
  if (NewBlockTerminator == nullptr) return;

  // Get call block, unique predecessor and successor for new block
  BasicBlock *NewBlock = NewBlockTerminator->getParent();
  BasicBlock *NewSingleSucc = NewBlock->getUniqueSuccessor();
  BasicBlock *NewSinglePred = NewBlock->getUniquePredecessor();

  // Generate new builder on insertion point in the successor of the new block and
  // generate second level phi node (SecondaryPHI)
  Instruction *SecondaryInsertionPoint = NewSingleSucc->getFirstNonPHI();
  if (SecondaryInsertionPoint == nullptr) return;

  IRBuilder<> SecondaryPHIBuilder{SecondaryInsertionPoint};
  PHINode *SecondaryPHI = SecondaryPHIBuilder.CreatePHI(IntTy, 0);

  // Populate selection points for SecondaryPHI
  SecondaryPHI->addIncoming(Iterator, NewSinglePred);
  SecondaryPHI->addIncoming(ZeroValue, NewBlock);

  // Store value back to counter
  StoreInst *Restore = SecondaryPHIBuilder.CreateStore(SecondaryPHI, TheCounter);
  Restore->setAlignment(Align(4));

  // Set counter
  Counter = TheCounter;

  // Mark callback location
  CallbackLocations.insert(NewBlockTerminator);

  return;
}


/*
 * ExtendLoop
 *
 * Unroll the loop by an unroll factor = ExtensionCount --- this unroll
 * factor is predetermined by _calculateLoopExtensionStats. Unroll factors
 * are deliberately kept small to reduce compilation time (which can be
 * quite high when nested loops are unrolled by a large factor), reduce
 * code bloat, and reduce the possibility of high instruction cache miss
 * rate.
 *
 * If unrolling is unsuccessful for some reason, we resort to building the
 * branch, so the transformation is still accounted for --- this should
 * not occur often at all, but this fallback mechanism is in place
 */

ExtendLoopResult LoopTransform::ExtendLoop(uint64_t ExtensionCount, Instruction *&Counter) {
  // Nothing to unroll if ExtensionCount is 0
  if (!ExtensionCount) return ExtendLoopResult::ELFull;  // Transform handler simply returns

  if (LoopIDFA->GetLoopInstructionCount() > MaxLoopSize) {
    LOOP_DEBUG_INFO("F: " + F->getName() + "\n");
    LOOP_OBJ_INFO(L);
    LOOP_DEBUG_INFO("Not Unrolled (size), Count: ");
    LOOP_DEBUG_INFO(to_string(LoopIDFA->GetLoopInstructionCount()));

    Debug::PrintCurrentLoop(L);
    LOOP_DEBUG_INFO("\n\n\n");

    // Build a branch to handle large loop size
    BuildBiasedBranch(L->getHeader()->getFirstNonPHI(), ExtensionCount, Counter);

    return ExtendLoopResult::ELFail;
  }

  // Unroll iterations of the loop based on ExtensionCount
  // uint32_t MinTripMultiple = 1;
  UnrollLoopOptions *ULO = new UnrollLoopOptions();
  ULO->Count = ExtensionCount;
  // ULO->TripCount = SE->getSmallConstantTripCount(L);
  ULO->Force = true;
  ULO->Runtime = true;
  ULO->AllowExpensiveTripCount = true;
  // ULO->PreserveCondBr = true;
  // ULO->PreserveOnlyFirst = false;
  // ULO->TripMultiple = max(MinTripMultiple, SE->getSmallConstantTripMultiple(L));
  // ULO->PeelCount = 0;
  ULO->UnrollRemainder = false;
  ULO->ForgetAllSCEV = false;

  LoopUnrollResult Unrolled = UnrollLoop(L, *ULO, LI, SE, DT, AC, TTI, ORE, true);

  if (!((Unrolled == LoopUnrollResult::FullyUnrolled) || (Unrolled == LoopUnrollResult::PartiallyUnrolled))) {
    LOOP_DEBUG_INFO("F: " + F->getName() + "\n");
    LOOP_OBJ_INFO(L);
    LOOP_DEBUG_INFO("Not Unrolled (failure): ");
    Debug::PrintCurrentLoop(L);
    LOOP_DEBUG_INFO("\n\n\n");

    // Build a branch to handle the unroll failure
    BuildBiasedBranch(L->getHeader()->getFirstNonPHI(), ExtensionCount, Counter);

    return ExtendLoopResult::ELFail;
  }

  if (Unrolled == LoopUnrollResult::FullyUnrolled) {
    LOOP_DEBUG_INFO("F: " + F->getName() + "\n");
    LOOP_DEBUG_INFO("Fully unrolled --- loop no longer available\n");
    LOOP_DEBUG_INFO("\n\n\n");
    return ExtendLoopResult::ELFull;
  }

  // Collect the callback locations and add them to the set --- should be
  // the terminator of the last
  Instruction *LatchTerminator = L->getLoopLatch()->getTerminator();
  Utils::SetCallbackMetadata(LatchTerminator, (UNROLL_MD + to_string(ExtensionCount)));
  CallbackLocations.insert(LatchTerminator);

  Debug::PrintCurrentLoop(L);

  return ExtendLoopResult::ELSuccess;
}


/*
 * _calculateLoopExtensionStats
 *
 * Based on the LatencyDFA object (for entire loop analysis), we calculate
 * the transformation policy based on several metrics --- comparing to the
 * granularity, margin of error from the granularity
 *
 * Calculates the extension count necessary to transform with some assumptions
 * (including vectorization, heuristics for margin of error, etc.)
 *
 * Details are described in the method.
 *
 * NOTE --- MarginOffset functionality is deprecated
 */

uint64_t LoopTransform::_calculateLoopExtensionStats(uint64_t LLS, uint64_t *MarginOffset) {
  if (!LLS) return 0;

  // Calculate the extension count --- i.e. how much we should
  // unroll the loop or how many iterations should pass before
  // taking the biased branch

  // If LLS is larger than the granularity, we must designate
  // callback locations manually but NOT actually perform
  // any transformations to the loop --- for injection purposes,
  // a callback MUST be injected no matter the accumulated
  // latency entering the loop body (mathematically speaking)
  if (LLS > Granularity) {
    Extend = TransformOption::MANUAL;
    return 0;
  }

  double GranDouble = (double)Granularity;

  // Find margin of error
  double Exact = GranDouble / LLS;
  uint64_t Rounded = Granularity / LLS;
  double MarginOfError = (Exact - Rounded) * LLS;  // Number of cycles

  // If we exceed the maximum margin of errorm, then we must extend
  // the loop in some way until LLS > Granularity
  uint64_t ExtensionCount = 0;
  if (MarginOfError > MaxMargin) {
    ExtensionCount = Rounded;                   // + 1; // Rounded; --- FIX
    *MarginOffset = (uint64_t)(MarginOfError);  // Only matters for biased branching
  } else {
    Rounded++;
    ExtensionCount = Rounded;
  }

  /*
   * Below is an attempt at weighting nested loops --- nested loops tend to
   * be smaller or a quicker set of operations, possibly more prone to
   * vectorization, so miscalculating on a nested loop will generate
   * consistently late or early calls to the callback function (i.e. the
   * effect will compound through the loop depths at runtime) --- we want
   * to mitigate for that by moving from overestimating the LLS of a nested
   * loop to something closer to the ground truth
   */
  if (_canVectorizeLoop()) ExtensionCount *= (L->getLoopDepth() * ExpansionFactor);  // INACCURACY --- FIX

  // Check if we should extend or inject a biased branch --- we
  // need to set up a biased branch if we are past the max extension
  // count --- Otherwise, if the extension count is low enough
  // (after incrementing to account for the margin --- i.e. rounding
  // up), return that extension count (Rounded++)
  Extend = (ExtensionCount > MaxExtensionCount) ? TransformOption::BRANCH : TransformOption::EXTEND;

  return ExtensionCount;
}
