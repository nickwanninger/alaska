
include_directories(
	${LLVM_INCLUDE_DIRS}
	include/
)

add_library(AlaskaCompilerLibrary OBJECT 
	lib/Graph.cpp
	lib/Utils.cpp

	lib/noelle/MetadataEntry.cpp
	lib/noelle/MetadataManager_Instruction.cpp
	lib/noelle/MetadataManager_Module.cpp
)

add_llvm_library(Alaska MODULE
	passes/TranslatePass.cpp
	$<TARGET_OBJECTS:AlaskaCompilerLibrary>

	PLUGIN_TOOL
	opt
)

add_llvm_library(AlaskaNormalize MODULE
	passes/NormalizePass.cpp
	$<TARGET_OBJECTS:AlaskaCompilerLibrary>

	PLUGIN_TOOL
	opt
)