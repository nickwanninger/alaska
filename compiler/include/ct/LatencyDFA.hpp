
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
 * LatencyDFA.hpp
 * ----------------------------------------
 *
 * Driver class for the data-flow analysis required to determine
 * accumulated latency calculations for instructions and basic blocks
 * for each function in the kernel, and for the interval analysis
 * required to determine injection locations for a specified callback
 * function
 *
 */

#include <ct/LatencyTable.hpp>

using namespace std;
using namespace llvm;

class LatencyDFA {
 public:
  LatencyDFA(Function *F, LoopInfo *LI, int32_t PropPolicy, int32_t ConservPolicy, bool TopLevelAnalysis);
  LatencyDFA(Loop *L, int32_t PropPolicy, int32_t ConservPolicy, bool TopLevelAnalysis);

  // ------- Analysis -------
  void ComputeDFA();
  set<Instruction *> *BuildIntervalsFromZero();
  uint64_t GetLoopLatencySize() {
    return this->LoopLatencySize;
  }
  uint64_t GetLoopInstructionCount() {
    return this->LoopInstructionCount;
  }
  double GetAccumulatedLatency(Instruction *I);
  double GetAccumulatedLatency(BasicBlock *BB);
  double GetIndividualLatency(Instruction *I);
  double GetIndividualLatency(BasicBlock *BB);

  template <typename ChecksFunc, typename SetupFunc, typename WorkerFunc>
  void WorkListDriver(ChecksFunc &CF, SetupFunc &SF, WorkerFunc &WF);

  // ------- Debugging -------
  void PrintBBLatencies();
  void PrintInstLatencies();

  // ------- Utility ------
  bool IsBackEdge(BasicBlock *From, BasicBlock *To);
  bool IsContainedInSubLoop(BasicBlock *BB);
  bool IsContainedInLoop(BasicBlock *BB);

  // Public for simplicity
  vector<BasicBlock *> Blocks;

 private:
  Function *F;
  Loop *L;
  LoopInfo *LI;

  // ------- Analysis settings, statistics -------
  int32_t PropagationPolicy;
  int32_t ConservativenessPolicy;
  uint64_t LoopInstructionCount;
  uint64_t LoopLatencySize;
  bool TopLevelAnalysis;
  BasicBlock *EntryBlock;

  // ------- Analysis data structures -------
  // NOTE --- BitSets not necessary here, as:
  // a) these are simple DFA calculations with no complex set operations,
  // b) IDs is too complicated for an indirection here, not necessary where
  //    a direct instruction to latency mapping is necessary
  unordered_map<BasicBlock *, set<BasicBlock *>> BackEdges;  // SOURCE to set of DESTs
  vector<Loop *> Loops;                                      // Function analysis
  unordered_map<Instruction *, double> GEN;
  unordered_map<Instruction *, double> IN;
  unordered_map<Instruction *, double> OUT;  // NOTE --- these are the actual per-inst accumulated latencies
  unordered_map<BasicBlock *, double> IndividualLatencies;
  unordered_map<BasicBlock *, double> AccumulatedLatencies;
  unordered_map<BasicBlock *, double> LastCallbackLatencies;
  unordered_map<BasicBlock *, unordered_map<BasicBlock *, double> *> PredLatencyGraph;
  unordered_map<BasicBlock *, unordered_map<BasicBlock *, double> *> LastCallbackLatencyGraph;
  set<Instruction *> CallbackLocations;

  // ------- DFA -------

  // GEN set
  void _buildGEN();

  // IN and OUT set
  void _buildIndividualLatencies();
  void _buildINAndOUT();                                                     // Wrapper for IN/OUT set computation
  bool _DFAChecks(BasicBlock *CurrBlock, BasicBlock *PredBB);                // Checks for WorkListDriver
  void _buildPredLatencyGraph(BasicBlock *BB, vector<BasicBlock *> &Preds);  // Setup for WorkListDriver
  void _modifyINAndOUT(BasicBlock *BB);                                      // Computation for IN/OUT sets

  // ------- Interval/Callbacks Calculation -------
  bool _intervalBlockChecks(BasicBlock *CurrBlock, BasicBlock *PredBB);        // Loop level analysis, checks
  void _buildLastCallbacksGraph(BasicBlock *BB, vector<BasicBlock *> &Preds);  // Setup for WorkListDriver
  void _markCallbackLocations(BasicBlock *BB);                                 // Determine callback injection locations

  // ------- Policies -------
  double _configLatencyCalculation(vector<double> &PL);            // Enforcing policy
  double _configConservativenessCalculation(vector<double> &PL);   // Enforcing policy
  double _calculateEntryPointLatency(BasicBlock *CurrBlock);       // Application of policy
  double _calculateEntryPointLastCallback(BasicBlock *CurrBlock);  // Application of policy
  void _calculateLoopLatencySize();

  // ------- Utility -------
  void _organizeFunctionBackedges();
  bool _isValidBlock(BasicBlock *BB);
  void _setupDFASets();
  uint64_t _buildLoopBlocks(Loop *L);
};