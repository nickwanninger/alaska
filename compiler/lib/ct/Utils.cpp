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

#include <ct/Utils.hpp>

using namespace Utils;
using namespace Debug;

// ----------------------------------------------------------------------------------

// Utils --- Init

/*
 * ExitOnInit
 *
 * Register pass, execute doInitialization method but do not perform
 * any analysis or transformation --- exit in runOnModule --- mostly
 * for testing scenarios
 *
 */

void Utils::ExitOnInit() {
  if (FALSE) exit(0);
}


/*
 * GatherAnnotatedFunctions
 *
 * A user can mark a function in Nautilus with a function attribute to
 * prevent injections into the function. For example, it would be unwise
 * to inject calls to nk_time_hook_fire into nk_time_hook_fire itself. Users
 * should use the annotate attribute with the string "nohook" in order
 * to prevent injections.
 *
 * This method parses the global annotations array in the resulting bitcode
 * and collects all functions that have been annotated with the "nohook"
 * attribute. These functions will not have injections.
 *
 */

void Utils::GatherAnnotatedFunctions(GlobalVariable *GV, vector<Function *> &AF) {
  // First operand is the global annotations array --- get and parse
  // NOTE --- the fields have to be accessed through VALUE->getOperand(0),
  // which appears to be a layer of indirection for these values
  auto *AnnotatedArr = cast<ConstantArray>(GV->getOperand(0));

  for (auto OP = AnnotatedArr->operands().begin(); OP != AnnotatedArr->operands().end(); OP++) {
    // Each element in the annotations array is a ConstantStruct
    // There are two fields --- Function *, GlobalVariable * (function ptr, annotation)

    auto *AnnotatedStruct = cast<ConstantStruct>(OP);
    auto *FunctionAsStructOp = AnnotatedStruct->getOperand(0);          // first field
    auto *GlobalAnnotationAsStructOp = AnnotatedStruct->getOperand(1);  // second field

    // Set the function and global, respectively. Both have to exist to
    // be considered.
    Function *AnnotatedF = dyn_cast<Function>(FunctionAsStructOp);
    GlobalVariable *AnnotatedGV = dyn_cast<GlobalVariable>(GlobalAnnotationAsStructOp);

    if (AnnotatedF == nullptr || AnnotatedGV == nullptr) continue;

    // Check the annotation --- if it matches the NOHOOK global in the
    // pass --- push back to apply (X) transform as necessary later
    ConstantDataArray *ConstStrArr = dyn_cast<ConstantDataArray>(AnnotatedGV->getOperand(0));
    if (ConstStrArr == nullptr) continue;

    if (ConstStrArr->getAsCString() != NOHOOK) continue;

    AF.push_back(AnnotatedF);
  }

  Debug::PrintFNames(AF);

  return;
}


// ----------------------------------------------------------------------------------

// Utils --- Function tracking/identification

/*
 * IdentifyFiberRoutines
 *
 * Given a module (in this case the entire Nautilus kernel), this
 * method searches for all fiber routines registered explicitly
 * in the kernel via the arguments in nk_fiber_create, the fiber
 * "registration" method in Nautilus. Returns a pointer to a
 * set of these routines.
 *
 */

set<Function *> *Utils::IdentifyFiberRoutines() {
  set<Function *> *Routines = new set<Function *>();

  // Iterate over uses of nk_fiber_create
  for (auto &FiberCreateUse : SpecialRoutines->at(FIBER_CREATE)->uses()) {
    User *FiberCreateUser = FiberCreateUse.getUser();
    if (auto *CallToRoutine = dyn_cast<CallInst>(FiberCreateUser)) {
      // First arg of nk_fiber_create is a function pointer
      // to an explicitly registered fiber routine
      auto FunctionPtr = CallToRoutine->getArgOperand(0);
      if (auto *Routine = dyn_cast<Function>(FunctionPtr)) Routines->insert(Routine);
    }
  }

  return Routines;
}


/*
 * IdentifyAllNKFunctions
 *
 * Given a module (in this case the entire Nautilus kernel), this
 * method finds all valid functions represented in the bitcode for
 * injection purposes. This includes all functions that are not LLVM
 * intrinsics, contain function bodies, aren't nullptr, not in the
 * NoHookFunctions set and not nk_time_hook_fire. Adds all functions
 * to a passed set.
 *
 */

void Utils::IdentifyAllNKFunctions(Module &M, set<Function *> &Routines) {
  // Iterate through all functions in the kernel
  for (auto &F : M) {
    // Check function for conditions
    if ((F.isIntrinsic()) || (&F == SpecialRoutines->at(HOOK_FIRE)) ||
        (find(NoHookFunctions->begin(), NoHookFunctions->end(), &F) != NoHookFunctions->end()) ||
        (!(F.getInstructionCount() > 0)))
      continue;

    DEBUG_INFO(F.getName() + ": InstructionCount --- " + to_string(F.getInstructionCount()) + "; ReturnType --- ");
    OBJ_INFO((F.getReturnType()));

    // Add pointer to set if all conditions check out
    Routines.insert(&F);
  }

  return;
}


/*
 * DoesNotDirectlyRecurse
 *
 * Recognize if a function is directly recursive by examining the call
 * instructions in the function --- this replaces the doesNotRecurse
 * method from LLVM, which checks for the existance of the NoRecurse
 * function attribute
 *
 */

bool Utils::DoesNotDirectlyRecurse(Function *F) {
  bool Found = true;

  for (auto &B : *F) {
    for (auto &I : B) {
      if (auto *CI = dyn_cast<CallInst>(&I)) {
        Function *Callee = CI->getCalledFunction();
        if (Callee == F) {
          Found &= false;
          break;
        }
      }
    }

    if (!Found) break;
  }

  return Found;
}


// ----------------------------------------------------------------------------------

// Utils --- Transformations

/*
 * InlineNKFunction
 *
 * Inline a given function (if it passes the sanity checks) by
 * iterating over all uses and inlining each call invocation
 * to that function --- standard
 *
 */

void Utils::InlineNKFunction(Function *F) {
  // If F doesn't check out, don't inline
  if ((F == nullptr) || (F->isIntrinsic()) || (!(F->getInstructionCount() > 0))) return;

  // Set up inlining structures
  vector<CallInst *> CIToIterate;
  InlineFunctionInfo IFI;

  DEBUG_INFO("Current Function: " + F->getName() + "\n");

  // Iterate over all uses of F
  for (auto &FUse : F->uses()) {
    User *FUser = FUse.getUser();

    DEBUG_INFO("Current User: ");
    OBJ_INFO(FUser);

    // Collect all uses that are call instructions
    // that reference F
    if (auto *FCall = dyn_cast<CallInst>(FUser)) {
      DEBUG_INFO("    Found a CallInst\n");
      CIToIterate.push_back(FCall);
    }

    DEBUG_INFO("\n");
  }

  // Iterate through all call instructions referencing
  // F and inline all invocations --- if any of them fail,
  // abort (serious)
  for (auto CI : CIToIterate) {
    DEBUG_INFO("Current CI: ");
    OBJ_INFO(CI);

    auto Inlined = InlineFunction(*CI, IFI);
    if (!Inlined.isSuccess()) {
      DEBUG_INFO("INLINE FAILED --- ");
      DEBUG_INFO(Inlined.getFailureReason());
      DEBUG_INFO("\n");
      abort();  // Serious
    }
  }

  return;
}


/*
 * InjectCallback
 *
 * Given a set of instructions that act as "injection locations,"
 * inject callbacks to the callback function (nk_time_hook_fire) at
 * each of these locations --- debug information is set accordingly
 * as well
 *
 */

void Utils::InjectCallback(set<Instruction *> &InjectionLocations, Function *F) {
  // Call instructions for the callback function need to be injected with
  // relatively "correct" debug locations --- Clang 9 will complain otherwise
  Instruction *FirstInstWithDBG = nullptr;
  for (auto &I : instructions(F)) {
    if (I.getDebugLoc()) {
      FirstInstWithDBG = &I;
      break;
    }
  }

  // Inject callback invocations at all locations specified in the set
  for (auto IL : InjectionLocations) {
    DEBUG_INFO("Current IL: ");
    OBJ_INFO(IL);

    // Inject callback invocation with correct debug locations
    IRBuilder<> Builder{IL};
    if (FirstInstWithDBG != nullptr) Builder.SetCurrentDebugLocation(FirstInstWithDBG->getDebugLoc());

    CallInst *CBInvocation = Builder.CreateCall((*SpecialRoutines)[HOOK_FIRE], None);

    // Sanity check
    if (CBInvocation == nullptr) {
      DEBUG_INFO("---Injection failed---\n");
      abort();
    }
  }

  return;
}

// ----------------------------------------------------------------------------------

// Utils --- Metadata

/*
 * SetCallbackMetadata
 *
 * Sets callback location metadata for an instruction --- each instruction is
 * set with metadata kind = "cb.loc" and a specified metadata that describes
 * the policy used to determine the callback location (kind = MD)
 */

void Utils::SetCallbackMetadata(Instruction *I, const string MD) {
  // Build metadata node
  MDNode *TheNode = MDNode::get(I->getContext(), MDString::get(I->getContext(), "callback"));

  // Set callback location metadata
  I->setMetadata(CALLBACK_LOC, TheNode);

  // Set specific metadata
  I->setMetadata(MD, TheNode);

  return;
}


/*
 * HasCallbackMetadata
 *
 * - Parses metadata for an instruction --- queries for "cb.loc" metadata
 * - Parses metadata for a loop --- collects the instructions that contain
 *   metadata kind = "cb.loc"
 * - Parses metadata for a function --- collects the instructions that
 *   contain metadata kind = "cb.loc"
 */

bool Utils::HasCallbackMetadata(Instruction *I) {
  MDNode *CBMD = I->getMetadata(CALLBACK_LOC);
  if (CBMD == nullptr) return false;

  return true;
}

bool Utils::HasCallbackMetadata(Loop *L, set<Instruction *> &InstructionsWithCBMD) {
  for (auto B = L->block_begin(); B != L->block_end(); ++B) {
    BasicBlock *CurrBB = *B;
    for (auto &I : *CurrBB) {
      if (Utils::HasCallbackMetadata(&I)) InstructionsWithCBMD.insert(&I);
    }
  }

  if (InstructionsWithCBMD.empty()) return false;

  return true;
}

bool Utils::HasCallbackMetadata(Function *F, set<Instruction *> &InstructionsWithCBMD) {
  for (auto &B : *F) {
    for (auto &I : B) {
      if (Utils::HasCallbackMetadata(&I)) InstructionsWithCBMD.insert(&I);
    }
  }

  if (InstructionsWithCBMD.empty()) return false;

  return true;
}


// ----------------------------------------------------------------------------------

// Debug

void Debug::PrintFNames(vector<Function *> &Functions) {
  for (auto F : Functions) {
    if (F == nullptr) {
      DEBUG_INFO("F is nullptr\n");
      continue;
    }

    DEBUG_INFO(F->getName() + "\n");
  }


  return;
}

void Debug::PrintFNames(set<Function *> &Functions) {
  for (auto F : Functions) {
    if (F == nullptr) {
      DEBUG_INFO("F is nullptr\n");
      continue;
    }

    DEBUG_INFO(F->getName() + "\n");
  }

  return;
}

void Debug::PrintCurrentLoop(Loop *L) {
  DEBUG_INFO("Current Loop:\n");
  if (L == nullptr) {
    DEBUG_INFO("Nullptr Loop");
    DEBUG_INFO("\n\n\n---\n\n\n");
    return;
  }

  OBJ_INFO(L);

  for (auto B = L->block_begin(); B != L->block_end(); ++B) {
    BasicBlock *CurrBB = *B;
    OBJ_INFO(CurrBB);
  }

  DEBUG_INFO("\n\n\n---\n\n\n");

  return;
}

void Debug::PrintCurrentFunction(Function *F) {
  DEBUG_INFO("Current Function:\n");
  if (F == nullptr) {
    DEBUG_INFO("Nullptr Function");
    DEBUG_INFO("\n\n\n---\n\n\n");
    return;
  }

  OBJ_INFO(F);
  DEBUG_INFO("\n\n\n---\n\n\n");

  return;
}

void Debug::PrintCallbackLocations(set<Instruction *> &CallbackLocations) {
  DEBUG_INFO("Callback Locations\n");
  for (auto CL : CallbackLocations)
    OBJ_INFO(CL);

  DEBUG_INFO("\n\n\n---\n\n\n");

  return;
}
