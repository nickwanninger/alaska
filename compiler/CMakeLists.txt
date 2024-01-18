
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -gdwarf-4 -fdeclspec ${WARNINGS} ")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++20 -Wno-deprecated -fno-exceptions -fno-rtti ") 

include_directories(
	${LLVM_INCLUDE_DIRS}
	include/
)
# add_definitions(${LLVM_DEFINITIONS})

install(DIRECTORY ldscripts DESTINATION share/alaska)
# set(LLVM_LINK_COMPONENTS Linker)


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

	passes/PinTracking.cpp
	passes/TranslationPrinter.cpp
	passes/ArgPinElision.cpp
	passes/Normalize.cpp
	passes/Escape.cpp
	passes/TranslatePass.cpp
	passes/Replacement.cpp
	passes/LinkLibrary.cpp
	passes/Lower.cpp
	passes/PlaceSafepoints.cpp
  passes/ArgumentTrace.cpp

	PLUGIN_TOOL
	opt
)

# llvm_map_components_to_libnames(llvm_libs core)
#
# # Link against LLVM libraries
# target_link_libraries(Alaska PRIVATE ${llvm_libs})
#

add_subdirectory(bin)

