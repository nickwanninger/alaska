/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2024, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2024, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */

#pragma once

#include <alaska/alaska.hpp>
#include <alaska/util/list_head.h>

namespace alaska {

  // Forward declaration
  class WorkScheduler;

  /**
   * @brief A periodic worker is a task that should be run repeatedly at some frequency (or every
   * tick).
   *
   * It replaces the deprecated BarrierWorker.
   * Subclasses should implement `periodic_work()`.
   */
  class Worker {
   public:
    Worker();  // implemented in .cpp or inline if needed, but let's do inline for now
    virtual ~Worker();

    // The function called by the scheduler.
    // deltaTime: the simulated time passed since the last tick.
    virtual void periodic_work(float deltaTime) = 0;
    virtual void deferred_work();

    // Schedule a deferred task.
    // This requires the worker to be registered with the scheduler.
    void schedule_deferred();

    void register_periodic_work(WorkScheduler& scheduler);
    void deregister_periodic_work();

   protected:
    friend class WorkScheduler;
    struct list_head m_list;
    WorkScheduler* m_scheduler = nullptr;
    bool m_deferred_scheduled = false;
  };

  /**
   * @brief WorkScheduler manages background workers on a single timer.
   *
   * Each tick runs periodic_work() on all workers, then deferred_work()
   * on any worker that flagged itself via schedule_deferred().
   * The tick interval is configured via set_interval().
   */
  class WorkScheduler {
   public:
    WorkScheduler();
    ~WorkScheduler();

    void schedule_periodic(Worker* worker);
    void deschedule_periodic(Worker* worker);

    void set_interval(float seconds) { m_interval_s = seconds; }

    // Run one tick. Returns the time (in seconds) the caller should wait
    // before calling tick() again.
    float tick(float deltaTime);

    // Main loop entry point for a background thread.
    void work();

   private:
    struct list_head m_periodic_workers;
    float m_interval_s = 0.25f;  // Measured in seconds
  };

  // Inline implementations for Worker

  inline Worker::Worker() { INIT_LIST_HEAD(&m_list); }

  inline Worker::~Worker() { deregister_periodic_work(); }

  inline void Worker::register_periodic_work(WorkScheduler& scheduler) {
    if (m_scheduler) return;
    scheduler.schedule_periodic(this);
    m_scheduler = &scheduler;
  }

  inline void Worker::deregister_periodic_work() {
    if (!m_scheduler) return;
    m_scheduler->deschedule_periodic(this);
    m_scheduler = nullptr;
  }


  inline void Worker::schedule_deferred() {
    if (m_deferred_scheduled) return;
    m_deferred_scheduled = true;
  }

  inline void Worker::deferred_work() {}
}  // namespace alaska
