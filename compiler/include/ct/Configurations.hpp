#pragma once
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

/*
 * Configurations.hpp
 * ----------------------------------------
 *
 * Store all includes, macros, settings/configurations,
 * globals, and potential inputs necessary for compiler
 * timing transform.
 *
 */

#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Verifier.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SparseBitVector.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Utils/UnrollLoop.h"
#include "llvm/Transforms/Utils/LoopSimplify.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Transforms/Vectorize/LoopVectorizationLegality.h"
#include "llvm/Analysis/TargetTransformInfo.h"

#include <vector>
#include <set>
#include <unordered_map>
#include <queue>
#include <algorithm>
#include <cstdlib>

using namespace llvm;
using namespace std;

// Pass settings
#define DEBUG 0
#define LOOP_DEBUG 0
#define VERIFY 0
#define INLINE 1
#define WHOLE 1  // Whole kernel injection
#define INJECT 1
#define FALSE 0

// Special Routines --- necessary for functionality
#define HOOK_FIRE 0
#define FIBER_START 1
#define FIBER_CREATE 2
#define IDLE_FIBER_ROUTINE 3

// Policies
#define GRAN 5000
#define LOOP_GUARD 1

#define MAXIMUM 0
#define EXPECTED 1

#define HIGHCON 0
#define MEDCON 1
#define LOWCON 2

#define CONSERV MEDCON
#define LATCONFIG EXPECTED

// Debugging
#define DEBUG_INFO(str) \
  do {                  \
    if (DEBUG) {        \
      errs() << str;    \
    }                   \
  } while (0)
#define LOOP_DEBUG_INFO(str) \
  do {                       \
    if (LOOP_DEBUG) {        \
      errs() << str;         \
    }                        \
  } while (0)
#define OBJ_INFO(obj)     \
  do {                    \
    if (DEBUG) {          \
      obj->print(errs()); \
      errs() << "\n";     \
    }                     \
  } while (0)
#define LOOP_OBJ_INFO(obj) \
  do {                     \
    if (LOOP_DEBUG) {      \
      obj->print(errs());  \
      errs() << "\n";      \
    }                      \
  } while (0)
#define VERIFY_DEBUG_INFO(str) \
  do {                         \
    if (VERIFY) {              \
      errs() << str;           \
    }                          \
  } while (0)
#define VERIFY_OBJ_INFO(obj) \
  do {                       \
    if (VERIFY) {            \
      obj->print(errs());    \
      errs() << "\n";        \
    }                        \
  } while (0)

// Metadata strings
extern const string CALLBACK_LOC;
extern const string UNROLL_MD;
extern const string MANUAL_MD;
extern const string BIASED_MD;
extern const string TOP_MD;
extern const string BOTTOM_MD;
extern const string BB_MD;
extern const string LOOP_MD;
extern const string FUNCTION_MD;

// Functions necessary to find in order to inject (hook_fire, fiber_start/create for fiber explicit injections)
extern const vector<uint32_t> NKIDs;
extern const vector<string> NKNames;

// No hook function names functionality --- (non-injectable)
extern const string ANNOTATION;
extern const string NOHOOK;

// Globals
extern unordered_map<uint32_t, Function *> *SpecialRoutines;
extern vector<string> *NoHookFunctionSignatures;
extern vector<Function *> *NoHookFunctions;
