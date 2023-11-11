#include <alaska/Passes.h>
#include <alaska/Translations.h>
#include <alaska/Utils.h>

using namespace llvm;

PreservedAnalyses TranslationPrinterPass::run(Module &M, ModuleAnalysisManager &AM) {
  std::set<std::string> focus_on;
  std::string focus;


#ifdef ALASKA_DUMP_TRANSLATIONS_FOCUS
  focus = ALASKA_DUMP_TRANSLATIONS_FOCUS;
#endif

  size_t pos = 0, found;
  while ((found = focus.find(",", pos)) != std::string::npos) {
    focus_on.insert(focus.substr(pos, found - pos));
    pos = found + 1;
  }
  focus_on.insert(focus.substr(pos));

  for (auto &F : M) {
    if (focus.size() == 0 || (focus_on.find(std::string(F.getName())) != focus_on.end())) {
      if (F.empty()) continue;
      errs() << F.getName() << "\n";
      auto l = alaska::extractTranslations(F);
      if (l.size() > 0) {
        alaska::printTranslationDot(F, l);
      }
    }
  }
  return PreservedAnalyses::all();
}
