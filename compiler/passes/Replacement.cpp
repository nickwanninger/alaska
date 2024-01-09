#include <alaska/Passes.h>
#include <alaska/Translations.h>
#include <alaska/Utils.h>
#include <alaska/WrappedFunctions.h>

#include "llvm/IR/Verifier.h"

using namespace llvm;

// Some functions should be blacklisted from having allocation
static bool is_allocation_blacklisted(const llvm::StringRef &name) {
  // XALAN
  if (name == "_ZN11xalanc_1_1025XalanMemoryManagerDefault8allocateEm") return true;


  // LEELA
  if (name == "_ZN3GTP7executeER9GameStateNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE")
    return true;
  if (name ==
      "_ZNSt15__new_allocatorINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEE8allocateEmPKv")
    return true;
  if (name ==
      "_ZNSt15__new_allocatorIN5boost6tuples5tupleIiiP7UCTNodeNS1_9null_typeES5_S5_S5_S5_S5_S5_"
      "EEE8allocateEmPKv")
    return true;


  if (getenv("ALASKA_SPECIAL_CASE_GCC")) {
    alaska::println("SPECIAL CASE? ", name);
    if (name == "alloc_page") {
      alaska::println("XXX SPECIAL CASE GCC page_alloc XXX\n");
      return true;
    }
  }
  return false;
}

// Take any calls to a function called `original_name` and
// replace them with a call to `new_name` instead.
static void replace_function(
    Module &M, std::string original_name, std::string new_name = "", bool is_allocator = false) {
  if (new_name == "") {
    new_name = "alaska_wrapped_" + original_name;
  }
  auto oldFunction = M.getFunction(original_name);


  if (oldFunction) {
    auto newFunction = M.getOrInsertFunction(new_name, oldFunction->getFunctionType()).getCallee();
    // oldFunction->replaceAllUsesWith(newFunction);
    oldFunction->replaceUsesWithIf(newFunction, [&](llvm::Use &use) -> bool {
      if (is_allocator) {
        auto user = use.getUser();
        if (auto inst = dyn_cast<Instruction>(user)) {
          auto callingFunc = inst->getFunction();
          // alaska::println("Allocation replacement in ", callingFunc->getName());
          if (is_allocation_blacklisted(callingFunc->getName())) {
            alaska::println("Not replacing ", original_name, " in ", callingFunc->getName(),
                " because it is blackisted");
            return false;
          }
        }
      }
      return true;
    });
  }
}


PreservedAnalyses AlaskaReplacementPass::run(Module &M, ModuleAnalysisManager &AM) {
#ifdef ALASKA_REPLACE_MALLOC
  if (getenv("ALASKA_NO_REPLACE_MALLOC") == NULL) {
    if (getenv("ALASKA_SPECIAL_CASE_GCC") != NULL) {
      // I hate GCC. I hate this (FIXME)
      replace_function(M, "xmalloc", "malloc", false);
      replace_function(M, "xcalloc", "calloc", false);
      replace_function(M, "xrealloc", "realloc", false);


      alaska::println("XXX SPECIAL CASE GCC XXX\n");
      replace_function(M, "xmalloc", "halloc", true);
      replace_function(M, "xcalloc", "hcalloc", true);
      replace_function(M, "xrealloc", "hrealloc", true);
    }

    replace_function(M, "malloc", "halloc", true);
    replace_function(M, "calloc", "hcalloc", true);
    replace_function(M, "realloc", "hrealloc", true);

    replace_function(M, "malloc_beebs", "halloc", true);     // embench
    replace_function(M, "calloc_beebs", "hcalloc", true);    // embench
    replace_function(M, "realloc_beebs", "hrealloc", true);  // embench
  }
#endif

  // even if calls to malloc are not replaced, we still ought to replace these functions for
  // compatability. Calling hfree() with a non-handle will fall back to the system's free() - same
  // for alaska_usable_size().
  replace_function(M, "free", "hfree");
  replace_function(M, "free_beebs", "hfree");  // embench
  replace_function(M, "malloc_usable_size", "alaska_usable_size");

  for (auto *name : alaska::wrapped_functions) {
    replace_function(M, name);
  }

  for (auto &F : M) {
    if (llvm::verifyFunction(F, &errs())) {
      errs() << "Function verification failed!\n";
      errs() << F.getName() << "\n";
      auto l = alaska::extractTranslations(F);
      if (l.size() > 0) {
        alaska::printTranslationDot(F, l);
      }
      // errs() << F << "\n";
      exit(EXIT_FAILURE);
    }
  }
  // exit(0);
  return PreservedAnalyses::none();
}
