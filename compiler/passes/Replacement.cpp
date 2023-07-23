#include <alaska/Passes.h>
#include <alaska/Translations.h>
#include <alaska/Utils.h>
#include <alaska/WrappedFunctions.h>

using namespace llvm;

// Take any calls to a function called `original_name` and
// replace them with a call to `new_name` instead.
static void replace_function(Module &M, std::string original_name, std::string new_name = "") {
  if (new_name == "") {
    new_name = "alaska_wrapped_" + original_name;
  }
  auto oldFunction = M.getFunction(original_name);
  if (oldFunction) {
    auto newFunction = M.getOrInsertFunction(new_name, oldFunction->getType()).getCallee();
    oldFunction->replaceAllUsesWith(newFunction);
    // oldFunction->eraseFromParent();
  }
  // delete oldFunction;
}


PreservedAnalyses AlaskaReplacementPass::run(Module &M, ModuleAnalysisManager &AM) {
  bool tracing = getenv("ALASKA_COMPILER_TRACE") != NULL;

  // replace
  if (tracing) {
    replace_function(M, "malloc", "halloc_trace");
    replace_function(M, "calloc", "hcalloc_trace");
    replace_function(M, "realloc", "hrealloc_trace");
    replace_function(M, "free", "hfree_trace");
    replace_function(M, "alaska_barrier", "alaska_barrier_trace");
  } else {
    if (getenv("ALASKA_NO_REPLACE_MALLOC") == NULL) {
      replace_function(M, "malloc", "halloc");
      replace_function(M, "calloc", "hcalloc");
      replace_function(M, "realloc", "hrealloc");

      replace_function(M, "malloc_beebs", "halloc"); // embench
      replace_function(M, "calloc_beebs", "hcalloc"); // embench
      replace_function(M, "realloc_beebs", "hrealloc"); // embench
    }

		// replace_function(M, "_ZdlPvm", "hfree");

    // even if calls to malloc are not replaced, we still ought to replace these functions for
    // compatability. Calling hfree() with a non-handle will fall back to the system's free() - same
    // for alaska_usable_size().
    replace_function(M, "free", "hfree");
    replace_function(M, "free_beebs", "hfree"); // embench
    replace_function(M, "malloc_usable_size", "alaska_usable_size");
  }

  for (auto *name : alaska::wrapped_functions) {
    replace_function(M, name);
  }
  return PreservedAnalyses::none();
}
