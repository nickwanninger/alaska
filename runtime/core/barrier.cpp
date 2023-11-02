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


#include <dlfcn.h>

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



struct PinSetInfo {
  uint32_t count;   // How many entries?
  uint32_t regNum;  // Which libunwind register?
  int32_t offset;   // offset from that register?
};
static ck::map<uintptr_t, PinSetInfo> pin_map;




struct StackMapping {
  bool direct;
  uint32_t regNum;
  int32_t offset;
  uint64_t id;
};


// #define INSTRUCTION_BARRIER ((uint64_t)0x7ffff00025048b48ULL)  // mov rax, QWORD PTR ds:0x7ffff000
#define INSTRUCTION_BARRIER ((uint64_t)0x0b0fULL)  // mov rax, QWORD PTR ds:0x7ffff000
#define INSTRUCTION_NOP ((uint64_t)0x0000020000841f0fULL)      // nopl   0x200(%rax,%rax,1)

static ck::map<uintptr_t, ck::vec<StackMapping>> stack_maps;
static ck::vec<void*> patchPoints;


template <typename T>
static void patch(T inst) {
  // return;
  for (auto& patch_addr : patchPoints) {
    auto* dst = (T*)patch_addr;
    __atomic_store_n(dst, inst, __ATOMIC_RELEASE);
  }
}


// The definition for thread-local root chains
alaska::LockFrame alaska_lock_chain_base = {NULL, NULL, 0};
extern "C" __thread alaska::LockFrame* alaska_lock_root_chain = &alaska_lock_chain_base;
__thread alaska_thread_state_t alaska_thread_state;

extern uint64_t alaska_next_usage_timestamp;

// We track all threads w/ a simple linked list
struct alaska_thread_info {
  pthread_t thread;
  alaska_thread_state_t* state;
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



void alaska_dump_thread_states(void) {
  struct alaska_thread_info* pos;
  list_for_each_entry(pos, &all_threads, list_head) {
    printf("%d ", pos->state->escaped);
  }
  printf("\n");
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



void alaska::barrier::get_locked(ck::set<void*>& out) {
  unw_cursor_t cursor;
  unw_context_t uc;
  unw_word_t ip, sp, reg;

  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);
  while (unw_step(&cursor) > 0) {
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    unw_get_reg(&cursor, UNW_REG_SP, &sp);

    auto it = pin_map.find(ip);
    if (it != pin_map.end()) {
      auto& psi = it->value;
      unw_get_reg(&cursor, psi.regNum, &reg);


      void** localSet = (void**)(reg + psi.offset);


      printf("%p %d (%p) |", ip, psi.count, localSet);

      for (uint32_t i = 0; i < psi.count; i++) {
        printf(" %016llx", localSet[i]);
        out.add(localSet[i]);
      }

      printf("\n");

      // for (auto& m : mappings.value) {
      //   if (m.direct) {
      //     unw_get_reg(&cursor, m.regNum, &reg);
      //     void* loc = (void*)(reg + m.offset);
      //     out.add(loc);
      //     assert(out.contains(loc));
      //   } else {
      //     unw_get_reg(&cursor, m.regNum, &reg);
      //     void* loc = *(void**)(reg + m.offset);
      //     out.add(loc);
      //     assert(out.contains(loc));
      //   }
      // }
    }

    // auto it = stack_maps.find(ip);
    // if (it != stack_maps.end()) {
    //   auto& mappings = *it;
    //
    //   for (auto& m : mappings.value) {
    //     if (m.direct) {
    //       unw_get_reg(&cursor, m.regNum, &reg);
    //       void* loc = (void*)(reg + m.offset);
    //       out.add(loc);
    //       assert(out.contains(loc));
    //     } else {
    //       unw_get_reg(&cursor, m.regNum, &reg);
    //       void* loc = *(void**)(reg + m.offset);
    //       out.add(loc);
    //       assert(out.contains(loc));
    //     }
    //   }
    // }
  }
}

static void alaska_barrier_join(bool leader, const ck::set<void*>& ps) {
  // if (!leader) {
  for (auto* p : ps) {
    record_handle(p, true);
  }
  // }

  // Wait on the barrier so everyone's state has been commited.
  if (num_threads > 1) {
    pthread_barrier_wait(&the_barrier);
  }
}




static void alaska_barrier_leave(bool leader, const ck::set<void*>& ps) {
  // wait for the the leader (and everyone else to catch up)
  if (num_threads > 1) {
    pthread_barrier_wait(&the_barrier);
  }

  // go and clean up our commits to the global structure.
  for (auto* p : ps) {
    record_handle(p, false);
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
  patch(INSTRUCTION_BARRIER);
  // send signals to every other thread.
  // struct alaska_thread_info* pos;
  // list_for_each_entry(pos, &all_threads, list_head) {
  //   if (pos->thread != pthread_self()) {
  //     pthread_kill(pos->thread, SIGUSR2);
  //   }
  // }
  //
  ck::set<void*> locked;
  alaska::barrier::get_locked(locked);  // TODO: slow!
  alaska_barrier_join(true, locked);
}




void alaska::barrier::end(void) {
  patch(INSTRUCTION_NOP);
  alaska::barrier::allow_access_to_safepoint_page();
  ck::set<void*> locked;
  alaska::barrier::get_locked(locked);  // TODO: slow!
  // Join the barrier to signal everyone we are done.
  alaska_barrier_leave(true, locked);
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
  ck::set<void*> ps;
  alaska::barrier::get_locked(ps);  // TODO: slow!
#if 0
  // printf("\n\n\n");
  // unw_context_t context;
  // unw_cursor_t cursor;
  // unw_getcontext(&context);
  // unw_init_local(&cursor, &context);
  // unw_word_t pc, sp;
  // do {
  //   unw_get_reg(&cursor, UNW_REG_IP, &pc);
  //   unw_get_reg(&cursor, UNW_REG_SP, &sp);
  //   printf("pc=0x%p sp=0x%016zx", (size_t)pc, (size_t)sp);
  //   Dl_info info = {};
  //   if (dladdr((void*)pc, &info))
  //     printf(" %s:%s", info.dli_fname, info.dli_sname ? info.dli_sname : "");
  //   puts("");
  //
  //   auto it = stack_maps.find(pc);
  //
  //   if (it != stack_maps.end()) {
  //     auto& mappings = *it;
  //     for (auto& m : mappings.value) {
  //       printf("   %d %d\n", m.regNum, m.offset);
  //     }
  //   }
  // } while (unw_step(&cursor) > 0);




  ck::set<void*> pss;
  for (auto p : ps) {
    // printf("pinned %p\n", p);
    pss.add(p);
  }

  printf("===== Checking =====\n");
  bool valid = true;
  alaska::LockFrame* cur = alaska_lock_root_chain;
  while (cur != NULL) {
    printf("cur = %p, f = %p\n", cur, cur->func);
    for (uint64_t i = 0; i < cur->count; i++) {
      void* p = cur->locked[i];
      if (p == NULL) continue;
      if (!pss.contains(p)) {
        printf("%p not in set!\n", p);
        valid = false;
      } else {
        printf("%p in set!\n", p);
      }
    }
    cur = cur->prev;
  }
  if (!valid) {
    printf("\e[31m============================\e[0m\n");
    printf("Not valid! Aborting\n");
    abort();
  }

  printf("\n\n");
#endif

  // Simply join the barrier, then leave immediately. This
  // will deal with all the synchronization that needs done.
  alaska_barrier_join(false, ps);
  alaska_barrier_leave(false, ps);
  printf("\n\n");
}




void alaska::barrier::add_self_thread(void) {
  auto self = pthread_self();
  // Setup a signal handler for SIGUSR2 for remote
  // barrier calls. We basically just hope that the
  // app doesn't need this signal, as we are SOL if
  // they do!
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_handler = barrier_signal_handler;


  // signal(SIGUSR2, barrier_signal_handler);
  // if (sigaction(SIGUSR2, &act, NULL) != 0) {
  //   perror("Failed to add sigaction to new thread.\n");
  //   exit(EXIT_FAILURE);
  // }

  // if (sigaction(SIGSEGV, &act, NULL) != 0) {
  //   perror("Failed to add sigaction to new thread.\n");
  //   exit(EXIT_FAILURE);
  // }
  if (sigaction(SIGILL, &act, NULL) != 0) {
    perror("Failed to add sigaction to new thread.\n");
    exit(EXIT_FAILURE);
  }

  pthread_mutex_lock(&all_threads_lock);
  num_threads++;

  auto* tinfo = (alaska_thread_info*)calloc(1, sizeof(alaska_thread_info));
  tinfo->thread = self;
  tinfo->state = &alaska_thread_state;


  alaska_thread_state.escaped = 0;
  list_add(&tinfo->list_head, &all_threads);

  pthread_mutex_unlock(&all_threads_lock);
}



void alaska::barrier::remove_self_thread(void) {
  auto self = pthread_self();
  pthread_mutex_lock(&all_threads_lock);
  num_threads--;

  bool found = false;
  struct alaska_thread_info* pos;
  list_for_each_entry(pos, &all_threads, list_head) {
    if (pos->thread == self) {
      found = true;
      break;
    }
  }

  if (!found) {
    fprintf(stderr, "Failed to remove non-added thread, %p\n", self);
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



/**
 * This function parses a stackmap emitted from LLVM and pushes all
 * the patch points to
 */
void readStackMap(uint8_t* t) {
  // return;
  alaska::StackMapParser p(t);

  printf("Version:     %d\n", p.getVersion());
  printf("Constants:   %d\n", p.getNumConstants());
  printf("Functions:   %d\n", p.getNumFunctions());

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
      // Apply the instruction patch
      auto* patch_addr = (void*)(addr - sizeof(INSTRUCTION_BARRIER));
      auto patch_page = (void*)((uintptr_t)patch_addr & ~0xFFF);
      mprotect(patch_page, 0x2000, PROT_EXEC | PROT_READ | PROT_WRITE);
      patchPoints.push(patch_addr);
    }
    if (record.getID() == 'PATC') {
      addr -= sizeof(INSTRUCTION_BARRIER);
    }

    PinSetInfo psi;

    ck::vec<StackMapping> mappings;
    for (std::uint16_t i = 3; i < record.getNumLocations(); i++) {
      auto l = record.getLocation(i);
      printf("a:%p id:%p ", addr, record.getID());

      void* array[1];
      array[0] = (void*)addr;
      char** names = backtrace_symbols(array, 1);
      printf("(%s) + %-5d ", names[0], record.getInstructionOffset());
      free(names);
      switch (l.getKind()) {
        case alaska::StackMapParser::LocationKind::Indirect:
          mappings.push({false, l.getDwarfRegNum(), l.getOffset(), record.getID()});
          printf("Indirect %s + %d %d\n", regname(l.getDwarfRegNum()), l.getOffset(),
              l.getSizeInBytes());
          break;

        case alaska::StackMapParser::LocationKind::Register:
          printf("Register %d\n", l.getDwarfRegNum());
          break;
        case alaska::StackMapParser::LocationKind::Direct:
          mappings.push({true, l.getDwarfRegNum(), l.getOffset(), record.getID()});
          psi.regNum = l.getDwarfRegNum();
          psi.offset = l.getOffset();
          printf("Direct %s + %d\n", regname(l.getDwarfRegNum()), l.getOffset());
          break;

        case alaska::StackMapParser::LocationKind::Constant:
          psi.count = l.getSmallConstant();
          printf("Constant %d\n", l.getSmallConstant());
          break;
        case alaska::StackMapParser::LocationKind::ConstantIndex:
          printf("<Constant Index>\n");
          break;
      }
    }

    pin_map[addr] = psi;

    stack_maps[addr] = move(mappings);

    printf("\n");
  }

  // exit(-1);


  auto start = alaska_timestamp();
  patch(INSTRUCTION_NOP);
  auto end = alaska_timestamp();



  // printf("\n\n\n");
  // for (auto& [addr, mappings] : stack_maps) {
  //   for (auto m : mappings) {
  //     if (m.id == 'PATC') {
  //       printf("%p %d %d\n", addr, m.regNum, m.offset);
  //     }
  //   }
  // }

  printf("patching %ld points took %lu\n", patchPoints.size(), (end - start) / 1000);
}


extern "C" void alaska_register_stack_map(void* map) {
  if (map) {
    printf("Register stack map %p\n", map);
    readStackMap((uint8_t*)map);
  }
}



// This function doesn't really need to exist,
extern "C" void alaska_barrier_poll(void) {
  int x = *(volatile int*)ALASKA_SAFEPOINT_PAGE;
  (void)x;
  // alaska_show_backtrace();
  // asm __volatile__(".byte 0x48, 0x85, 0x04, 0x25, 0x00, 0xf0, 0xff, 0x7f\n");
  return;
}
