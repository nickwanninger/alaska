#include <ct/Pass.hpp>
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/PassManager.h"
#include <ct/Utils.hpp>
#include <ct/Configurations.hpp>
#include <ct/LatencyDFA.hpp>
#include <ct/LoopTransform.hpp>

using namespace llvm;


void CompilerTimingPass::init(llvm::Module &M) {
  Utils::ExitOnInit();

  auto fty = FunctionType::get(llvm::Type::getVoidTy(M.getContext()), {});
  M.getOrInsertFunction("alaska_time_hook_fire", fty);


  /*
   * Find all functions with special names (NKNames) and apply
   * NoInline attributes --- preserve for analysis and transformation
   */
  for (auto Name : NKNames) {
    auto F = M.getFunction(Name);
    if (F == nullptr) continue;

    F->addFnAttr(Attribute::NoInline);
  }


  /*
   * Find all functions with special "nohook" attributes and add
   * NoInline attributes to those functions as well --- prevent indirect
   * injections that may occur via inlining
   */
  GlobalVariable *GV = M.getGlobalVariable(ANNOTATION);
  if (GV == nullptr) return;

  vector<Function *> AnnotatedFunctions;
  Utils::GatherAnnotatedFunctions(GV, AnnotatedFunctions);
  for (auto AF : AnnotatedFunctions) {
    if (AF == nullptr) continue;

    AF->addFnAttr(Attribute::NoInline);
    NoHookFunctionSignatures->push_back(AF->getName().str());
  }
}

PreservedAnalyses CompilerTimingPass::run(Module &M, ModuleAnalysisManager &AM) {
  this->init(M);

  /*
   * Find all functions necessary for analysis and transformation. If
   * at least one function is missing, abort.
   */
  for (auto ID : NKIDs) {
    auto F = M.getFunction(NKNames[ID]);
    if (F == nullptr) {
      DEBUG_INFO("Nullptr special routine: " + NKNames[ID] + "\n");
      abort();  // Serious
    }

    (*SpecialRoutines)[ID] = F;
  }


  /*
   * Populate NoHookFunctions --- these functions are specified so no
   * calls to nk_time_hook_fire are injected into these.
   */
  for (auto Name : *NoHookFunctionSignatures) {
    auto F = M.getFunction(Name);
    if (F == nullptr) continue;

    NoHookFunctions->push_back(F);
  }


#if INJECT

  /*
   * TOP LEVEL ---
   * Perform analysis and transformation on fiber routines. These routines
   * are the most specific interface for compiler-based timing. Fiber routines
   * are always analyzed and transformed.
   */

  /*
   * Force inlining of nk_fiber_start to generate direct calls to nk_fiber_create
   * (nk_fiber_start calls nk_fiber_create internally) --- we can extract all
   * fiber routines in the kernel by following the arguments of nk_fiber_create.
   */
  // Utils::InlineNKFunction((*SpecialRoutines)[FIBER_START]);
  // set<Function *> Routines = *(Utils::IdentifyFiberRoutines());

#if WHOLE

  /*
   * If whole kernel injection/optimization is turned on, we want to inject
   * callbacks to nk_time_hook_fire across the entire kernel module. In order
   * to do so, we want to identify all valid functions in the bitcode and mark
   * them (add them in the Routines set)
   */
  set<Function *> Routines;

  Utils::IdentifyAllNKFunctions(M, Routines);

#endif

#endif

  auto dl = DataLayout(&M);

  llvm::TargetLibraryInfoWrapperPass tliwp;  // ugh.
  /*
   * Iterate through all the routines collected in the kernel, and perform all
   * DFA, interval analysis, loop and function transformations, etc. --- calls
   * will be injected per function
   */

  for (auto Routine : Routines) {
    // Set injection locations data structure
    set<Instruction *> InjectionLocations;


    // Gather wrapper analysis state

    // auto DT = DominatorTreeAnalysis(*Routine).get
    // auto *DT = &getAnalysis<DominatorTreeWrapperPass>(*Routine).getDomTree();
    // auto *AC = &getAnalysis<AssumptionCacheTracker>().getAssumptionCache(*Routine);
    // auto *LI = &getAnalysis<LoopInfoWrapperPass>(*Routine).getLoopInfo();
    // auto *SE = &getAnalysis<ScalarEvolutionWrapperPass>(*Routine).getSE();
    // auto *TTI = &getAnalysis<TargetTransformInfoWrapperPass>().getTTI(*Routine);
    OptimizationRemarkEmitter ORE(Routine);



    llvm::DominatorTree DT(*Routine);
    llvm::LoopInfo LI(DT);
    llvm::TargetTransformInfo TTI(dl);
    llvm::AssumptionCache AC(*Routine, &TTI);
    auto &TLI = tliwp.getTLI(*Routine);

    ScalarEvolution SE(*Routine, TLI, AC, DT, LI);


    // Gather loops
    vector<Loop *> Loops;
    for (auto L : LI)
      Loops.push_back(L);


    // Execute loop analysis and transformations, collect any callback
    // locations generated
    for (auto L : Loops) {
      auto LT = new LoopTransform(L, Routine, &LI, &DT, &SE, &TTI, &AC, &ORE, GRAN, L);
      LT->Transform();

      for (auto I : *(LT->GetCallbackLocations()))
        InjectionLocations.insert(I);
    }


    // Execute function level analysis and transformation, collect any
    // callback locations generated
    LatencyDFA *FunctionIDFA = new LatencyDFA(Routine, &LI, EXPECTED, MEDCON, true);
    FunctionIDFA->ComputeDFA();
    auto FCallbackLocations = FunctionIDFA->BuildIntervalsFromZero();

    for (auto I : *FCallbackLocations)
      InjectionLocations.insert(I);


    // Inject callbacks at all specified injection locations
    Utils::InjectCallback(InjectionLocations, Routine);


    // Verify the function hasn't broken LLVM invariants --- if
    // it has, print to standard error
    if (verifyFunction(*Routine, &(errs()))) {
      VERIFY_DEBUG_INFO("\n");
      VERIFY_DEBUG_INFO(Routine->getName() + "\n");
      VERIFY_DEBUG_INFO("\n\n\n");
    }
  }


#if INLINE

  /*
   * Inline the callback function --- nk_time_hook_fire, option for
   * a potential optimization.
   *
   * (NOTE --- this is potentially very dangerous at low granularities
   * --- code bloat and explosion of instrisic debug info causes a
   * shift of the .text section at link time and potential linker
   * failure, also may cause performance bottleneck at low granularities
   * (instruction cache thrashing))
   */
  Utils::InlineNKFunction((*SpecialRoutines)[HOOK_FIRE]);

#endif

  return PreservedAnalyses::all();
}
