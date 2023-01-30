#include "llvm/IR/Instruction.h"
#include <stdio.h>
#include <stdlib.h>
#include <Graph.h>

namespace alaska {

  llvm::Instruction *insertLockBefore(llvm::Instruction *inst, llvm::Value *pointer);
  void insertUnlockBefore(llvm::Instruction *inst, llvm::Value *pointer);


  // Insert get/puts for a graph conservatively (every load and store)
  void insertConservativeTranslations(alaska::PointerFlowGraph &G);
  void insertNaiveFlowBasedTranslations(alaska::PointerFlowGraph &G);

  void dumpBacktrace(void);


  void runReplacementPass(llvm::Module &M);

  inline void println() {
    // base case
    llvm::errs() << "\n";
  }

  template <class T, class... Ts>
  inline void println(T const &first, Ts const &...rest) {
    llvm::errs() << first;
    alaska::println(rest...);
  }

  inline void print() { /* Base case. */ }
  template <class T, class... Ts>
  inline void print(T const &first, Ts const &...rest) {
    llvm::errs() << first;
    alaska::print(rest...);
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
