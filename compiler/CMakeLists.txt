
include_directories(
	${LLVM_INCLUDE_DIRS}
	include/
)
install(DIRECTORY ldscripts DESTINATION share/alaska)

add_library(AlaskaCompilerLibrary OBJECT 
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
	
)


add_llvm_library(Alaska MODULE
	passes/Alaska.cpp
	passes/CompilerTiming.cpp
	$<TARGET_OBJECTS:AlaskaCompilerLibrary>

	PLUGIN_TOOL
	opt
)
# add_llvm_library(Alaska MODULE
# 	passes/TranslatePass.cpp
# 	$<TARGET_OBJECTS:AlaskaCompilerLibrary>
#
# 	PLUGIN_TOOL
# 	opt
# )
#
# add_llvm_library(AlaskaNormalize MODULE
# 	passes/NormalizePass.cpp
# 	$<TARGET_OBJECTS:AlaskaCompilerLibrary>
#
# 	PLUGIN_TOOL
# 	opt
# )
#
#
# add_llvm_library(AlaskaReplacement MODULE
# 	passes/ReplacementPass.cpp
# 	$<TARGET_OBJECTS:AlaskaCompilerLibrary>
#
# 	PLUGIN_TOOL
# 	opt
# )
#
# add_llvm_library(AlaskaEscapeHandler MODULE
# 	passes/EscapeHandlerPass.cpp
# 	$<TARGET_OBJECTS:AlaskaCompilerLibrary>
#
# 	PLUGIN_TOOL
# 	opt
# )


add_subdirectory(bin)

