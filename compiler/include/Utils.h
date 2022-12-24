#include "llvm/IR/Instruction.h"
#include <stdio.h>
#include <stdlib.h>
#include <Graph.h>

namespace alaska {

	llvm::Instruction *insertLockBefore(llvm::Instruction *inst, llvm::Value *pointer);
  enum InsertionType { Lock, Unlock };

  // Split the basic block before the instruction and insert a guarded
  // call to lock or unlock, based on if the `handle` is a handle or not
  llvm::Instruction *insertGuardedRTCall(
      InsertionType type, llvm::Value *handle, llvm::Instruction *inst, llvm::DebugLoc dbg);


  // Insert get/puts for a graph conservatively (every load and store)
  void insertConservativeTranslations(alaska::PointerFlowGraph &G);
  void insertNaiveFlowBasedTranslations(alaska::PointerFlowGraph &G);

  inline void println() {
#ifdef ALASKA_DEBUG
    // base case
    llvm::errs() << "\n";
#endif
  }

  template <class T, class... Ts>
  inline void println(T const &first, Ts const &...rest) {
#ifdef ALASKA_DEBUG
    llvm::errs() << first;
    alaska::println(rest...);
#endif
  }
}  // namespace alaska


#define ALASKA_SANITY(c, msg, ...)                                                              \
  do {                                                                                          \
    if (!(c)) { /* if the check is not true... */                                               \
      fprintf(stderr, "\x1b[31m-----------[ Alaska Sanity Check Failed ]-----------\x1b[0m\n"); \
      fprintf(stderr, "%s line %d\n", __FILE__, __LINE__);                                      \
      fprintf(stderr, "Check, `%s`, failed\n", #c, ##__VA_ARGS__);                              \
      fprintf(stderr, msg "\n", ##__VA_ARGS__);                                                 \
      fprintf(stderr, "\x1b[31m----------------------------------------------------\x1b[0m\n"); \
      exit(EXIT_FAILURE);                                                                       \
    }                                                                                           \
  } while (0);
