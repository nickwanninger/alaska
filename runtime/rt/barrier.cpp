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
#include <alaska/config.h>


#include <dlfcn.h>

#include <alaska.h>
#include <alaska/utils.h>
#include <alaska/alaska.hpp>
#include <alaska/rt/barrier.hpp>
#include <alaska/Runtime.hpp>
#include <alaska/ThreadRegistry.hpp>

#include <ck/lock.h>
#include <ck/map.h>
#include <ck/set.h>
#include <ck/vec.h>
#include <ck/func.h>

#include <ucontext.h>
#include <execinfo.h>
#include <pthread.h>
#include <alaska/list_head.h>
#include <stdbool.h>
#include <sys/signal.h>
#include <sys/mman.h>
#include <string.h>
#include <assert.h>


enum class StackState {
  ManagedTracked,    // The thread is in managed code, at a poll. (safe to join barrier)
  ManagedUntracked,  // The thread is in managed code, but not at a poll
  Unmanaged,         // The thread is in unmanaged code. (save to join barrier)
};


struct PinSetInfo {
  uint32_t count = 0;   // How many entries?
  uint32_t regNum = 0;  // Which libunwind register?
  int32_t offset = 0;   // offset from that register?
};

struct StackMapping {
  bool direct;
  uint32_t regNum;
  int32_t offset;
  uint64_t id;
};

struct ManagedBlobRegion {
  uintptr_t start, end;
};


static ck::vec<ManagedBlobRegion> managed_blob_text_regions;
static ck::map<uintptr_t, PinSetInfo> pin_map;
// the return addresses from calls to potentially blocking functions
static ck::set<uintptr_t> block_rets;


#ifdef __amd64__
using inst_t = uint16_t;
#endif
#ifdef __aarch64__
using inst_t = uint32_t;
#endif
#ifdef __riscv
using inst_t = uint16_t;
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

static void setup_signal_handlers(void);
static void clear_pending_signals(void);



enum class JoinReason { Signal, Safepoint };


#define ALASKA_THREAD_TRACK_STATE_T AlaskaThreadState
#define ALASKA_THREAD_TRACK_INIT setup_signal_handlers();
#include <alaska/thread_tracking.in.hpp>




// This is *the* barrier used in alaska_barrier to make sure threads are stopped correctly.
static pthread_barrier_t the_barrier;
static long barrier_last_num_threads = 0;
static pthread_mutex_t barrier_lock = PTHREAD_MUTEX_INITIALIZER;




void alaska_remove_from_local_lock_list(void* ptr) { return; }
static void alaska_dump_thread_states_r(void) {
  struct info* pos;
  alaska::thread_tracking::threads().for_each_locked([&](auto thread, AlaskaThreadState* state) {
    if (state->escaped == 0) {
      printf("\e[0m. ");  // a thread will join a barrier (not out to lunch)
    } else {
      printf("\e[41m. ");  // a thread will need to be interrupted (out to lunch)
    }
  });
  printf("\e[0m\n");
}

void alaska_dump_thread_states(void) {
  auto lk = alaska::thread_tracking::threads().take_lock();
  alaska_dump_thread_states_r();
}


static StackState get_stack_state(uintptr_t return_address) {
  if (pin_map.contains(return_address)) {
    return StackState::ManagedTracked;
  }

  for (auto [start, end] : managed_blob_text_regions) {
    if (return_address >= start && return_address < end) {
      return StackState::ManagedUntracked;
    }
  }

  return StackState::Unmanaged;
}



static void record_handle(void* possible_handle, bool marked) {
  alaska::Mapping* m = alaska::Mapping::from_handle_safe(possible_handle);

  // It wasn't a handle, don't consider it.
  if (m == NULL) return;
  if (not alaska::Runtime::get().handle_table.valid_handle(m)) return;
  if (m->is_free()) return;
  m->set_pinned(marked);
}



static bool might_be_handle(void* possible_handle) {
  alaska::Mapping* m = alaska::Mapping::from_handle_safe(possible_handle);
  return m != nullptr;
}


static ck::mutex dump_lock;

static bool in_might_block_function(uintptr_t start_addr) {
  void* buffer[512];

  int depth = backtrace(buffer, 512);
  dump_lock.lock();
  bool found_start = false;
  for (int i = 0; i < depth; i++) {
    auto addr = (uintptr_t)buffer[i];
    if (addr == start_addr) {
      found_start = true;
    }
    if (!found_start) continue;

    const char* msg = "\e[33m(unmanaged)\e[0m";

    for (auto [start, end] : managed_blob_text_regions) {
      if (addr >= start && addr < end) {
        // red
        msg = "\e[31m(managed)\e[0m";
        break;
      }
    }
    if (pin_map.contains(addr)) {
      // green
      msg = "\e[32m(managed)\e[0m";
    }

    printf("%016lx %s\n", (uintptr_t)buffer[i], msg);
  }
  printf("\n");

  dump_lock.unlock();


  for (int i = 0; i < depth; i++) {
    if (block_rets.contains((uintptr_t)buffer[i])) {
      return true;
    }
  }

  return false;
}

void alaska::barrier::get_pinned_handles(ck::set<void*>& out) {
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
      if (psi.count == 0) continue;  // should not happen!
      unw_get_reg(&cursor, psi.regNum, &reg);


      void** localSet = (void**)(reg + psi.offset);

      for (uint32_t i = 0; i < psi.count; i++) {
        if (might_be_handle(localSet[i])) {
          out.add(localSet[i]);
        }
      }
    }
  }
}



static void participant_join(bool leader, const ck::set<void*>& ps) {
  for (auto* p : ps) {
    record_handle(p, true);
  }
  // Wait on the barrier so everyone's state has been commited.
  if (alaska::thread_tracking::threads().num_threads() > 1) {
    pthread_barrier_wait(&the_barrier);
  }
}




static void participant_leave(bool leader, const ck::set<void*>& ps) {
  // wait for the the leader (and everyone else to catch up)
  if (alaska::thread_tracking::threads().num_threads() > 1) {
    pthread_barrier_wait(&the_barrier);
  }

  // go and clean up our commits to the global structure.
  for (auto* p : ps) {
    record_handle(p, false);
  }
}


void dump_thread_states(void) {
  struct info* pos;
  alaska::thread_tracking::threads().for_each_locked([&](auto thread, AlaskaThreadState* state) {
    switch (state->join_status) {
      case ALASKA_JOIN_REASON_NOT_JOINED:
        printf("\e[41m! ");  // a thread will need to be interrupted (out to lunch)
        break;

      case ALASKA_JOIN_REASON_SIGNAL:
        printf("\e[41m. ");  // a thread will need to be interrupted (out to lunch)
        break;

      case ALASKA_JOIN_REASON_SAFEPOINT:
        printf("\e[42m. ");  // a thread will join a barrier (not out to lunch)
        break;

      case ALASKA_JOIN_REASON_ORCHESTRATOR:
        printf("\e[44m. ");  // a thread will join a barrier (not out to lunch)
        break;
    }
  });
  printf("\e[0m");
}



bool alaska::barrier::begin(void) {
  // Pseudocode:
  //
  // function begin():
  //   patch();
  //   while not everyone_joined():
  //      usleep(n);
  //      for thread in threads:
  //        if not thread->joined:
  //          signal(thread);
  //   synch();


  // Take locks so nobody else tries to signal a barrier.
  pthread_mutex_lock(&barrier_lock);
  alaska::thread_tracking::threads().lock_thread_creation();



  auto num_threads = alaska::thread_tracking::threads().num_threads();
#if 0
  alaska::printf("Barrier begin from %lx:\n", pthread_self());
  alaska::printf("  num threads: %lu\n", num_threads);

  printf("  threads:\n");
  alaska::thread_tracking::threads().for_each_locked([](auto thread, AlaskaThreadState* state) {
    alaska::printf("  - %lx %p %d\n", thread, state, state->join_status);
  });
#endif


  // First, mark everyone as *not* in the barrier.
  alaska::thread_tracking::threads().for_each_locked([](auto thread, AlaskaThreadState* state) {
    state->join_status = ALASKA_JOIN_REASON_NOT_JOINED;
  });

  // Mark the orch thread (us) as joined
  alaska::thread_tracking::my_state.join_status = ALASKA_JOIN_REASON_ORCHESTRATOR;

  // If the barrier needs resizing, do so.
  if (barrier_last_num_threads != num_threads) {
    if (barrier_last_num_threads != 0) pthread_barrier_destroy(&the_barrier);
    // Initialize the barrier so we know when everyone is ready!
    pthread_barrier_init(&the_barrier, NULL, num_threads);
    barrier_last_num_threads = num_threads;
  }


  // now, patch the threads!
  patchSignal();
  int retries = 0;
  int signals_sent = 0;

  bool success = true;

  // Make sure the threads that are in unmanaged (library) code get signalled.
  while (true) {
    if (retries >= 1000) {
      success = false;
      break;
    }
    retries++;
    bool sent_signal = false;
    bool aborted = false;

    alaska::thread_tracking::threads().for_each_locked([&](auto thread, auto* state) {
      if (state->join_status == ALASKA_JOIN_REASON_NOT_JOINED) {
        pthread_kill(thread, SIGUSR2);
        sent_signal = true;
        signals_sent++;
      }

      if (state->join_status == ALASKA_JOIN_REASON_ABORT) {
        success = false;
        printf("Abort barrier. Could not join all threads for some reason!\n");
      }
    });
    if (!sent_signal) break;
    usleep(1000);  // lazy optimization
  }


  ck::set<void*> locked;
  alaska::barrier::get_pinned_handles(locked);  // TODO: slow!
  participant_join(true, locked);

  (void)retries;
  (void)signals_sent;


  return success;
}




void alaska::barrier::end(void) {
  patchNop();
  ck::set<void*> locked;
  alaska::barrier::get_pinned_handles(locked);  // TODO: slow!
  // Join the barrier to signal everyone we are done.
  participant_leave(true, locked);

  // Unlock all the locks we took.
  alaska::thread_tracking::threads().unlock_thread_creation();
  pthread_mutex_unlock(&barrier_lock);
}




void alaska_barrier(void) {
  // alaska::service::barrier();
}


thread_local bool invalid_state_abort = false;

static void alaska_barrier_signal_handler(int sig, siginfo_t* info, void* ptr) {
  ucontext_t* ucontext = (ucontext_t*)ptr;
  uintptr_t return_address = 0;

#if defined(__amd64__)
  return_address = ucontext->uc_mcontext.gregs[REG_RIP];
#elif defined(__aarch64__)
  return_address = ucontext->uc_mcontext.pc;
#elif defined(__riscv)
  return_address = ucontext->uc_mcontext.__gregs[REG_PC];
#endif

  auto state = get_stack_state(return_address);

  switch (state) {
    case StackState::ManagedUntracked:
      // If we interrupted in managed code, but it wasn't at a poll point,
      // we need to return back to the thread so it can hit a poll.


      // First, though, we need to wait for the patches to be done.
      // while (!patches_done) {
      // }

      for (auto [start, end] : managed_blob_text_regions) {
        __builtin___clear_cache((char*)start, (char*)end);
      }

      printf(
          "ManagedUntracked: pc:0x%zx sig:%d invl:%d!\n", return_address, sig, invalid_state_abort);
      if (sig == SIGILL) {
        alaska::thread_tracking::my_state.join_status = ALASKA_JOIN_REASON_ABORT;
        break;
      }
      if (not invalid_state_abort) {
        invalid_state_abort = true;
        return;
      }
      alaska::thread_tracking::my_state.join_status = ALASKA_JOIN_REASON_ABORT;
      invalid_state_abort = false;
      break;

    case StackState::ManagedTracked:
      // it's possible to be at a managed poll point *and* get interrupted
      // through SIGUSR2
      alaska::thread_tracking::my_state.join_status = ALASKA_JOIN_REASON_SAFEPOINT;
      break;

    case StackState::Unmanaged:
      assert(sig == SIGUSR2 && "Unmanaged code got into the barrier handler w/ the wrong signal");
      alaska::thread_tracking::my_state.join_status = ALASKA_JOIN_REASON_SIGNAL;
      break;
  }

  invalid_state_abort = false;

  ck::set<void*> ps;
  alaska::barrier::get_pinned_handles(ps);  // TODO: slow!

  // Simply join the barrier, then leave immediately. This
  // will deal with all the synchronization that needs done.
  participant_join(false, ps);

  participant_leave(false, ps);

  clear_pending_signals();

  for (auto [start, end] : managed_blob_text_regions) {
    __builtin___clear_cache((char*)start, (char*)end);
  }
}



static void setup_signal_handlers(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));

  // Block signals while we are in these signal handlers.
  sigemptyset(&sa.sa_mask);
  sigaddset(&sa.sa_mask, SIGILL);
  sigaddset(&sa.sa_mask, SIGUSR2);

  // Store siginfo (ucontext) on the stack of the signal
  // handlers (so we can grab the return address)
  sa.sa_flags = SA_SIGINFO;
  // Go to the `barrier_signal_handler`
  sa.sa_sigaction = alaska_barrier_signal_handler;
  // Attach this action on two signals:
  assert(sigaction(SIGILL, &sa, NULL) == 0);
  assert(sigaction(SIGUSR2, &sa, NULL) == 0);
}


static void clear_pending_signals(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));

  // Ignoring a signal while it is pending will clear it's pending status
  sa.sa_handler = SIG_IGN;
  // Attach this action on two signals:
  assert(sigaction(SIGILL, &sa, NULL) == 0);
  assert(sigaction(SIGUSR2, &sa, NULL) == 0);
  // Now, set them up again
  setup_signal_handlers();
}




// void alaska::barrier::add_self_thread(void) {
//   setup_signal_handlers();
//   alaska::thread_tracking::join();
//   alaska::thread_tracking::my_state.escaped = 0;
// }

// void alaska::barrier::remove_self_thread(void) { alaska::thread_tracking::leave(); }

/**
 * This function parses a stackmap emitted from LLVM and pushes all
 * the patch points to
 */
void parse_stack_map(uint8_t* t) {
  alaska::StackMapParser p(t);

  auto currFunc = p.functions_begin();
  size_t recordCount = 0;

  for (auto it = p.records_begin(); it != p.records_end(); it++) {
    auto record = *it;
    std::uintptr_t addr = currFunc->getFunctionAddress() + record.getInstructionOffset();
    if (++recordCount == currFunc->getRecordCount()) {
      currFunc++;
      recordCount = 0;
    }

    if (record.getID() == 'BLOK') {
      block_rets.add(addr);
    }

    if (record.getID() == 'PATC') {
      // Apply the instruction patch
      auto* rip = (void*)(addr - ALASKA_PATCH_SIZE);

      PatchPoint p;
      p.pc = (inst_t*)rip;


      // X86:
#ifdef __amd64__
      // Byte pointer to the instruction.
      p.inst_sig = 0x0B'0F;
      // A 2 byte nop
      p.inst_nop = 0x90'66;
#endif

// Arm is way easier than x86...
#ifdef __aarch64__
      p.inst_nop = 0xd503201f;  // nop
      p.inst_sig = 0x00000000;  // udf #0
#endif


#ifdef __riscv
      p.inst_nop = 0x0001;  // nop
      p.inst_sig = 0x0000;  // unimp
#endif

      patchPoints.push(p);
    }
    if (record.getID() == 'PATC') {
      addr -= ALASKA_PATCH_SIZE;
    }

    PinSetInfo psi;
    psi.count = 0;

    for (std::uint16_t i = 3; i < record.getNumLocations(); i++) {
      auto l = record.getLocation(i);

      switch (l.getKind()) {
        case alaska::StackMapParser::LocationKind::Direct:
          psi.regNum = l.getDwarfRegNum();
          psi.offset = l.getOffset();
          break;

        case alaska::StackMapParser::LocationKind::Constant:
          psi.count = l.getSmallConstant();
          break;

        default:
          break;
      }
    }


    if (record.getID() == 'HFLT') {
      printf("handle fault at 0x%lx lo:%d, loc:%d\n", addr, record.getNumLiveOuts(),
          record.getNumLocations());
      for (std::uint16_t i = 0; i < record.getNumLocations(); i++) {
        auto l = record.getLocation(i);

        switch (l.getKind()) {
          case alaska::StackMapParser::LocationKind::Direct: {
            auto regNum = l.getDwarfRegNum();
            auto offset = l.getOffset();
            printf(" direct: regnum = %d, offset = %d\n", regNum, offset);
            break;
          }

          case alaska::StackMapParser::LocationKind::Indirect: {
            auto regNum = l.getDwarfRegNum();
            auto offset = l.getOffset();
            printf(" indirect: regnum = %d, offset = %d\n", regNum, offset);
            break;
          }

          case alaska::StackMapParser::LocationKind::Constant: {
            auto constant = l.getSmallConstant();
            printf(" constant = %d\n", constant);
            break;
          }

          default:
            printf(" other\n");
            break;
        }
      }
    }

    if (record.getID() == 'PATC') {
      pin_map[addr] = psi;
      //       if (record.getID() == 'BLOK') {
      // #ifdef __amd64__
      //         pin_map[addr - 5] = psi;  // TODO(HACK)
      // #endif
      // #ifdef __aarch64__
      //         pin_map[addr - 4] = psi;
      // #endif
      //       }
    }
  }


  patchNop();
}



void alaska_blob_init(struct alaska_blob_config* cfg) {
  managed_blob_text_regions.push({cfg->code_start, cfg->code_end});

  // Not particularly safe, but we will ignore that for now.
  auto patch_page = (void*)((uintptr_t)cfg->code_start & ~0xFFF);
  size_t size = round_up(cfg->code_end - cfg->code_start, 4096);
  mprotect(patch_page, size + 4096, PROT_EXEC | PROT_READ | PROT_WRITE);

  if (cfg->stackmap) parse_stack_map((uint8_t*)cfg->stackmap);
}

// This function doesn't really need to exist,
extern "C" void alaska_barrier_poll(void) {
  abort();
  return;
}
