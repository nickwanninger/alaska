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
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include <alaska/StackMapParser.h>

#include <alaska.h>
#include <alaska/alaska.hpp>
#include <alaska/service.hpp>
#include <alaska/table.hpp>
#include <alaska/barrier.hpp>

#include <ck/map.h>
#include <ck/set.h>
#include <ck/vec.h>


#include <execinfo.h>
#include <pthread.h>
#include <alaska/list_head.h>
#include <stdbool.h>
#include <sys/signal.h>
#include <sys/mman.h>
#include <string.h>
#include <assert.h>


struct IndirectMapping {
  uint32_t regNum;
  int32_t offset;
};

static ck::map<uintptr_t, ck::vec<IndirectMapping>> stack_maps;
bool alaska_should_safepoint = false;

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


void alaska_remove_from_local_lock_list(void* ptr) {
  return;
#ifdef ALASKA_LOCK_TRACKING
  alaska::LockFrame* cur;

  cur = alaska_lock_root_chain;
  while (cur != NULL) {
    for (uint64_t i = 0; i < cur->count; i++) {
      if (cur->locked[i] == ptr) {
        cur->locked[i] = NULL;
        return;
      }
    }
    cur = cur->prev;
  }
#endif
}



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



static bool might_be_handle(void* possible_handle) {
  alaska::Mapping* m = alaska::Mapping::from_handle_safe(possible_handle);
  return m != nullptr;
}



ck::set<void*> alaska::barrier::get_locked(void) {
  ck::set<void*> out;

  alaska::LockFrame* cur;

  cur = alaska_lock_root_chain;
  while (cur != NULL) {
    for (uint64_t i = 0; i < cur->count; i++) {
      if (might_be_handle(cur->locked[i])) out.add(cur->locked[i]);
    }
    cur = cur->prev;
  }

  return out;
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

  printf("Threads: %d\n", num_threads);


  alaska::barrier::block_access_to_safepoint_page();
  // send signals to every other thread.
  // struct alaska_thread_info* pos;
  // list_for_each_entry(pos, &all_threads, list_head) {
  //   if (pos->thread != pthread_self()) {
  //     pthread_kill(pos->thread, SIGUSR2);
  //   }
  // }
  //
  alaska_barrier_join(true);
}




void alaska::barrier::end(void) {
  alaska::barrier::allow_access_to_safepoint_page();
  // Join the barrier to signal everyone we are done.
  alaska_barrier_leave(true);
  // Unlock all the locks we took.
  pthread_mutex_unlock(&all_threads_lock);
  pthread_mutex_unlock(&barrier_lock);
}




void alaska_barrier(void) {
  // auto start = alaska_timestamp();
  // Simply defer to the service. It will begin/end the barrier
  alaska::service::barrier();
  // auto end = alaska_timestamp();
  // printf("%f\n", (end - start) / 1000.0);
}




static void barrier_signal_handler(int sig) {
  // Simply join the barrier, then leave immediately. This
  // will deal with all the synchronization that needs done.
  alaska_barrier_join(false);
  alaska_barrier_leave(false);
}




void alaska::barrier::add_thread(pthread_t* thread) {
  assert(pthread_equal(*thread, pthread_self()));
  // Setup a signal handler for SIGUSR2 for remote
  // barrier calls. We basically just hope that the
  // app doesn't need this signal, as we are SOL if
  // they do!
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_handler = barrier_signal_handler;


  // signal(SIGUSR2, barrier_signal_handler);
  if (sigaction(SIGUSR2, &act, NULL) != 0) {
    perror("Failed to add sigaction to new thread.\n");
    exit(EXIT_FAILURE);
  }

  if (sigaction(SIGSEGV, &act, NULL) != 0) {
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


void alaska::barrier::initialize_safepoint_page(void) {
  auto res =
      mmap(ALASKA_SAFEPOINT_PAGE, 4096, PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  assert(res == ALASKA_SAFEPOINT_PAGE && "Failed to allocate safepoint page");
}

void alaska::barrier::block_access_to_safepoint_page() {
  mprotect(ALASKA_SAFEPOINT_PAGE, 4096, PROT_NONE);
}

void alaska::barrier::allow_access_to_safepoint_page() {
  mprotect(ALASKA_SAFEPOINT_PAGE, 4096, PROT_WRITE | PROT_READ);
}




const char* regname(int r) {
  switch (r) {
    case 0:
      return "RAX";
    case 1:
      return "RDX";
    case 2:
      return "RCX";
    case 3:
      return "RBX";
    case 4:
      return "RSI";
    case 5:
      return "RDI";
    case 6:
      return "RBP";
    case 7:
      return "RSP";
    case 8:
      return "R8";
    case 9:
      return "R9";
    case 10:
      return "R10";
    case 11:
      return "R11";
    case 12:
      return "R12";
    case 13:
      return "R13";
    case 14:
      return "R14";
    case 15:
      return "R15";
    case 16:
      return "RIP";
    case 17:
      return "XMM0";
    case 33:
      return "ST0";
    case 34:
      return "ST1";
    case 35:
      return "ST2";
    case 36:
      return "ST3";
    case 37:
      return "ST4";
    case 38:
      return "ST5";
    case 39:
      return "ST6";
    case 40:
      return "ST7";
    case 49:
      return "EFLAGS";
    case 50:
      return "ES";
    case 51:
      return "CS";
    case 52:
      return "SS";
    case 53:
      return "DS";
    case 54:
      return "FS";
    case 55:
      return "GS";
    case 62:
      return "TR";
    case 63:
      return "LDTR";
    case 64:
      return "MXCSR";
    case 65:
      return "CTRL";
    case 66:
      return "STAT";
  }
  return "UNK";
}



const uint8_t patch_bytes[] = {
    // test qword [0x7ffff000], rax
    // 0x48, 0x85, 0x04, 0x25, 0x00, 0xf0, 0xff, 0x7f,
    //
    // mov rax,QWORD PTR ds:0x7ffff000
    0x48, 0x8b, 0x04, 0x25, 0x00, 0xf0, 0xff, 0x7f

    // mov [0x7ffff000],edi
    // 0x48, 0x89, 0x3c, 0x25, 0x00, 0xf0, 0xff, 0x7f
};
void readStackMap(uint8_t* t) {
  alaska::StackMapParser p(t);

  printf("Version:     %d\n", p.getVersion());
  printf("Constants:   %d\n", p.getNumConstants());
  printf("Functions:   %d\n", p.getNumFunctions());

  auto start = alaska_timestamp();


  auto currFunc = p.functions_begin();
  size_t recordCount = 0;

  for (auto it = p.records_begin(); it != p.records_end(); it++) {
    auto record = *it;
    std::uintptr_t addr = currFunc->getFunctionAddress() + record.getInstructionOffset();
    if (++recordCount == currFunc->getRecordCount()) {
      currFunc++;
      recordCount = 0;
    }

    if (record.getID() == 'PATC') {
      // continue;
      // Apply the instruction patch
      auto* patch_addr = (void*)(addr - sizeof(patch_bytes));
      auto patch_page = (void*)((uintptr_t)patch_addr & ~0xFFF);
      mprotect(patch_page, 0x2000, PROT_EXEC | PROT_READ | PROT_WRITE);
      memcpy(patch_addr, patch_bytes, sizeof(patch_bytes));
      // mprotect(patch_page, 0x2000, PROT_EXEC | PROT_READ);
    }
    continue;

    ck::vec<IndirectMapping> mappings;
    for (std::uint16_t i = 3; i < record.getNumLocations(); i++) {
      auto l = record.getLocation(i);
      printf("a:%p id:%p ", addr, record.getID());

      void* array[1];
      array[0] = (void*)addr;
      char** names = backtrace_symbols(array, 1);
      printf("(%s) + %-5d ", names[0], record.getInstructionOffset());
      free(names);
      switch (l.getKind()) {
        case alaska::StackMapParser::LocationKind::Register:
          printf("Register %d\n", l.getDwarfRegNum());
          break;
        case alaska::StackMapParser::LocationKind::Direct:
          printf("Direct %s + %d\n", regname(l.getDwarfRegNum()), l.getOffset());
          break;
        case alaska::StackMapParser::LocationKind::Indirect:
          mappings.push({l.getDwarfRegNum(), l.getOffset()});
          printf("Indirect %s + %d %d\n", regname(l.getDwarfRegNum()), l.getOffset(),
              l.getSizeInBytes());
          break;

        case alaska::StackMapParser::LocationKind::Constant:
          printf("Constant %d\n", l.getSmallConstant());
          break;
        case alaska::StackMapParser::LocationKind::ConstantIndex:
          printf("<Constant Index>\n");
          break;
      }
    }

    stack_maps[addr] = move(mappings);
  }

  auto end = alaska_timestamp();

  printf("patching took %lu\n", (end - start) / 1000);
}


extern "C" void alaska_register_stack_map(void* map) {
  if (map) {
    printf("Register stack map %p\n", map);
    readStackMap((uint8_t*)map);
  }
}



extern "C" void alaska_show_backtrace(void) {
  return;
  // if (p == NULL) return;
  ck::set<void*> old_set, new_set;

  old_set = alaska::barrier::get_locked();

  auto start = alaska_timestamp();
  // return;
  unw_cursor_t cursor;
  unw_context_t uc;
  unw_word_t ip, sp, reg;

  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);
  while (unw_step(&cursor) > 0) {
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    unw_get_reg(&cursor, UNW_REG_SP, &sp);


    auto it = stack_maps.find(ip);

    if (it != stack_maps.end()) {
      auto& mappings = *it;

      for (auto& m : mappings.value) {
        unw_get_reg(&cursor, m.regNum, &reg);
        void* loc = *(void**)(reg + m.offset);
        if (might_be_handle(loc)) {
          new_set.add(loc);
        }
      }
    }
  }

  auto end = alaska_timestamp();
  (void)end;
  (void)start;
  // printf("%f ms\n", (end - start) / 1000.0 / 1000.0);
  // if (new_set.size() == 0) return;
  // printf("New:");
  // for (auto p : new_set)
  //   printf(" %p", p);
  // printf("\n");

  if (old_set.is_subset_of(new_set)) {
    // Nothing
  } else {
    printf("Not a subset!\n");
    printf("Old:");
    for (auto p : old_set)
      printf(" %p", p);
    printf("\n");
    printf("New:");
    for (auto p : new_set)
      printf(" %p", p);
    printf("\n");
  }
}

extern "C" void alaska_barrier_poll(volatile int* foo) {
  // int x = *(volatile int *)ALASKA_SAFEPOINT_PAGE;
  // alaska_show_backtrace();
  // asm __volatile__(".byte 0x48, 0x85, 0x04, 0x25, 0x00, 0xf0, 0xff, 0x7f\n");
  return;
}
