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


static void* alaska_signal_function = NULL;
static ck::map<uintptr_t, ck::vec<StackMapping>> stack_maps;


#ifdef __amd64__
using inst_t = uint64_t;
#endif
#ifdef __aarch64__
using inst_t = uint32_t;
#endif

struct PatchPoint {
  inst_t* pc;
  inst_t inst_nop;
  inst_t inst_sig;
};
static ck::vec<PatchPoint> patchPoints;


static void patchSignal() {
  for (auto& p : patchPoints) {
    __atomic_store_n(p.pc, p.inst_sig, __ATOMIC_RELEASE);
  }
}


static void patchNop(void) {
  for (auto& p : patchPoints) {
    __atomic_store_n(p.pc, p.inst_nop, __ATOMIC_RELEASE);
  }
}

// The definition for thread-local root chains
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
}



void alaska_dump_thread_states(void) {
  struct alaska_thread_info* pos;
  list_for_each_entry(pos, &all_threads, list_head) {
    printf("%d ", pos->state->escaped);
  }
  printf("\n");
}




static void record_handle(void* possible_handle, bool marked) {
  alaska::Mapping* m = alaska::Mapping::from_handle_safe(possible_handle);

  // It wasn't a handle, don't consider it.
  if (m == NULL) return;

  // Was it well formed? Is it in the table?
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
  unw_word_t pc, sp, reg;

  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);
  while (1) {
    int res = unw_step(&cursor);
    if (res == 0) {
      break;
    }
    if (res < 0) {
      printf("unknown libunwind error! %d\n", res);
      abort();
    }
    unw_get_reg(&cursor, UNW_REG_IP, &pc);
    unw_get_reg(&cursor, UNW_REG_SP, &sp);
    // printf("pc:%016lx sp:%016lx ", pc, sp);
    auto it = pin_map.find(pc);
    if (it != pin_map.end()) {
      auto& psi = it->value;
      unw_get_reg(&cursor, psi.regNum, &reg);


      void** localSet = (void**)(reg + psi.offset);

      // printf("%p %d (%p) |", pc, psi.count, localSet);
      for (uint32_t i = 0; i < psi.count; i++) {
        if (might_be_handle(localSet[i])) {
          out.add(localSet[i]);
          // printf(" %016llx", localSet[i]);
        }
      }
    }
    // printf("\n");
  }
}

static void alaska_barrier_join(bool leader, const ck::set<void*>& ps) {
  for (auto* p : ps) {
    record_handle(p, true);
  }
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

  alaska_dump_thread_states();

  patchSignal();

  ck::set<void*> locked;
  alaska::barrier::get_locked(locked);  // TODO: slow!
  alaska_barrier_join(true, locked);
}




void alaska::barrier::end(void) {
  patchNop();
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



extern "C" void alaska_barrier_signal_join(void) {
  ck::set<void*> ps;
  alaska_dump_backtrace();
  alaska::barrier::get_locked(ps);  // TODO: slow!
  alaska_barrier_join(false, ps);
  alaska_barrier_leave(false, ps);
}


static void barrier_signal_handler(int sig) {
  ck::set<void*> ps;
  alaska::barrier::get_locked(ps);  // TODO: slow!

  // Simply join the barrier, then leave immediately. This
  // will deal with all the synchronization that needs done.
  alaska_barrier_join(false, ps);
  alaska_barrier_leave(false, ps);
  printf("\n\n");
}


void alaska::barrier::add_self_thread(void) {
  auto self = pthread_self();
  pthread_mutex_lock(&all_threads_lock);
  num_threads++;


  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_handler = barrier_signal_handler;
  if (sigaction(SIGILL, &act, NULL) != 0) {
    perror("Failed to add sigaction to new thread.\n");
    exit(EXIT_FAILURE);
  }

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
void readStackMap(uint8_t* t, void* signalFunc) {
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
      auto* rip = (void*)(addr - ALASKA_PATCH_SIZE);
      auto patch_page = (void*)((uintptr_t)rip & ~0xFFF);
      mprotect(patch_page, 0x2000, PROT_EXEC | PROT_READ | PROT_WRITE);


      // Byte pointer to the instruction.
      uint8_t* ripB = (uint8_t*)rip;

      PatchPoint p;

      p.pc = (inst_t*)rip;
      // TODO: ARM

#ifdef __amd64__
      {
        // uint8_t inst[8] = {0xe8, 0, 0, 0, 0, 1, 1, 1};
        // uint8_t inst[8] = {0xcc, 0, 0, 0, 0, 1, 1, 1};
        uint8_t inst[8] = {0x0F, 0xFF, 0, 0, 0, 1, 1, 1};

        // Compute the address after the call inst.
        // uint64_t after = (uint64_t)rip + ALASKA_PATCH_SIZE;
        // uint32_t dstRel = (uint64_t)signalFunc - after;
        // *(uint32_t*)(inst + 1) = dstRel;

        // inst[5] = ripB[5];
        // inst[6] = ripB[6];
        // inst[7] = ripB[7];
        p.inst_call = *(uint64_t*)inst;
      }

      {
        uint8_t inst[8] = {0x0f, 0x1f, 0x44, 0x00, 0x08, 1, 1, 1};
        uint8_t* ripB = (uint8_t*)rip;

        inst[5] = ripB[5];
        inst[6] = ripB[6];
        inst[7] = ripB[7];

        p.inst_nop = *(uint64_t*)inst;
      }
#endif
#ifdef __aarch64__
      p.inst_nop = 0xd503201f; // nop
      p.inst_sig = 0x00000000; // udf #0
#endif

      patchPoints.push(p);
    }
    if (record.getID() == 'PATC') {
      addr -= ALASKA_PATCH_SIZE;
    }

    PinSetInfo psi;

    ck::vec<StackMapping> mappings;
    for (std::uint16_t i = 3; i < record.getNumLocations(); i++) {
      auto l = record.getLocation(i);
      // printf("a:%p id:%p ", addr, record.getID());

      void* array[1];
      array[0] = (void*)addr;
      char** names = backtrace_symbols(array, 1);
      // printf("(%s) + %-5d ", names[0], record.getInstructionOffset());
      free(names);
      switch (l.getKind()) {
        case alaska::StackMapParser::LocationKind::Indirect:
          mappings.push({false, l.getDwarfRegNum(), l.getOffset(), record.getID()});
          // printf("Indirect %s + %d %d\n", regname(l.getDwarfRegNum()), l.getOffset(),
          //     l.getSizeInBytes());
          break;

        case alaska::StackMapParser::LocationKind::Register:
          // printf("Register %d\n", l.getDwarfRegNum());
          break;
        case alaska::StackMapParser::LocationKind::Direct:
          mappings.push({true, l.getDwarfRegNum(), l.getOffset(), record.getID()});
          psi.regNum = l.getDwarfRegNum();
          psi.offset = l.getOffset();
          // printf("Direct %s + %d\n", regname(l.getDwarfRegNum()), l.getOffset());
          break;

        case alaska::StackMapParser::LocationKind::Constant:
          psi.count = l.getSmallConstant();
          // printf("Constant %d\n", l.getSmallConstant());
          break;
        case alaska::StackMapParser::LocationKind::ConstantIndex:
          // printf("<Constant Index>\n");
          break;
      }
    }

    pin_map[addr] = psi;

    stack_maps[addr] = move(mappings);

    // printf("\n");
  }

  patchNop();



  alaska_signal_function = signalFunc;
}


extern "C" void alaska_register_stack_map(void* map, void* signalFunc) {
  if (map) {
    printf("Register stack map %p\n", map);
    readStackMap((uint8_t*)map, signalFunc);
  }
}



// This function doesn't really need to exist,
extern "C" void alaska_barrier_poll(void) {
  printf("yee\n");
  abort();
  return;
}
