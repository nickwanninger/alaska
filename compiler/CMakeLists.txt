
include_directories(
	${LLVM_INCLUDE_DIRS}
	include/
)

add_llvm_library(Alaska MODULE
	passes/PinPass.cpp
	lib/Graph.cpp

	lib/noelle/MetadataEntry.cpp
	lib/noelle/MetadataManager_Instruction.cpp
	lib/noelle/MetadataManager_Module.cpp

	PLUGIN_TOOL
	opt
)

add_llvm_library(AlaskaNormalize MODULE
	passes/NormalizePass.cpp

	PLUGIN_TOOL
	opt
)