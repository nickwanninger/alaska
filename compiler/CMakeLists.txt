
include_directories(
	${LLVM_INCLUDE_DIRS}
	include/
)
install(DIRECTORY ldscripts DESTINATION share/alaska)


add_llvm_library(Alaska MODULE
	passes/Alaska.cpp

	lib/PointerFlowGraph.cpp
	lib/Utils.cpp
	lib/TranslationForest.cpp
	lib/Translations.cpp

	
	# ======= Hacky Noelle Embedding ======= 
	lib/noelle/MetadataEntry.cpp
	lib/noelle/MetadataManager_Instruction.cpp
	lib/noelle/MetadataManager_Module.cpp

	lib/noelle/DataFlowAnalysis.cpp
	lib/noelle/DataFlowEngine.cpp
	lib/noelle/DataFlowResult.cpp

	# lib/noelle/BitMatrix.cpp
	# lib/noelle/CallGraph.cpp
	# lib/noelle/CallGraphEdge.cpp
	# lib/noelle/CallGraphNode.cpp
	# lib/noelle/CallGraphTraits.cpp
	# lib/noelle/IntegrationWithSVF.cpp
	# lib/noelle/PDGAnalysis_callGraph.cpp
	# lib/noelle/PDGAnalysis_compare.cpp
	# lib/noelle/PDGAnalysis_controlDependences.cpp
	# lib/noelle/PDGAnalysis.cpp
	# lib/noelle/PDGAnalysis_embedder.cpp
	# lib/noelle/PDGAnalysis_memory.cpp
	# lib/noelle/PDG.cpp
	# lib/noelle/PDGPrinter.cpp
	# lib/noelle/SCCCAG.cpp
	# lib/noelle/SCCCAGNode.cpp
	# lib/noelle/SCCCAGNode_Function.cpp
	# lib/noelle/SCCCAGNode_SCC.cpp
	# lib/noelle/SCC.cpp
	# lib/noelle/SCCDAG.cpp
	# lib/noelle/SubCFGs.cpp
	# lib/noelle/Utils.cpp
	# lib/noelle/AllocAA.cpp
	# lib/noelle/MemorySite.cpp
	# ======= Hacky Noelle Embedding ======= 

	lib/ct/Configurations.cpp
	lib/ct/LatencyDFA.cpp
	lib/ct/LoopTransform.cpp
	lib/ct/Utils.cpp

	passes/CompilerTiming.cpp
	passes/LockInsertion.cpp
	passes/LockPrinter.cpp
	passes/ArgLockElision.cpp
	passes/Normalize.cpp
	passes/Escape.cpp
	passes/TranslatePass.cpp
	passes/Replacement.cpp
	passes/LinkLibrary.cpp
	passes/Lower.cpp

	PLUGIN_TOOL
	opt
)


add_subdirectory(bin)

