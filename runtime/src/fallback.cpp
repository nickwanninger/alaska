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


#define sigsegv_outp(x, ...) fprintf(stderr, x "\n", ##__VA_ARGS__)


void alaska_sigsegv_handler(int sig, siginfo_t *info, void *ptr) {
  ucontext_t *ucontext = (ucontext_t *)ptr;
  sigsegv_outp("SEGFAULT: pc = %llx %llx", ucontext->uc_mcontext.pc, ucontext->uc_mcontext.sp);

  off_t pc = ucontext->uc_mcontext.pc;

  csh handle;
  cs_insn *insn;
  size_t count;

  if (cs_open(CS_ARCH_ARM64, CS_MODE_ARM, &handle) != CS_ERR_OK) {
		fprintf(stderr, "dead!\n");
    exit(-1);
  }
  count = cs_disasm(handle, (uint8_t *)pc, 100, pc, 0, &insn);
  if (count > 0) {
    printf("0x%lx:%d:\t%s\t\t%s\n", insn[0].address, insn[0].size, insn[0].mnemonic, insn[0].op_str);

    cs_free(insn, count);
  } else
    printf("ERROR: Failed to disassemble given code!\n");

  cs_close(&handle);
  // for (int i = 0; i < 32; i++)
  //   sigsegv_outp("reg[%02d]       = 0x%llx", i, ucontext->uc_mcontext.regs[i]);
  exit(-1);
}


static int *ptr = NULL;

static void __attribute__((constructor)) alaska_init(void) {
  struct sigaction sa;

  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = alaska_sigsegv_handler;
  int rc = sigaction(SIGSEGV, &sa, NULL);
  if (rc != 0) {
    fprintf(stderr, "sigaction: %s\n", strerror(errno));
    // exit(EXIT_FAILURE);
  }
  *ptr = 0;
}
