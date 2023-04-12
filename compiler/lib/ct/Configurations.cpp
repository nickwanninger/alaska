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

#include <ct/Configurations.hpp>

// --------- Global Declarations ---------

// Metadata strings
const string CALLBACK_LOC = "cb.loc", UNROLL_MD = "unr.", MANUAL_MD = "man.", BIASED_MD = "bias.", TOP_MD = "top.",
             BOTTOM_MD = "bot.", BB_MD = "cb.bb", LOOP_MD = "loop.", FUNCTION_MD = "func.";

// Functions necessary to find in order to inject (hook_fire, fiber_start/create for fiber explicit injections)
// const vector<uint32_t> NKIDs = {HOOK_FIRE, FIBER_START, FIBER_CREATE, IDLE_FIBER_ROUTINE};
// const vector<string> NKNames = {"nk_time_hook_fire", "nk_fiber_start", "nk_fiber_create", "__nk_fiber_idle"};
const vector<uint32_t> NKIDs = {HOOK_FIRE};
const vector<string> NKNames = {"alaska_time_hook_fire"};

// No hook function names functionality --- (non-injectable)
const string ANNOTATION = "llvm.global.annotations", NOHOOK = "nohook";

// Globals
unordered_map<uint32_t, Function *> *SpecialRoutines = new unordered_map<uint32_t, Function *>();
vector<string> *NoHookFunctionSignatures = new vector<string>();
vector<Function *> *NoHookFunctions = new vector<Function *>();