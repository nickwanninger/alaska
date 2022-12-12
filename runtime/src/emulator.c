#include <capstone/capstone.h>

#include <alaska.h>
#include <signal.h>
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <unicorn/unicorn.h>
// Just hacking on this on arm for now
// #ifdef __aarch64__

#define sigsegv_outp(x, ...) fprintf(stderr, x "\n", ##__VA_ARGS__)


extern void *alaska_lock(void *ptr);
extern void alaska_unlock(void *ptr);

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

static inline uint64_t alaska_emulate_load(void *addr, size_t size) {
  void *ptr = alaska_lock(addr);
  uint64_t val = 0;
  switch (size) {
    case 8:
      val = *(uint64_t *)ptr;
      break;
    case 4:
      val = *(uint32_t *)ptr;
      break;
    case 2:
      val = *(uint16_t *)ptr;
      break;
    case 1:
      val = *(uint8_t *)ptr;
      break;
  }
#ifdef ALASKA_CORRECTNESS_EMULATOR_LOGGING
  if (addr != ptr) {
    printf("alaska: emulate ld%c %016zx -> %0*zx", byte_size_human(size), (off_t)addr, size * 2, val);
    if (size == 1) printf("  '%c'", val);
    printf("\n");
  }
  #endif

  alaska_unlock(addr);
  return val;
}

static inline void alaska_emulate_store(void *addr, uint64_t val, size_t size) {
  void *ptr = alaska_lock(addr);
  
#ifdef ALASKA_CORRECTNESS_EMULATOR_LOGGING
  if (addr != ptr){
    printf("alaska: cemulate st%c %016zx <- %0*zx\n", byte_size_human(size), (off_t)addr, size * 2, val);
  }
#endif
  switch (size) {
    case 8:
      *(uint64_t *)ptr = val;
      break;
    case 4:
      *(uint32_t *)ptr = val;
      break;
    case 2:
      *(uint16_t *)ptr = val;
      break;
    case 1:
      *(uint8_t *)ptr = val;
      break;
  }

  alaska_unlock(addr);
}


void uc_err_check(const char *name, uc_err err) {
  if (err) {
    fprintf(stderr, "Unicorn: %s failed with error returned: %s\n", name, uc_strerror(err));
    exit(EXIT_FAILURE);
  }
}
__thread uc_engine *uc = NULL;
__thread bool fault_handling = false;

void alaska_sigsegv_handler(int sig, siginfo_t *info, void *ptr) {
  ucontext_t *ucontext = (ucontext_t *)ptr;

  if (fault_handling == true) {
    fprintf(stderr, "segfault while performing a correctness emulation at pc:%p, fa:%p!\n", ucontext->uc_mcontext.pc,
        ucontext->uc_mcontext.fault_address);
  }
  fault_handling = true;
  // fprintf(stderr, "correctness emulation at pc:%p, fa:%p!\n", ucontext->uc_mcontext.pc, ucontext->uc_mcontext.fault_address);


  if (uc == NULL) {
    uc_err_check("uc_open", uc_open(UC_ARCH_ARM64, UC_MODE_ARM, &uc));
    uc_alaska_set_memory_emulation(uc, alaska_emulate_load, alaska_emulate_store);
  }

  // populate the registers
  off_t pc = ucontext->uc_mcontext.pc;
  for (int i = 0; i < 31; i++) {
    uc_reg_write(uc, UC_ARM64_REG_X0 + i, &ucontext->uc_mcontext.regs[i]);
  }
  uc_reg_write(uc, UC_ARM64_REG_SP, &ucontext->uc_mcontext.sp);

  uc_err_check("uc_emu_start", uc_emu_start(uc, (uint64_t)pc, (uint64_t)pc + 1000, 0, 1));


  for (int i = 0; i < 31; i++) {
    uc_reg_read(uc, UC_ARM64_REG_X0 + i, &ucontext->uc_mcontext.regs[i]);
  }
  uc_reg_read(uc, UC_ARM64_REG_SP, &ucontext->uc_mcontext.sp);
  uc_reg_read(uc, UC_ARM64_REG_PC, &ucontext->uc_mcontext.pc);

  fault_handling = false;
}


static void __attribute__((constructor)) alaska_fallback_init(void) {
  printf("init!\n");
  struct sigaction sa;

  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = alaska_sigsegv_handler;
  sigaction(SIGSEGV, &sa, NULL);
}

// #endif
