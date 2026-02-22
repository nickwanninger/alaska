// This code is shared between yukon and yukon_stub.
// It is here just to make sure the two systems stay in sync.
// The main thing it does it optimize the acces to the runtime and thread cache.


#define _GLIBCXX_INCLUDE_NEXT_C_HEADERS

#include <alaska/core/ThreadCache.hpp>
#include <alaska/core/Runtime.hpp>
#include <sys/ioctl.h>
#include <math.h>

#define CONSTRUCTOR __attribute__((constructor))
#define DESTRUCTOR __attribute__((destructor))


#define CSR_HTBASE 0xc2
#define CSR_HTDUMP 0xc3
#define CSR_HTINVAL 0xc4



#define write_csr(reg, val) \
  ({ asm volatile("csrw %0, %1" ::"i"(reg), "rK"((uint64_t)val) : "memory"); })


#define read_csr(csr, val) \
  __asm__ __volatile__("csrr %0, %1" : "=r"(val) : "n"(csr) : /* clobbers: none */);



#define INSTCOUNT_MALLOC 0
#define INSTCOUNT_CALLOC 1
#define INSTCOUNT_FREE 2
#define INSTCOUNT_REALLOC 3
#define INSTCOUNT_GETSIZE 4
static struct {
  const char *name;
  uint64_t count;
  uint64_t instructions;
} inst_counts[] = {
    [INSTCOUNT_MALLOC] = {"malloc", 0, 0},   [INSTCOUNT_CALLOC] = {"calloc", 0, 0},
    [INSTCOUNT_FREE] = {"free", 0, 0},       [INSTCOUNT_REALLOC] = {"realloc", 0, 0},
    [INSTCOUNT_GETSIZE] = {"getsize", 0, 0},
};


// #define INSTRUCTION_TRACKER(x) InstructionTracker inst_tracker(x)
#define INSTRUCTION_TRACKER(x)

static inline uint64_t read_instret() {
  uint64_t instret;
  asm volatile("rdinstret %0" : "=r"(instret));
  return instret;
}

static inline uint64_t read_cycles() {
  uint64_t cycles;
  asm volatile("rdcycle %0" : "=r"(cycles));
  return cycles;
}




static void CONSTRUCTOR print_inst_counts_init() {
  atexit([]() {
    for (size_t i = 0; i < sizeof(inst_counts) / sizeof(inst_counts[0]); i++) {
      uint64_t instructions = inst_counts[i].instructions;
      if (instructions == 0) continue;
      uint64_t count = inst_counts[i].count;
      double avg = (double)instructions / (double)count;
      printf("YUKON_INSTRUCTION_STAT=%s sum_inst:%zu, calls:%zu, avg:%lf\n", inst_counts[i].name,
             instructions, count, avg);
    }
  });
}


struct InstructionTracker {
  uint64_t start_inst;
  int dst_index;
  InstructionTracker(int dst_index)
      : dst_index(dst_index) {
    start_inst = read_instret();
  }
  ~InstructionTracker() {
    auto i = read_instret() - start_inst;
    inst_counts[dst_index].instructions += i;
    inst_counts[dst_index].count++;
  }
};




static inline alaska::Runtime *the_runtime;

__attribute__((noinline)) static void yukon_init_runtime_and_tc() {
  the_runtime = new alaska::Runtime();
}

static inline alaska::ThreadCache *yukon_get_tc_unchecked() {
  return alaska::ThreadCache::current();
}

static inline alaska::ThreadCache *yukon_get_tc() {
  if (unlikely(the_runtime == nullptr)) yukon_init_runtime_and_tc();
  return yukon_get_tc_unchecked();
}




static char stdout_buf[BUFSIZ];
static char stderr_buf[BUFSIZ];


// ------------------------- LOCALIZATION ------------------------- //

/* Used to chat w/ the kernel module to deliver timer interrupts for dumping */
struct yukon_schedule_arg {
  unsigned long handler_address;  // Address of the handler function
  unsigned long delay;            // Delay in microseconds before scheduling
};

#define YUKON_IOCTL_MAGIC 'y'
#define YUKON_IOCTL_SCHEDULE _IOR(YUKON_IOCTL_MAGIC, 0, struct yukon_schedule_arg *)
#define YUKON_IOCTL_RETURN _IO(YUKON_IOCTL_MAGIC, 1)

static volatile long localization_latch = 0;
static uint64_t yukon_mean_dump_interval = 100'000;  // measured in microseconds.

// This is true if localization is enabled in the system.
// There are a few ways to disble localization, such as environment variables and the user-facing
// `yukon_enable_localization` function. This is used independently of the below state machine.
static bool enable_localization = false;

enum LocalizationState {
  LS_DISABLED,
  LS_PENDING,
  LS_LOCALIZING,
  LS_DEFERRED,
  LS_SCHEDULING,
};
const char *localization_state_names[] = {"Disabled", "Pending", "Localizing", "Deferred",
                                          "Scheduling"};

static LocalizationState localization_state = LS_DISABLED;
static void localizer_state_transition(LocalizationState new_state) {
  // alaska::printf("YUKON: localization state transition: %10s -> %10s   depth=%3d\n",
  //     localization_state_names[localization_state], localization_state_names[new_state],
  //     localization_latch);

  localization_state = new_state;
}


static void yukon_dump_alarm_handler(int sig);

static unsigned int seed;

// Box-Muller transform to generate Gaussian random numbers
static float yukon_gaussian_random(float mean, float stddev) {
  // Generate two uniform random numbers
  float u1 = ((float)rand_r(&seed) + 1.0) / ((float)RAND_MAX + 1.0);
  float u2 = ((float)rand_r(&seed) + 1.0) / ((float)RAND_MAX + 1.0);

  // Box-Muller transformation
  float mag = stddev * sqrt(-2.0 * log(u1));
  return mag * sin(2.0 * M_PI * u2) + mean;
}

static void schedule_localization_interrupt(uint64_t interval_override = 0) {
  if (localization_state == LS_PENDING) {
    // alaska::printf("YUKON: localization already scheduled, not scheduling again.\n");
    localizer_state_transition(LS_PENDING);
    return;
  }

  if (!enable_localization) {
    localizer_state_transition(LS_DISABLED);
    return;
  }

  localizer_state_transition(LS_SCHEDULING);

  // uint64_t interval = interval_override != 0 ? interval_override
  //                                            : yukon_gaussian_random(yukon_mean_dump_interval,
  //                                                  yukon_mean_dump_interval / 3);

  uint64_t interval = interval_override != 0 ? interval_override : yukon_mean_dump_interval;

  // Apply a minimum interval for safety (or something)
  constexpr uint64_t min_interval = 20;
  if (interval < min_interval) interval = min_interval;


  // Mark this before we schedule the alarm to make sure it is set before the handler runs.
  localizer_state_transition(LS_PENDING);

  struct yukon_schedule_arg arg;
  arg.delay = interval;
  arg.handler_address = (unsigned long)yukon_dump_alarm_handler;
  long res = ioctl(alaska::HandleTable::get_ht_fd(), YUKON_IOCTL_SCHEDULE, &arg);


  if (res < 0) {
    printf("fd = %d\n", alaska::HandleTable::get_ht_fd());
    perror("Failed to schedule localization");
    exit(-1);
  }
}


constexpr size_t DUMP_SIZE = 16 + 512;

static int dump_htlb_into(alaska::ThreadCache *tc, alaska::handle_id_t *space) {
  auto fd = alaska::HandleTable::get_ht_fd();
  memset(space, 0, DUMP_SIZE * sizeof(alaska::handle_id_t));  // This memset is gross!
  int r = read(fd, space, DUMP_SIZE * sizeof(alaska::handle_id_t));
  return r;
}

static void dump_htlb(alaska::ThreadCache *tc) {
  // return;
  auto fd = alaska::HandleTable::get_ht_fd();
  auto &rt = alaska::Runtime::get();

  rt.with_barrier([&]() {
    alaska::printf("YUKON: Grading the heap...\n");
    auto grade = rt.grade_heap();

    auto total_pointers = grade.in_pointers + grade.out_pointers;
    alaska::printf("Total pointers graded: %zu\n", total_pointers);
    if (total_pointers > 0) {
      float locality = (float)grade.in_pointers / (float)total_pointers;
      alaska::printf("Locality = %f%% (%zu in, %zu out)\n", locality * 100.0f, grade.in_pointers,
                     grade.out_pointers);
      if (locality < 0.5f) {
        alaska::printf(
            "YUKON: Low locality detected (%.2f%%). Running brute-force localization...\n",
            locality * 100.0f);
        // rt.brute_force_localization(*tc);
      }
    }
    ::printf("YUKON: Done grading the heap.\n");
  });

  return;

  // auto *space = tc->localizer.get_hotness_buffer(DUMP_SIZE);
  alaska::handle_id_t space[DUMP_SIZE];
  dump_htlb_into(tc, space);
  tc->localize(space, DUMP_SIZE);
  // tc->localizer.feed_hotness_buffer(DUMP_SIZE, space);
}

// This method attempts to localize by feeding the localizer with new dump data.
// It returns true if anything was able to be done, and false if it should try again later.
__attribute__((noinline)) static bool attempt_localization(void) {
  localizer_state_transition(LS_LOCALIZING);
  if (localization_latch) {
    return false;
  }

  dump_htlb(yukon_get_tc());

  return true;
}


static int num_localization_interrupts = 0;
static uint64_t total_localization_cycles = 0;
static uint64_t start_cycles = 0;

static void yukon_dump_alarm_handler(int sig) {
  auto start = read_cycles();
  bool localized = attempt_localization();
  auto end = read_cycles();

  total_localization_cycles += (end - start);
  if (start_cycles == 0) {
    start_cycles = start;
  }


  num_localization_interrupts++;
  auto total_cycles = end - start_cycles;

  if (num_localization_interrupts == 25) {
    float percent = (float)total_localization_cycles / (float)total_cycles * 100.0f;
    alaska::printf("localization %f%% of runtime\n", percent);
    num_localization_interrupts = 0;
  }

  if (localized) {
    // Successfully localized! Schedule the next one!
    schedule_localization_interrupt();
  } else {
    // Oh no! We couldn't localize right now, so we need to defer the localization.
    localizer_state_transition(LS_DEFERRED);
  }
}



struct LocalizationLatch {
  LocalizationLatch() {
    localization_latch = 1;
    // localization_latch++;
  }
  ~LocalizationLatch() {
    localization_latch = 0;
    // localization_latch--;
    if (localization_state == LS_DEFERRED) {
      yukon_dump_alarm_handler(SIGPROF);
    }
  }
};



void CONSTRUCTOR alaska_init(void) {
  seed = rand();
  // Setup the output buffers for stdout and stderr so they don't invoke our allocator.
  // This is important because we want to avoid recursion in the allocator.
  setvbuf(stdout, stdout_buf, _IOLBF, BUFSIZ);
  setvbuf(stderr, stderr_buf, _IOLBF, BUFSIZ);

  // Unset LD_PRELOAD to avoid running alaska in subprocesses. (We haven't tested this yet.)
  unsetenv("LD_PRELOAD");

  // Make sure the runtime and thread cache are initialized.
  alaska::handle_id_t t[DUMP_SIZE];
  auto *tc = yukon_get_tc();
  dump_htlb_into(tc, t);
}


// Fwd Declare.
extern "C" void yukon_enable_localization(int enable);

static int in_roi = 0;
static uint64_t roi_start_insts = 0;
static uint64_t roi_start_cycles = 0;

extern "C" __attribute__((visibility("default"))) void yukon_change_roi(int roi_enabled) {
  if (roi_enabled) {
    alaska::printf("YUKON: entering ROI\n");

    yukon_enable_localization(true);

    roi_start_insts = read_instret();
    roi_start_cycles = read_cycles();

    alaska::printf("YUKON_START_CRITICAL=%zu\n", roi_start_cycles);
    in_roi = 1;

  } else {
    if (!in_roi) {
      return;
    }
    auto end_insts = read_instret();
    auto end_cycles = read_cycles();

    yukon_enable_localization(false);

    alaska::printf("YUKON_END_CRITICAL=%zu\n", end_cycles);
    alaska::printf("YUKON_ROI_CYCLES=%zu\n", end_cycles - roi_start_cycles);
    alaska::printf("YUKON_ROI_INSTRET=%zu\n", end_insts - roi_start_insts);
    in_roi = 0;
  }
}

// .. Then the rest of the code follows, after including this file.
