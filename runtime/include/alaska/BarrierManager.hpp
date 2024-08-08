#pragma once

#include <alaska/alaska.hpp>


namespace alaska {


  // A BarrierManager exists to abstract the delivery of "barrier" events in alaksa. This is needed
  // because different configurations of alaska require different mechanisms to "stop the world" and
  // track all the pinned handles at a point in time. Some systems require sending signals to every
  // thread, some don't require anything. Therefore, when a Runtime is constructed, a BarrierManager
  // can be attached to it, effectively injecting the requirements of the runtime system into the
  // core runtime's capabilities.
  //
  // This class also records some statistics about barrier events as they occur.
  // The only requirement of this class is that when `::begin()` returns, all pinned handles must
  // be marked as such, and when `::end()` returns they should be released. The core runtime relies
  // on this to be the case for safe operation. If the user of the core runtime does not need to
  // pin any handles, the barrier does not need to do anything.
  struct BarrierManager : public alaska::InternalHeapAllocated {
    virtual ~BarrierManager() = default;
    virtual void begin(void){};
    virtual void end(void){};

    unsigned long barrier_count = 0;
  };
}  // namespace alaska
