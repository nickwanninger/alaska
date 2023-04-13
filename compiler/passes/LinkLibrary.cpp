#include <Passes.h>
#include <Locks.h>
#include <Utils.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <noelle/core/MetadataManager.hpp>
#include <llvm/Linker/Linker.h>
#include <llvm/Bitcode/BitcodeReader.h>


using namespace llvm;


AlaskaLinkLibraryPass::AlaskaLinkLibraryPass(const char *lib_path, GlobalValue::LinkageTypes linkage)
    : lib_path(lib_path)
    , linkage(linkage) {
}


void AlaskaLinkLibraryPass::prepareLibrary(Module &M) {
  for (auto &G : M.globals())
    if (!G.isDeclaration()) G.setLinkage(linkage);

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

  prepareLibrary(*other_module);
  llvm::Linker::linkModules(M, std::move(other_module));
  return PreservedAnalyses::none();
}