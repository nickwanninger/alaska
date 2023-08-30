#include <alaska/Passes.h>
#include <alaska/Translations.h>
#include <alaska/Utils.h>
#include <alaska/WrappedFunctions.h>
#include <unistd.h>

using namespace llvm;


struct FunctionEscapeInfo {
  bool escape_varargs = false;  // Do you escape var args that are passed?
  std::set<int> args;           // which arguments must be escaped?
};



llvm::PreservedAnalyses AlaskaEscapePass::run(llvm::Module &M, llvm::ModuleAnalysisManager &AM) {
  std::set<std::string> functions_to_ignore = {
      "__alaska_leak",

      "halloc",
      "hrealloc",
      "hrealloc_trace",
      "hcalloc",
      "hfree",
      "hfree_trace",

      // Don't pre-translate
      "alaska_usable_size",
      // Anchorage stuff
      "anchorage_manufacture_locality",
      // "intrinsics"
      "alaska.root",
      "alaska.translate",
      "alaska.release",
      // Gross functions in libc that we handle ourselves
      "strstr",
      "strchr",

      // Redis hacks
      "bioCreateLazyFreeJob",


			// stub/net.c hacks
			"dlsym",
  };

  for (auto r : alaska::wrapped_functions) {
    functions_to_ignore.insert(r);
  }

  // Which things should be escaped?
  std::set<llvm::Function *> calleesToEscape;

  for (auto &F : M) {
    bool ignore = false;
    // if the function is defined (has a body) skip it
    if (!F.empty()) continue;
    // Ignore calls to alaska functions
    if (F.getName().startswith("alaska_")) ignore = true;
    if (F.getName().startswith("alaska.")) ignore = true;
    // Intriniscs
    if (F.getName().startswith("llvm.lifetime")) ignore = true;
    if (F.getName().startswith("llvm.va_start")) ignore = true;
    if (F.getName().startswith("llvm.va_end")) ignore = true;
    if (F.getName().startswith("llvm.dbg")) ignore = true;

    if (ignore) functions_to_ignore.insert(std::string(F.getName()));
  }

  // Given a function, which arguments should be escapes?
  // This exists to memoize the analysis we would do after checking
  // if a value is a pointer or not below
  std::map<llvm::Function *, FunctionEscapeInfo> escapeInfo;

  auto *va_start_func = M.getFunction("llvm.va_start");

  for (auto &F : M) {
    FunctionEscapeInfo info;

    if (functions_to_ignore.find(std::string(F.getName())) == functions_to_ignore.end()) {
      // Handle vararg functions a bit specially.
      if (F.isVarArg()) {
        // If the function is empty, we must escape varargs
        if (F.empty()) {
          info.escape_varargs = true;
        } else {
          // Otherwise, we need to do some snooping, as a va_list could escape somewhere else.
          // The way we do this is *dumb*. If a function which has varargs calls "va_start", we
          // conservativley say that the arguments will escape somewhere else. *va_args suck*
          if (va_start_func != NULL) {
            for (auto user : va_start_func->users()) {
              if (auto inst = dyn_cast<llvm::Instruction>(user)) {
                if (inst->getFunction() == &F) {
                  info.escape_varargs = true;
                  break;
                }
              }
            }
          }
        }
      }

      for (auto &arg : F.args()) {
        int no = arg.getArgNo();
        if (F.empty()) {
          info.args.insert(no);
          continue;
        }
      }
    }

    escapeInfo[&F] = std::move(info);
  }

  // for (auto &[func, info] : escapeInfo) {
  //   alaska::println("escape info for ", func->getName());
  //   if (func->isVarArg()) alaska::println("   is vararg");
  //   if (info.escape_varargs) alaska::println("   escape va_args");
  //
  //   alaska::print("   args:");
  //   for (auto arg : info.args) {
  //     alaska::print(" ", arg);
  //   }
  //   alaska::println();
  // }

  auto shouldLockArgument = [&](llvm::Value *callee, size_t ind) {
    if (auto func = dyn_cast<llvm::Function>(callee)) {
      auto &info = escapeInfo[func];
      // If the argument is explicitly marked as escaping, escape
      if (info.args.find(ind) != info.args.end()) {
        return true;
      }
      // If the function is vararg and is marked as "escape_vararg", escape those arguments
      if (func->isVarArg()) {
        if (ind >= func->arg_size()) {
          return info.escape_varargs;
        }
      }

      return false;
    }
    // TODO:
    return false;
  };


  std::set<llvm::Value *> externalFunctionEscapes;

  for (auto &F : M) {
    if (F.empty()) continue;

    for (auto &I : llvm::instructions(F)) {
      if (auto *call = dyn_cast<CallInst>(&I)) {
        // auto called = call->getCalledOperand();
        // if (auto func = dyn_cast<llvm::Function>(called)) {
        //   if (calleesToEscape.count(func) == 0) {
        //     continue;
        //   }
        // }
        // alaska::println("escaping", *call);

        int i = -1;
        for (auto &arg : call->args()) {
          i++;
          if (!alaska::shouldTranslate(arg)) continue;

          if (!shouldLockArgument(call->getCalledOperand(), i)) {
            continue;
          }


          externalFunctionEscapes.insert(call->getCalledOperand());

          IRBuilder<> b(call);

          // auto val = b.CreateGEP(arg->getType(), arg, {});
          auto val = alaska::insertRootBefore(call, arg);
          auto translated = alaska::insertTranslationBefore(call, val);
          alaska::insertReleaseBefore(call->getNextNode(), arg);
          call->setArgOperand(i, translated);
        }
        // alaska::println("   after", *call);
      }
    }
  }

  // for (auto f : externalFunctionEscapes) {
  //   if (f != NULL) {
  //     if (auto *func = dyn_cast<llvm::Function>(f)) {
  //       alaska::println("\e[33mWARNING\e[0m: escape handle to function ", func->getName());
  //     }
  //   }
  // }

  return PreservedAnalyses::none();
}
