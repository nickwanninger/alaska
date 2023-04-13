
include_directories(
	${LLVM_INCLUDE_DIRS}
	include/
)
install(DIRECTORY ldscripts DESTINATION share/alaska)


add_llvm_library(Alaska MODULE
	lib/Graph.cpp
	lib/Utils.cpp
	lib/LockForest.cpp
	lib/Locks.cpp

	lib/noelle/MetadataEntry.cpp
	lib/noelle/MetadataManager_Instruction.cpp
	lib/noelle/MetadataManager_Module.cpp
	lib/noelle/DataFlowAnalysis.cpp
	lib/noelle/DataFlowEngine.cpp
	lib/noelle/DataFlowResult.cpp


	lib/ct/Configurations.cpp
	lib/ct/LatencyDFA.cpp
	lib/ct/LoopTransform.cpp
	lib/ct/Utils.cpp


	passes/Alaska.cpp
	passes/CompilerTiming.cpp

	passes/LockTracker.cpp
	passes/LockPrinter.cpp
	passes/RedundantArgumentLockElision.cpp
	passes/Normalize.cpp
	passes/Escape.cpp
	passes/Translate.cpp
	passes/Replacement.cpp
	passes/LinkLibrary.cpp

	PLUGIN_TOOL
	opt
)

add_subdirectory(bin)

