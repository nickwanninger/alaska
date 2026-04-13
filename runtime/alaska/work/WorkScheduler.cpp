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

#include "WorkScheduler.hpp"
#include <alaska/util/list_head.h>
#include <alaska/core/Runtime.hpp>
#include <unistd.h>

namespace alaska {

  WorkScheduler::WorkScheduler() { INIT_LIST_HEAD(&m_periodic_workers); }

  WorkScheduler::~WorkScheduler() {}

  void WorkScheduler::schedule_periodic(Worker* worker) {
    if (!worker) return;
    list_add_tail(&worker->m_list, &m_periodic_workers);
  }

  void WorkScheduler::deschedule_periodic(Worker* worker) {
    if (!worker) return;
    if (worker->m_list.next == 0 || worker->m_list.prev == 0) return;
    if (worker->m_list.next != &worker->m_list) {
      list_del_init(&worker->m_list);
    }
  }

  float WorkScheduler::tick(float deltaTime) {
    struct list_head *pos, *n;
    list_for_each_safe(pos, n, &m_periodic_workers) {
      auto* worker = list_entry(pos, Worker, m_list);
      worker->periodic_work(deltaTime);

      if (!worker->m_deferred_scheduled) continue;
      worker->m_deferred_scheduled = false;
      worker->deferred_work();
    }
    return m_interval_s;
  }

  void WorkScheduler::work() {
    float toWait = m_interval_s;

    // XXX: Implict global runtime dependency!
    auto& rt = alaska::Runtime::get();
    while (true) {
      usleep((useconds_t)(toWait * 1000000.0f));
      rt.with_barrier([&]() {
        toWait = tick(toWait);
      });
    }
  }

}  // namespace alaska
