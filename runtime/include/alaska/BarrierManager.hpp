#pragma once



namespace alaska {


  // A BarrierManager exists to abstract the delivery of "barrier" events in alaksa. This is needed
  // because different configurations of alaska require different mechanisms to "stop the world" and
  // track all the pinned handles at a point in time. Some systems require sending signals to every
  // thread, some don't require anything. Therefore, when a Runtime is constructed, a BarrierManager
  // can be attached to it, effectively injecting the requirements of the runtime system into the
  // core runtime's capabilities.
  //
  // This class also records some statistics about barrier events as they occur.
  struct BarrierManager {
    virtual ~BarrierManager() = default;
    virtual void begin(void) = 0;
    virtual void end(void) = 0;

    unsigned long barrier_count = 0;
  };
}
