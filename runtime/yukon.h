#pragma once

#include <stdlib.h>


#ifdef __cplusplus
extern "C" {
#endif

// This is an interface to use when utilizing the yukon configuration
// of alaska. That is, when alaska is being used as an LD_PRELOAD
// library.  As a result, we want to allow providing `alaska_halloc` as a
// as a function in the baseline which just calls `malloc`.

#ifdef __cplusplus
}
#endif
