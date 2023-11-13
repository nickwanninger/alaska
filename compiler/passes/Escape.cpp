#include <alaska/Passes.h>
#include <alaska/Translations.h>
#include <alaska/Utils.h>
#include <alaska/WrappedFunctions.h>
#include <unistd.h>

using namespace llvm;


struct FunctionEscapeInfo {
  bool isEscape = false;
  bool escape_varargs = false;  // Do you escape var args that are passed?
  std::set<int> args;           // which arguments must be escaped?
};



static bool mightBlock(llvm::Function &F) {
  // This is a big list of library functions that may end up blocking in the kernel or
  std::set<StringRef> blocking_whitelist = {
      // "printf",
      "epoll_create",
      "close",
      "epoll_ctl",
      "epoll_wait",
      "poll",
      "getsockopt",
      "fcntl64",
      "setsockopt",
      "getaddrinfo",
      "inet_ntop",
      "freeaddrinfo",
      "socket",
      "bind",
      "connect",
      "listen",
      "chmod",
      "accept4",
      "getpeername",
      "getsockname",
      "pipe2",
      "pipe",
      "fopen64",
      "fprintf",
      "fflush",
      "fclose",
      "syslog",
      "open64",
      "write",
      "time",
      "_exit",
      "waitpid",
      "exit",
      "fdatasync",
      "unlink",
      "flock",
      "sysconf",
      "access",
      "usleep",
      "execve",
      "read",
      "getrlimit64",
      "setrlimit64",
      "fgets",
      "signal",
      "openlog",
      "setlocale",
      "sigemptyset",
      "sigaction",
      "uname",
      "getrusage",
      "prctl",
      "fork",
      "setsid",
      "dup2",
      "fileno",
      "isatty",
      "getenv",
      "raise",
      "unsetenv",
      "tzset",
      "srand",
      "srandom",
      "umask",
      "__assert_fail",
      "abort",
      "perror",
      "pthread_mutex_lock",
      "pthread_mutex_unlock",
      "pthread_setname_np",
      "pthread_mutex_init",
      "pthread_create",
      "pthread_cancel",
      "pthread_join",
      "fwrite",
      "fread",
      "stat64",
      "mkdir",
      "opendir",
      "readdir64",
      "closedir",
      "fstat64",
      "rmdir",
      "dirname",
      "strtoul",
      "sleep",
      "ftruncate64",
      "sync_file_range",
      "fsync",
      "rename",
      "lstat64",
      "lseek64",
      "kill",
      "chdir",
      "glob64",
      "globfree64",
      "mkostemp64",
      "fchmod",
      "feof",
      "atoll",
      "fseek",
      "ftello64",
      "truncate64",
      "mmap64",
      "nanosleep",
      "dladdr",
      "setitimer",
      "htons",
      "inet_pton",
      "abs",
      "pthread_cond_init",
      "pthread_attr_init",
      "pthread_attr_getstacksize",
      "pthread_attr_setstacksize",
      "sigaddset",
      "pthread_sigmask",
      "pthread_cond_wait",
      "pthread_cond_signal",
      "ioctl",
      "clock_gettime",
      "clearenv",
      "setenv",
      "strncasecmp",
      "pthread_mutex_trylock",
      "dlopen",
      "dlerror",
      "dlsym",
      "dlclose",
      "putchar",
      "shutdown",
      "sched_setaffinity",
      "recv",
      "send",
      "fcntl",
      "longjmp",
      "_setjmp",
      "fopen",
      "getc",
      "freopen",
      "ungetc",
      "ferror",
      "fputc",
      "fputs",
      "vfprintf",
      "dcgettext",
  };


  auto name = F.getName();
  if (blocking_whitelist.find(name) != blocking_whitelist.end()) return true;

  return false;
}

llvm::PreservedAnalyses AlaskaEscapePass::run(llvm::Module &M, llvm::ModuleAnalysisManager &AM) {
  LLVMContext &ctx = M.getContext();
  auto barrierBoundType = FunctionType::get(Type::getVoidTy(ctx), {}, false);
  auto barrierEscapeStart =
      M.getOrInsertFunction("alaska_barrier_before_escape", barrierBoundType).getCallee();
  auto barrierEscapeEnd =
      M.getOrInsertFunction("alaska_barrier_after_escape", barrierBoundType).getCallee();

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

      "llvm.stackrestore",

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
    if (F.getName().startswith("llvm.smax")) ignore = true;
    if (F.getName().startswith("llvm.smin")) ignore = true;
    // if (F.getName().startswith("llvm.memcpy")) ignore = true;
    // if (F.getName().startswith("llvm.memset")) ignore = true;
    if (F.getName().startswith("llvm.experimental.noalias")) ignore = true;

    // Openmp stuff :)
    // if (F.getName().startswith("__kmp")) ignore = true;

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
      info.isEscape = true;

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

      if (F.empty()) {
        F.addFnAttr("alaska_escape");
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


  std::set<llvm::Function *> blockingFunctions;
  std::set<llvm::CallInst *> blockingSites;

  for (auto &F : M) {
    if (F.empty()) {
      if (mightBlock(F)) {
        F.addFnAttr("alaska_mightblock");
        // alaska::println("\e[31m\"", F.getName(), "\",\e[0m");
        blockingFunctions.insert(&F);
      } else {
        // alaska::println("\"", F.getName(), "\",");
      }
      continue;
    }



    for (auto &I : llvm::instructions(F)) {
      if (auto *call = dyn_cast<CallInst>(&I)) {
        if (auto func = dyn_cast<llvm::Function>(call->getCalledOperand())) {
          if (mightBlock(*func)) {
            blockingSites.insert(call);
          }
        }
        int i = -1;
        for (auto &arg : call->args()) {
          i++;
          if (!alaska::shouldTranslate(arg)) continue;

          if (!shouldLockArgument(call->getCalledOperand(), i)) {
            continue;
          }


          IRBuilder<> b(call);

          // auto val = b.CreateGEP(arg->getType(), arg, {});
          auto val = alaska::insertRootBefore(call, arg);
          auto translated = alaska::insertTranslationBefore(call, val);
          alaska::insertReleaseBefore(call->getNextNode(), arg);
          call->setArgOperand(i, translated);
        }
      }
    }
  }


  for (auto *call : blockingSites) {
    // Now that we have the arguments all translated, let's insert
    // logic to handle barriers - because they are poll based, we
    // will not be able to poll while in an external function.
    // TODO: filter out obvious external functions here.
    // For example, memcpy should not get this treatment, as it
    // will eventually return. fwrite should be handled as it
    // may never return!
    IRBuilder<> b(call);
    // alaska::println(*call);
    b.CreateCall(barrierBoundType, barrierEscapeStart, {});
    b.SetInsertPoint(call->getNextNode());
    b.CreateCall(barrierBoundType, barrierEscapeEnd, {});
  }

  // for (auto f : blockingFunctions) {
  //   if (f != NULL) {
  //     if (auto *func = dyn_cast<llvm::Function>(f)) {
  //       alaska::println("\e[33mWARNING\e[0m: escape handle to function ", func->getName());
  //     }
  //   }
  // }

  // for (auto &F : M) {
  //   if (F.getName() == "main") {
  //     alaska::println(F);
  //   }
  // }

  return PreservedAnalyses::none();
}
