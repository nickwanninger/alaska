/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2023, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2023, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */

#include <alaska.h>
#include <alaska/alaska.hpp>
#include <alaska/service.hpp>
#include <alaska/table.hpp>
#include <alaska/barrier.hpp>


#include <pthread.h>
#include <alaska/list_head.h>
#include <stdbool.h>
#include <sys/signal.h>
#include <string.h>
#include <assert.h>

// The definition for thread-local root chains
alaska::LockFrame alaska_lock_chain_base = {NULL, 0};
extern "C" __thread alaska::LockFrame* alaska_lock_root_chain = &alaska_lock_chain_base;

extern uint64_t alaska_next_usage_timestamp;

// We track all threads w/ a simple linked list
struct alaska_thread_info {
  pthread_t thread;
  struct list_head list_head;
};


static pthread_mutex_t all_threads_lock = PTHREAD_MUTEX_INITIALIZER;
static long num_threads = 0;
static struct list_head all_threads = LIST_HEAD_INIT(all_threads);
static pthread_mutex_t barrier_lock = PTHREAD_MUTEX_INITIALIZER;

// This is *the* barrier used in alaska_barrier to make sure threads are stopped correctly.
static pthread_barrier_t the_barrier;
static long barrier_last_num_threads = 0;




int alaska_verify_is_locally_locked(void* ptr) {
  if (!IS_ENABLED(ALASKA_LOCK_TRACKING)) return 1;
  alaska::LockFrame* cur;

  cur = alaska_lock_root_chain;
  while (cur != NULL) {
    for (uint64_t i = 0; i < cur->count; i++) {
      if (cur->locked[i] == ptr) return 1;
    }
    cur = cur->prev;
  }

  return 0;
}




static void record_handle(void* possible_handle, bool marked) {
  alaska::Mapping* m = alaska::Mapping::from_handle_safe(possible_handle);

  // It wasn't a handle, don't consider it.
  if (m == NULL) return;

  // Was it well formed (allocated?)
  if (m < alaska::table::begin() || m >= alaska::table::end()) {
    return;
  }

  alaska::service::commit_lock_status(m, marked);
}




static void alaska_barrier_join(bool leader) {
  alaska::LockFrame* cur;

  cur = alaska_lock_root_chain;
  while (cur != NULL) {
    for (uint64_t i = 0; i < cur->count; i++) {
      if (cur->locked[i]) record_handle(cur->locked[i], true);
    }
    cur = cur->prev;
  }

  // Wait on the barrier so everyone's state has been commited.
  if (num_threads > 1) {
    pthread_barrier_wait(&the_barrier);
  }
}




static void alaska_barrier_leave(bool leader) {
  alaska::LockFrame* cur;
  // wait for the the leader (and everyone else to catch up)
  if (num_threads > 1) {
    pthread_barrier_wait(&the_barrier);
  }

  // go and clean up our commits to the global structure.
  cur = alaska_lock_root_chain;
  while (cur != NULL) {
    for (uint64_t i = 0; i < cur->count; i++) {
      if (cur->locked[i]) {
        record_handle(cur->locked[i], false);
      }
    }
    cur = cur->prev;
  }
}




void alaska::barrier::begin(void) {
  // As the leader, signal everyone to begin the barrier
  pthread_mutex_lock(&barrier_lock);
  // also lock the thread list! Nobody is allowed to create a thread right now.
  pthread_mutex_lock(&all_threads_lock);

  if (barrier_last_num_threads != num_threads) {
    if (barrier_last_num_threads != 0) pthread_barrier_destroy(&the_barrier);
    // Initialize the barrier so we know when everyone is ready!
    pthread_barrier_init(&the_barrier, NULL, num_threads);
    barrier_last_num_threads = num_threads;
  }

  // send signals to every other thread.
  struct alaska_thread_info* pos;
  list_for_each_entry(pos, &all_threads, list_head) {
    if (pos->thread != pthread_self()) {
      pthread_kill(pos->thread, SIGUSR2);
    }
  }

  alaska_barrier_join(true);
}




void alaska::barrier::end(void) {
  // Join the barrier to signal everyone we are done.
  alaska_barrier_leave(true);
  // Unlock all the locks we took.
  pthread_mutex_unlock(&all_threads_lock);
  pthread_mutex_unlock(&barrier_lock);
}




void alaska_barrier(void) {
  alaska::barrier::begin();
  alaska::service::barrier();
  alaska::barrier::end();
}




static void barrier_signal_handler(int sig) {
  // Simply join the barrier, then leave immediately. This
  // will deal with all the synchronization that needs done.
  alaska_barrier_join(false);
  alaska_barrier_leave(false);
}




void alaska::barrier::add_thread(pthread_t* thread) {
  // Setup a signal handler for SIGUSR2 for remote barrier calls. We basically just hope that the
  // app doesn't need this signal, as we are SOL if they do!
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_handler = [](int sig) {

  };

  if (sigaction(SIGUSR2, &act, NULL) != 0) {
    perror("Failed to add sigaction to new thread.\n");
    exit(EXIT_FAILURE);
  }

  pthread_mutex_lock(&all_threads_lock);
  num_threads++;

  auto* tinfo = (alaska_thread_info*)calloc(1, sizeof(alaska_thread_info));
  tinfo->thread = *thread;
  list_add(&tinfo->list_head, &all_threads);

  pthread_mutex_unlock(&all_threads_lock);
}




void alaska::barrier::remove_thread(pthread_t* thread) {
  pthread_mutex_lock(&all_threads_lock);
  num_threads--;

  bool found = false;
  struct alaska_thread_info* pos;
  list_for_each_entry(pos, &all_threads, list_head) {
    if (pos->thread == *thread) {
      found = true;
      break;
    }
  }

  if (!found) {
    fprintf(stderr, "Failed to remove non-added thread, %p\n", *thread);
    abort();
  }

  list_del(&pos->list_head);
  pthread_mutex_unlock(&all_threads_lock);
}
