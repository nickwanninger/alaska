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
 * Utils.hpp
 * ----------------------------------------
 *
 * All utility and debugging methods necessary for compiler-timing
 * transform. Split into two namespaces for clarity.
 */

#include <ct/Configurations.hpp>

using namespace llvm;
using namespace std;

namespace Utils {
  // Init
  void ExitOnInit();
  void GatherAnnotatedFunctions(GlobalVariable *GV, vector<Function *> &AF);

  // Basic function tracking/identification/analysis
  set<Function *> *IdentifyFiberRoutines();
  void IdentifyAllNKFunctions(Module &M, set<Function *> &Routines);
  bool DoesNotDirectlyRecurse(Function *F);

  // Basic transformations
  void InlineNKFunction(Function *F);
  void InjectCallback(set<Instruction *> &InjectionLocations, Function *F);

  // Metadata handling
  void SetCallbackMetadata(Instruction *I, const string MD);
  bool HasCallbackMetadata(Instruction *I);
  bool HasCallbackMetadata(Loop *L, set<Instruction *> &InstructionsWithCBMD);
  bool HasCallbackMetadata(Function *F, set<Instruction *> &InstructionsWithCBMD);
}  // namespace Utils

namespace Debug {
  void PrintFNames(vector<Function *> &Functions);
  void PrintFNames(set<Function *> &Functions);
  void PrintCurrentLoop(Loop *L);
  void PrintCurrentFunction(Function *F);
  void PrintCallbackLocations(set<Instruction *> &CallbackLocations);
}  // namespace Debug
