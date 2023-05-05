#include <alaska/Passes.h>
#include <alaska/Translations.h>
#include <alaska/Utils.h>
#include <alaska/WrappedFunctions.h>
#include <unistd.h>

using namespace llvm;

llvm::PreservedAnalyses AlaskaEscapePass::run(llvm::Module &M, llvm::ModuleAnalysisManager &AM) {
  std::set<std::string> functions_to_ignore = {
      "halloc",
      "hrealloc",
      "hrealloc_trace",
      "hcalloc",
      "hfree",
      "hfree_trace",
      "alaska_translate",
      "alaska_translate_trace",
      "alaska_release",
      "alaska_release_trace",
      "anchorage_manufacture_locality",
      "strstr",
      "strchr",
  };

  for (auto r : alaska::wrapped_functions) {
    functions_to_ignore.insert(r);
  }

  std::vector<llvm::CallInst *> escapes;
  std::set<llvm::Function *> externalFunctionEscapes;
  for (auto &F : M) {
    // if the function is defined (has a body) skip it
    if (!F.empty()) continue;
    if (functions_to_ignore.find(std::string(F.getName())) != functions_to_ignore.end()) continue;
    if (F.getName().startswith("llvm.lifetime")) continue;
    if (F.getName().startswith("llvm.dbg")) continue;
		// Don't escape to alaska functions
    if (F.getName().startswith("alaska_")) continue;

    for (auto user : F.users()) {
      if (auto call = dyn_cast<CallInst>(user)) {
        if (call->getCalledFunction() == &F) {
          escapes.push_back(call);
        }
      }
    }
  }

  for (auto *call : escapes) {
    int i = -1;
    for (auto &arg : call->args()) {
      i++;
      if (!alaska::shouldTranslate(arg)) continue;

      IRBuilder<> b(call);

      auto val = b.CreateGEP(arg->getType(), arg, {});
      auto translated = alaska::insertTranslationBefore(call, val);
      alaska::insertReleaseBefore(call->getNextNode(), val);
      call->setArgOperand(i, translated);
      externalFunctionEscapes.insert(call->getCalledFunction());
    }
  }


	// char filename[512];
	// sprintf(filename, "/tmp/alaska-escapes.%d", getpid());

	FILE *outf = fopen("/tmp/alaska-escapes", "a+");

  for (auto f : externalFunctionEscapes) {
    if (f != NULL) {
			fprintf(outf, "%s\n", f->getName().data());
		// alaska::println("potential escape when calling ", f->getName());
    }
  }

	fclose(outf);

  return PreservedAnalyses::none();
}
