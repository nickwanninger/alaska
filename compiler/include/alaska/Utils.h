#pragma once

#include "llvm/IR/Instruction.h"
#include <stdio.h>
#include <stdlib.h>
#include <alaska/PointerFlowGraph.h>
#include <config.h>

namespace alaska {



  llvm::Instruction *insertRootBefore(llvm::Instruction *inst, llvm::Value *pointer);
  llvm::Instruction *insertTranslationBefore(llvm::Instruction *inst, llvm::Value *pointer);
  llvm::Instruction *insertReleaseBefore(llvm::Instruction *inst, llvm::Value *pointer);
  llvm::Instruction *insertDerivedBefore(
      llvm::Instruction *inst, llvm::Value *base, llvm::GetElementPtrInst *offset);



  void dumpBacktrace(void);

  // void runReplacementPass(llvm::Module &M);
  bool bootstrapping(void);  // are we bootstrapping?


  inline void fprint(llvm::raw_ostream &out) {
  }
  template <class T, class... Ts>
  inline void fprint(llvm::raw_ostream &out, T const &first, Ts const &... rest) {
    out << first;
    alaska::fprint(out, rest...);
  }



  template <class... Ts>
  inline void fprintln(llvm::raw_ostream &out, Ts const &... args) {
    alaska::fprint(out, args..., '\n');
  }



  template <class... Ts>
  inline void println(Ts const &... args) {
    alaska::fprintln(llvm::errs(), args...);
  }

  template <class... Ts>
  inline void print(Ts const &... args) {
    alaska::fprint(llvm::errs(), args...);
  }

  // format an llvm value in a simpler way
  std::string simpleFormat(llvm::Value *val);


  inline uint64_t timestamp() {
    struct timespec spec;
    clock_gettime(1, &spec);
    return spec.tv_sec * (1000 * 1000 * 1000) + spec.tv_nsec;
  }

  inline double time_ms() {
		return (double)timestamp() / 1000.0 / 1000.0;
  }
}  // namespace alaska


#define ALASKA_SANITY(c, msg, ...)                                                              \
  do {                                                                                          \
    if (!(c)) { /* if the check is not true... */                                               \
      fprintf(stderr, "\x1b[31m-----------[ Alaska Sanity Check Failed ]-----------\x1b[0m\n"); \
      fprintf(stderr, "%s line %d\n", __FILE__, __LINE__);                                      \
      fprintf(stderr, "Check, `%s`, failed\n", #c, ##__VA_ARGS__);                              \
      fprintf(stderr, msg "\n", ##__VA_ARGS__);                                                 \
      alaska::dumpBacktrace();                                                                  \
      fprintf(stderr, "\x1b[31m----------------------------------------------------\x1b[0m\n"); \
      exit(EXIT_FAILURE);                                                                       \
    }                                                                                           \
  } while (0);
