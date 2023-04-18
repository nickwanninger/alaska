#include <Passes.h>
#include <Locks.h>
#include <Utils.h>

using namespace llvm;

PreservedAnalyses LockPrinterPass::run(Module &M, ModuleAnalysisManager &AM) {
  std::set<std::string> focus_on;
  std::string focus;


#ifdef ALASKA_DUMP_LOCKS_FOCUS
  focus = ALASKA_DUMP_LOCKS_FOCUS;
#endif

  size_t pos = 0, found;
  while ((found = focus.find(",", pos)) != std::string::npos) {
    focus_on.insert(focus.substr(pos, found - pos));
    pos = found + 1;
  }
  focus_on.insert(focus.substr(pos));

  for (auto &F : M) {
    if (focus.size() == 0 || (focus_on.find(std::string(F.getName())) != focus_on.end())) {
      auto l = alaska::extractLocks(F);
      if (l.size() > 0) {
        alaska::printLockDot(F, l);
      }
    }
  }
  return PreservedAnalyses::all();
}