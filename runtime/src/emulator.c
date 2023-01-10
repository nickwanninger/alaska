#include <capstone/capstone.h>

#include <alaska.h>
#include <errno.h>
#include <malloc.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <unicorn/unicorn.h>
// TODO: Just hacking on this on arm for now, so it doesn't work on x86

#define sigsegv_outp(x, ...) fprintf(stderr, x "\n", ##__VA_ARGS__)
extern void* alaska_lock(void* ptr);
extern void alaska_unlock(void* ptr);

#ifdef ALASKA_CORRECTNESS_EMULATOR_LOGGING
static inline char byte_size_human(unsigned size) {
  switch (size) {
    case 8:
      return 'q';
    case 4:
      return 'w';
    case 2:
      return 'h';
    case 1:
      return 'b';
  }
  return '?';
}
#endif

static inline uint64_t alaska_emulate_load(void* addr, size_t size) {
  void* ptr = alaska_lock(addr);
  uint64_t val = 0;
#ifdef ALASKA_CORRECTNESS_EMULATOR_LOGGING
  if (true || addr != ptr) {
    fprintf(stderr, "alaska: ld%c %016zx(%p) -> ", byte_size_human(size), (off_t)addr, ptr);
    fflush(stdout);
  }
#endif
  switch (size) {
    case 8:
      val = *(uint64_t*)ptr;
      break;
    case 4:
      val = *(uint32_t*)ptr;
      break;
    case 2:
      val = *(uint16_t*)ptr;
      break;
    case 1:
      val = *(uint8_t*)ptr;
      break;
  }
#ifdef ALASKA_CORRECTNESS_EMULATOR_LOGGING
  if (true || addr != ptr) {
    fprintf(stderr, "%0*zx", size * 2, val);
    if (size == 1) fprintf(stderr, "  '%c'", val);
    fprintf(stderr, "\n");
  }
#endif

  alaska_unlock(addr);
  return val;
}

static inline void alaska_emulate_store(void* addr, uint64_t val, size_t size) {
  void* ptr = alaska_lock(addr);

#ifdef ALASKA_CORRECTNESS_EMULATOR_LOGGING
  if (true || addr != ptr) {
    fprintf(stderr, "alaska: st%c %016zx(%p) <- %0*zx\n", byte_size_human(size), (off_t)addr, ptr, size * 2, val);
  }
#endif
  switch (size) {
    case 8:
      *(uint64_t*)ptr = val;
      break;
    case 4:
      *(uint32_t*)ptr = val;
      break;
    case 2:
      *(uint16_t*)ptr = val;
      break;
    case 1:
      *(uint8_t*)ptr = val;
      break;
  }

  alaska_unlock(addr);
}

void uc_err_check(const char* name, uc_err err) {
  if (err) {
    fprintf(stderr, "Unicorn: %s failed with error returned: %s\n", name, uc_strerror(err));
    exit(EXIT_FAILURE);
  }
}
static __thread uc_engine* uc = NULL;
static __thread volatile ucontext_t* faulting_context = false;

__declspec(noinline) void alaska_real_segfault_handler(ucontext_t* ucontext) {
#ifdef __aarch64__
  fprintf(stderr, "segfault while performing a correctness emulation at pc:%p, fa:%p!\n", ucontext->uc_mcontext.pc,
      ucontext->uc_mcontext.fault_address);
#else
  fprintf(stderr, "segfault while performing a correctness emulation!\n");
#endif
  exit(-1);
}

#ifdef __aarch64__
#define UC_ARCH_CURRENT UC_ARCH_ARM64
#define UC_MODE_CURRENT UC_MODE_ARM

static void aarch64_emu_register_sync(ucontext_t* ucontext, uc_engine* uc, void* vsync) {
  uc_err (*uc_sync)(uc_engine*, int, void*);
  uc_sync = vsync;
  // Linux encodes the __reserved field in a variable length array of different
  // sized contexts. It contains the values of the floating point unit, ESR,
  // etc...
  uint8_t* res = (uint8_t*)ucontext->uc_mcontext.__reserved;
  for (int i = 0; i < 4096; i++) {
    uint32_t val = *(uint32_t*)&res[i];

    if (val == FPSIMD_MAGIC) {
      struct fpsimd_context* ctx = (struct fpsimd_context*)(res + i);
      for (int r = 0; r < 32; r++) {
        uc_sync(uc, UC_ARM64_REG_V0 + r, &ctx->vregs[r]);
      }
      i += ctx->head.size;
      continue;
    } else if (val == SVE_MAGIC) {
      // We don't really care about the `vl` register, as the emulator framework
      // doesn't seem to support changing it. For now, we will just skip it and
      // hope for the best :)
      struct sve_context* ctx = (struct sve_context*)(res + i);
      i += ctx->head.size;
    }
  }

  for (int i = 0; i < 31; i++)
    uc_sync(uc, UC_ARM64_REG_X0 + i, &ucontext->uc_mcontext.regs[i]);
  uc_sync(uc, UC_ARM64_REG_SP, &ucontext->uc_mcontext.sp);
  uc_sync(uc, UC_ARM64_REG_PC, &ucontext->uc_mcontext.pc);
}
static void emu_run(ucontext_t* ucontext) {
  // populate the registers
  off_t pc = ucontext->uc_mcontext.pc;
  aarch64_emu_register_sync(ucontext, uc, uc_reg_write);
  // Emulate a single instruction
  uc_err_check("uc_emu_start", uc_emu_start(uc, (uint64_t)pc, (uint64_t)pc + 1000, 0, 1));
  aarch64_emu_register_sync(ucontext, uc, uc_reg_read);
}
#elif defined(__x86_64__)

#define UC_ARCH_CURRENT UC_ARCH_X86
#define UC_MODE_CURRENT UC_MODE_64

static void amd64_emu_register_sync(ucontext_t* ucontext, uc_engine* uc, void* vsync) {
  uc_err (*uc_sync)(uc_engine*, int, void*);
  uc_sync = vsync;

  // UC_X86_REG_RIP;
#define DO_SYNC2(reg1, reg2) uc_sync(uc, UC_X86_REG_##reg1, &ucontext->uc_mcontext.gregs[REG_##reg2])
#define DO_SYNC(reg) DO_SYNC2(reg, reg)
	DO_SYNC(RIP);
	DO_SYNC(RSP);

  DO_SYNC(R8);
  DO_SYNC(R9);
  DO_SYNC(R10);
  DO_SYNC(R11);
  DO_SYNC(R12);
  DO_SYNC(R13);
  DO_SYNC(R14);
  DO_SYNC(R15);
  DO_SYNC(RDI);
  DO_SYNC(RSI);
  DO_SYNC(RBP);
  DO_SYNC(RBX);
  DO_SYNC(RDX);
  DO_SYNC(RAX);
  DO_SYNC(RCX);
  DO_SYNC(RSP);
  DO_SYNC(RIP);
  DO_SYNC2(EFLAGS, EFL);

  // for (int i = 0; i < 31; i++)
  //   uc_sync(uc, UC_ARM64_REG_X0 + i, &ucontext->uc_mcontext.regs[i]);
  // uc_sync(uc, UC_ARM64_REG_SP, &ucontext->uc_mcontext.sp);
  // uc_sync(uc, UC_ARM64_REG_PC, &ucontext->uc_mcontext.pc);
}

static void emu_run(ucontext_t* ucontext) {
  off_t pc = ucontext->uc_mcontext.gregs[REG_RIP];
  amd64_emu_register_sync(ucontext, uc, uc_reg_write);
  // Emulate a single instruction
  uc_err_check("uc_emu_start", uc_emu_start(uc, (uint64_t)pc, (uint64_t)pc + 1000, 0, 1));
  amd64_emu_register_sync(ucontext, uc, uc_reg_read);
}
#else
#error hmm
#endif

void alaska_sigsegv_handler(int sig, siginfo_t* info, void* ptr) {
  // allow segfaults to be delivered within the emulator
  sigset_t set, old;
  sigfillset(&set);
  sigprocmask(SIG_UNBLOCK, &set, &old);

  ucontext_t* ucontext = (ucontext_t*)ptr;

  if (faulting_context != NULL) {
    alaska_real_segfault_handler((ucontext_t*)faulting_context);
    exit(-1);
  }
  faulting_context = ucontext;

  if (uc == NULL) {
    uc_err_check("uc_open", uc_open(UC_ARCH_CURRENT, UC_MODE_CURRENT, &uc));
    uc_alaska_set_memory_emulation(uc, alaska_emulate_load, alaska_emulate_store);
  }

  emu_run(ucontext);

  faulting_context = NULL;

  // sigprocmask(SIG_SETMASK, &old, NULL);
}

static void __attribute__((constructor)) alaska_fallback_init(void) {
  // stack_t ss;
  //
  // ss.ss_sp = malloc(SIGSTKSZ);
  // if (ss.ss_sp == NULL) {
  //   perror("malloc");
  //   exit(EXIT_FAILURE);
  // }
  //
  // ss.ss_size = SIGSTKSZ;
  // ss.ss_flags = 0;
  // if (sigaltstack(&ss, NULL) == -1) {
  //   perror("sigaltstack");
  //   exit(EXIT_FAILURE);
  // }
  // printf("%p\n", ss.ss_sp);

  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = alaska_sigsegv_handler;
  sigaction(SIGSEGV, &sa, NULL);
}

// #endif
