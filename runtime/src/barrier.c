#include <alaska.h>
#include <alaska/internal.h>

// This just means that the application has decided it is willing to let alaska
// potentially stop the world and do what it needs to do. This function doesn't
// do anything yet, but it could :)
__declspec(noinline) void alaska_barrier(void) {
	//
}
