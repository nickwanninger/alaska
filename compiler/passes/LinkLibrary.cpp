#include <alaska/Passes.h>
#include <alaska/Translations.h>
#include <alaska/Utils.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <noelle/core/MetadataManager.hpp>
#include <llvm/Linker/Linker.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Transforms/IPO/Internalize.h>
#include <llvm/IR/DebugInfo.h>
// #include <llvm/Transforms/IPO/Internalize.h>

#include <llvm/Linker/IRMover.h>

using namespace llvm;


AlaskaLinkLibraryPass::AlaskaLinkLibraryPass(
    const char *lib_path, GlobalValue::LinkageTypes linkage)
    : lib_path(lib_path)
    , linkage(linkage) {
}


void AlaskaLinkLibraryPass::prepareLibrary(Module &M) {
  // for (auto &G : M.globals())
  //   if (!G.isDeclaration()) G.setLinkage(linkage);
	llvm::StripDebugInfo(M);

  for (auto &A : M.aliases())
    if (!A.isDeclaration()) A.setLinkage(linkage);

  for (auto &F : M) {
    if (!F.isDeclaration()) F.setLinkage(linkage);
    F.setLinkage(linkage);
  }
}


PreservedAnalyses AlaskaLinkLibraryPass::run(Module &M, ModuleAnalysisManager &AM) {
  auto buf = llvm::MemoryBuffer::getFile(lib_path);
  auto other = llvm::parseBitcodeFile(*buf.get(), M.getContext());
  auto other_module = std::move(other.get());

	if (other_module->materializeMetadata()) {
		exit(EXIT_FAILURE);
	}

  prepareLibrary(*other_module);

	// llvm::IRMover mv(*other_module);

  unsigned ApplicableFlags = Linker::Flags::OverrideFromSrc;
  llvm::Linker L(M);
  L.linkInModule(std::move(other_module), ApplicableFlags, [](Module &M, const StringSet<> &GVS) {
    llvm::internalizeModule(M, [&GVS](const GlobalValue &GV) {
      return !GV.hasName() || (GVS.count(GV.getName()) == 0);
    });
  });
  return PreservedAnalyses::none();
}
