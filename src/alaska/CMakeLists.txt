

include_directories(include)

add_llvm_library(Alaska MODULE
	src/Pass.cpp
	src/Graph.cpp

	PLUGIN_TOOL
	opt
)
