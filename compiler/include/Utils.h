#include "llvm/IR/Instruction.h"
#include <stdio.h>
#include <stdlib.h>
#include <Graph.h>
#include <config.h>

namespace alaska {

  llvm::Instruction *insertLockBefore(llvm::Instruction *inst, llvm::Value *pointer);
  llvm::Instruction *insertUnlockBefore(llvm::Instruction *inst, llvm::Value *pointer);


  // Insert get/puts for a graph conservatively (every load and store)
  void insertConservativeTranslations(alaska::PointerFlowGraph &G);
  void insertNaiveFlowBasedTranslations(alaska::PointerFlowGraph &G);

  void dumpBacktrace(void);

  // void runReplacementPass(llvm::Module &M);
  bool bootstrapping(void);  // are we bootstrapping?


  inline void fprint(llvm::raw_ostream &out) {}
  template <class T, class... Ts>
  inline void fprint(llvm::raw_ostream &out, T const &first, Ts const &...rest) {
    out << first;
    alaska::fprint(out, rest...);
  }



  template <class... Ts>
  inline void fprintln(llvm::raw_ostream &out, Ts const &...args) {
    alaska::fprint(out, args..., '\n');
  }



  template <class... Ts>
  inline void println(Ts const &...args) {
    alaska::fprintln(llvm::errs(), args...);
  }

  template <class... Ts>
  inline void print(Ts const &...args) {
    alaska::fprint(llvm::errs(), args...);
  }

  // format an llvm value in a simpler way
  std::string simpleFormat(llvm::Value *val);
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
