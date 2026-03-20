#include <filesystem>
#include <new>
#include <alaska/core/Runtime.hpp>
#include <alaska/core/ThreadCache.hpp>
#include <sim/HTLB.hpp>
#include <stdlib.h>
#include <stdio.h>
#include <unordered_map>
#include <vector>
#include "alaska.h"
#include "alaska/handles/ObjectHeader.hpp"
#include "alaska/util/RateCounter.hpp"
#include "alaska/alaska.hpp"
#include "alaska/util/utils.h"
#include <sys/wait.h>
#include <getopt.h>
#include <math.h>

struct SimKnobs {
  float effort = 1.0;
  long dump_interval_us = 100;
  long localization_interval = 20;
  long localization_depth = 0;
  long hotness_cutoff = 1;
  bool relocalize = false;
  float relocalize_ratio = 0.5;
};

enum class Event : uint8_t {
  ALLOC,
  FREE,
  ACCESS,
};

struct TraceEvent {
  uint64_t cycles : 56;  // cycle count at this event
  Event type : 8;
  uint64_t handle_id;
  // misc data. In ALLOC, this is the size. in ACCESS, this is the offset.
  uint32_t misc;
};

// Generate exponentially distributed random number with the given mean
// result is in us for use with itimer
static uint64_t exp_rand(uint64_t mean_us) {
  return mean_us;
  double u = drand48();

  // u = [0,1), uniform random, now convert to exponential
  u = -log(1.0 - u) * ((double)mean_us);

  // now shape u back into a uint64_t and return
  uint64_t ret = 0;
  if (u > ((double)(-1ULL))) {
    ret = -1ULL;
  } else {
    ret = (uint64_t)u;
  }

  // corner case
  if (ret == 0) {
    ret = 1;
  }

  return ret;
}



static void ensure_traceb(std::string path) {
  std::string binary_path = path + ".binary";
  // it already exists!
  if (std::filesystem::exists(binary_path)) {
    return;
  }

  // otherwise, we gotta make it.
  std::vector<TraceEvent> events;
  char command[256];
  char line[256];
  unsigned long start, end;

  start = alaska_timestamp();
  FILE *f = fopen(path.c_str(), "r");

  // now we need to process the events to determine the size of each allocation
  struct AllocationInfo {
    // maximum access offset in bytes.
    uint64_t rewritten_hid;
    uint32_t size;
    // the index in the events array where the allocation was made.
    uint64_t index;
  };
  uint64_t max_hid = 0;
  uint64_t next_hid = 1;
  std::unordered_map<uint32_t, AllocationInfo> allocs;




  auto file_size = std::filesystem::file_size(path);
  uint64_t line_num = 0;
  while (fgets(line, sizeof(line), f)) {
    line_num++;
    if (line[0] == '#') continue;  // skip comments

    if (line_num % 100000 == 0) {
      float byte_progress = (float)ftell(f) / (float)file_size;
      printf("processed %zu lines (%f%%)\n", line_num, byte_progress * 100.0f);
    }


    unsigned long cycles;
    sscanf(line, "  %lu %s\n", &cycles, command);

    TraceEvent event;
    event.cycles = cycles;

    switch (command[0]) {
      case 'a': {
        unsigned long hid;
        sscanf(command, "a%lx", &hid);

        AllocationInfo info;
        info.rewritten_hid = next_hid++;
        info.size = 8;  // minimum size of 8 bytes for an object.
        info.index = events.size();
        allocs[hid] = info;

        // printf("%016lx alloc %zx\n", cycles, hid);

        event.type = Event::ALLOC;
        event.handle_id = info.rewritten_hid;
        event.misc = 0;  // we don't know the size yet!
        break;
      }

      case 'h': {
        unsigned long addr;
        sscanf(command, "h%lx", &addr);
        auto hid = (addr & ~(1UL << 63)) >> ALASKA_SIZE_BITS;
        int32_t offset = addr & ((1LU << ALASKA_SIZE_BITS) - 1);

        // validate the offset is valid (handles aren't bigger than 4k
        // on a yukon core.
        if (offset < 0 || offset > 0xffff) {
          continue;
        }

        auto it = allocs.find(hid);
        // if there isn't an alloc for this hid, ignore the access
        if (it == allocs.end()) continue;
        auto &info = it->second;

        event.type = Event::ACCESS;
        event.handle_id = info.rewritten_hid;
        // the size required to access this offset with a 8 byte load
        uint32_t needed_size = round_up(offset + 8, 8);
        event.misc = offset;
        info.size = std::max(info.size, needed_size);
        break;
      }

      case 'f': {
        unsigned long hid;
        sscanf(command, "f%lx", &hid);
        auto it = allocs.find(hid);
        // printf("%016lx free %zx\n", cycles, hid);
        // if there isn't an alloc for this hid, ignore the access
        ALASKA_ASSERT(it != allocs.end(), "All frees must have an alloc");
        auto &info = it->second;

        event.type = Event::FREE;
        event.handle_id = info.rewritten_hid;
        event.misc = 0;
        // rewrite the size in the allocation event
        auto &alloc_event = events[info.index];
        alloc_event.misc = info.size;
        // remove the alloc from the map
        allocs.erase(hid);
        break;
      }
      default: {
        printf("unknown command %s\n", command);
        exit(-1);
        break;
      }
    }
    events.push_back(event);
  }

  // now go through the remaining allocs in the set that were not freed
  for (auto &pair : allocs) {
    auto &info = pair.second;
    auto &event = events[info.index];
    event.misc = info.size;
  }

  fclose(f);
  end = alaska_timestamp();

  printf("read %zu events in %lums\n", events.size(), (end - start) / 1000 / 1000);


  FILE *fout = fopen(binary_path.c_str(), "w");
  for (auto &event : events) {
    fwrite(&event, sizeof(TraceEvent), 1, fout);
  }
  fclose(fout);
  return;
}



class TraceRunner {
 protected:
  alaska::Runtime rt;
  alaska::ThreadCache *tc;
  // a mapping from logical handle id to the mapping object in the simulated runtime.
  std::unordered_map<uint64_t, void *> mappings;
  alaska::RateCounter event_counter;
  alaska::RateCounter cycles_handled;
  alaska::RateCounter allocations;
  alaska::RateCounter accesses;

  bool has_timeout = false;
  int64_t timeout_cycles = 0;

  float time_seconds = 0;
  FILE *event_stream;
  size_t num_events = 0;

  uint64_t start_cycle = 0;



 public:
  std::string run_name = "sim";

  TraceRunner(std::string tracefile_path) {
    if (!tracefile_path.ends_with(".binary")) {
      ensure_traceb(tracefile_path);
      tracefile_path = tracefile_path + ".binary";
    }
    event_stream = fopen(tracefile_path.c_str(), "r");
    num_events = std::filesystem::file_size(tracefile_path) / sizeof(TraceEvent);
    tc = rt.new_threadcache();
  }
  virtual ~TraceRunner() { fclose(event_stream); }

  // called after allocation/free
  virtual void on_alloc(uint64_t cycle, alaska::Mapping *m) {}
  virtual void on_free(uint64_t cycle, alaska::Mapping *m) {}
  // called when an access is made.
  virtual void on_access(uint64_t cycle, alaska::Mapping *m, uint32_t offset) {}
  virtual void on_timer(uint64_t cycle) {}

  // set a timer for the next event in microseconds (assuming 1ghz sim like in firesim)
  void set_timer(uint64_t microseconds) {
    has_timeout = true;
    timeout_cycles = microseconds * 1000;
  }



  void process_event(TraceEvent &event) {
    switch (event.type) {
      case Event::ALLOC: {
        allocations++;
        size_t size = event.misc;
        if (size < 16) size = 16;
        auto p = tc->halloc(size);
        mappings[event.handle_id] = p;
        if (auto *m = alaska::Mapping::from_handle_safe(mappings[event.handle_id])) {
          on_alloc(event.cycles, m);
        }
        break;
      }
      case Event::FREE: {
        if (auto *m = alaska::Mapping::from_handle_safe(mappings[event.handle_id])) {
          on_free(event.cycles, m);
        }
        // tc->hfree(mappings[event.handle_id]);
        mappings.erase(event.handle_id);
        break;
      }
      case Event::ACCESS: {
        accesses++;
        if (auto *m = alaska::Mapping::from_handle_safe(mappings[event.handle_id])) {
          on_access(event.cycles, m, event.misc);
        }
        break;
      }
    }
  }


  // step the simulation forward by one batch of events.
  long step(size_t batch_size, TraceEvent *events) {
    size_t batch_count = num_events / batch_size;
    uint64_t last_cycles = 0;
    // printf("[%s] reading...\n", run_name.c_str());
    size_t count = fread(events, sizeof(TraceEvent), batch_size, event_stream);
    // printf("[%s] read %zu\n", run_name.c_str(), count);

    // If we didn't read any events, we are done. Return 0.
    if (count == 0) return 0;
    // If we don't know the start_cycle of the
    if (unlikely(start_cycle == 0)) start_cycle = events[0].cycles;

    uint64_t sim_time_ns = events[count - 1].cycles - start_cycle;
    float sim_time_seconds = (float)sim_time_ns / 1e9f;
    time_seconds = sim_time_seconds;

    for (size_t i = 0; i < count; i++) {
      event_counter++;

      uint64_t current_cycle = events[i].cycles - start_cycle;
      if (last_cycles == 0) last_cycles = current_cycle;

      auto cycles_passed = current_cycle - last_cycles;
      cycles_handled.track(cycles_passed);

      process_event(events[i]);
      last_cycles = current_cycle;

      if (has_timeout) {
        timeout_cycles -= cycles_passed;
        if (timeout_cycles <= 0) {
          has_timeout = false;
          on_timer(current_cycle);
        }
      }
    }
    setlocale(LC_ALL, "");


    // how far are we along in the trace file?
    double progress = (double)(ftell(event_stream) / sizeof(TraceEvent)) / (double)num_events;
    double events_per_second = event_counter.digest();

    double percent_per_second = events_per_second / num_events;
    double remaining_percent = 1.0 - progress;
    double time_remaining_seconds = remaining_percent / percent_per_second;

    printf("[%s %12fs %6.2f%%] %'10ld ev/s  | ~%6lds remaining\n", run_name.c_str(), time_seconds,
           progress * 100.0, (long)events_per_second, (long)time_remaining_seconds);

    // float progress = printf(
    //     "[%16.8fs %3.0f%%] processed %15zu events %15.0f/s, %12.0fcyc/s, alloc:%8.0f/s, "
    //     "acc:%8.0f/s\n",
    //     sim_time_seconds, progress * 100.0f, (batch + 1) * batch_size, event_counter.digest(),
    //     cycles_handled.digest(), allocations.digest(), accesses.digest());
    return count;
  }


  void run(void) {
    size_t batch_size = 1'000'000;
    auto *events = new TraceEvent[batch_size];
    while (step(batch_size, events) != 0) {
    }
    delete[] events;
  }
};




/////////////////////////////////




constexpr float cpi = 2.89;

class HTLBTraceRunner : public TraceRunner {
 public:
  alaska::sim::HTLB htlb;
  bool simulating = false;
  uint64_t last_dump_cycle = 0;
  uint64_t us_since_last_dump = 0;

  bool do_localization = true;

  SimKnobs knobs;
  size_t localized_objects = 0;

  FILE *hitrate_file;  // file where csv hitrate output goes
  FILE *frag_file;     // file where csv fragmentation goes


  uint64_t ns_sim_skew = 0;

  HTLBTraceRunner(std::string path)
      : TraceRunner(path) {
    htlb.thread_cache = tc;
  }

  virtual ~HTLBTraceRunner() { fclose(hitrate_file); }


  int miss_count = 0;
  void on_access(uint64_t cycle, alaska::Mapping *m, uint32_t offset) override {
    if (simulating) {
      ns_sim_skew += htlb.access(*m, offset);
    }
  }
  void on_free(uint64_t cycle, alaska::Mapping *m) override {
    if (simulating) htlb.invalidate(m->handle_id());
  }




  size_t min_misses = -1;
  size_t max_misses = 0;
  float miss_frac = 0;


  void maybe_output_state(uint64_t cycle) {
    if (last_dump_cycle == 0) last_dump_cycle = cycle;

    // Compute the number of cycles since the last dump - we want this for MPKI approx.
    uint64_t cycles_passed = cycle - last_dump_cycle;
    last_dump_cycle = cycle;

    size_t misses = htlb.tlb.l2.misses;

    if (misses < min_misses) min_misses = misses;
    if (misses > max_misses) max_misses = misses;
    miss_frac = (float)(misses - min_misses) / (float)(max_misses - min_misses);

    if (hitrate_file != NULL) {
      fprintf(hitrate_file, "%zu,", cycle);
      fprintf(hitrate_file, "%zu,", htlb.tlb.l1.hits);
      fprintf(hitrate_file, "%zu,", htlb.tlb.l1.misses);
      fprintf(hitrate_file, "%zu,", htlb.tlb.l1.hits);
      fprintf(hitrate_file, "%zu,", htlb.tlb.l2.misses);
      fprintf(hitrate_file, "%f,", miss_frac);
      fprintf(hitrate_file, "%zu", localized_objects);
      fprintf(hitrate_file, "\n");
      fflush(hitrate_file);
      htlb.reset();
      // localized_objects = 0;
    }


    // if (frag_file != NULL) {
    //   // dump fragmentation info!
    //   auto &table = rt.heap.get_page_table();
    //   int id = 0;
    //   for (auto *heap : table) {
    //     fprintf(frag_file, "%zu,%s-%d,%f\n", cycle, heap->name, id, heap->fragmentation());
    //     id++;
    //   }
    //   fflush(frag_file);
    // }
  }

  bool cleared = false;

  void on_timer(uint64_t cycle) override {
    float sim_time_seconds = (float)(cycle + ns_sim_skew) / 1e9f;
    if (simulating and do_localization and sim_time_seconds > 0) {
      knobs.effort = miss_frac;

      ns_sim_skew += htlb.localize();
    }

    us_since_last_dump += knobs.dump_interval_us;
    if (us_since_last_dump > 10 * 1000) {
      us_since_last_dump = 0;

      maybe_output_state(cycle + ns_sim_skew);
      htlb.reset();
    }

    auto interval = exp_rand(knobs.localization_interval);

    set_timer(interval);
  }

  void run_sim(std::string output_dir) {
    // make sure that dir exists (mkdir -p behavior)
    std::filesystem::create_directories(output_dir);
    std::string hitrate_path = output_dir + "/hitrate.csv";
    hitrate_file = fopen(hitrate_path.c_str(), "w");
    fprintf(hitrate_file, "cycle,l1hit,l1miss,l2hit,l2miss,quality,localized_objects\n");

    std::string frag_path = output_dir + "/frag.csv";
    frag_file = fopen(frag_path.c_str(), "w");
    fprintf(frag_file, "cycle,id,frag\n");


    printf("running simulation in %s\n", output_dir.c_str());
    simulating = true;

    // schedule the first dump timer.
    set_timer(knobs.dump_interval_us);

    size_t batch_size = 1'000'000;
    auto *events = new TraceEvent[batch_size];

    // run the simulation until we hit the end of the trace.
    while (step(batch_size, events) != 0) {
    }

    delete[] events;
  }


  void run_sweep(float branch_time, int num_children = 16) {
    // This function sweeps the configuration space of the Localizer knobs,
    // starting from a given branch time in seconds.  It will call fork() on the
    // simulation for num_children times, and each child will run the simulation
    // with a different set of knobs.

    bool done = false;
    size_t batch_size = 1'000'000;
    auto *events = new TraceEvent[batch_size];
    do {
      if (step(batch_size, events) == 0) {
        done = true;
        break;
      }
    } while (time_seconds < branch_time);
    delete[] events;

    if (done) {
      printf("done before branch time!\n");
      return;
    }

    run_sim("htlb_sim/results/" + run_name);
  }
};


void usage(void) {
  fprintf(stderr, "Usage: HTLBTraceRunner [-r run_name] [-t tracefile] [-s sweep_start]\n");
  fprintf(stderr, "  -r run_name: name of the run. This is used to name the output files.\n");
  fprintf(stderr, "  -t tracefile: path to the trace file. This is a text file with the trace.\n");
  fprintf(stderr, "  -s sweep_start: start time for the sweep in seconds.\n");
  fprintf(stderr, "  -b: baseline (disable localization)\n");
  fprintf(stderr, "  -b knob=value: adjust knobs (value must be int or float)\n");
}


void adjust_knobs(SimKnobs &knobs, std::string knob_name, std::string value) {
  printf("adjust knob %s to %s\n", knob_name.c_str(), value.c_str());

  std::vector<const char *> allowed;

  long int_value = strtol(value.c_str(), NULL, 10);
  float float_value = strtof(value.c_str(), NULL);

#define KNOB(name, value)                   \
  if (knob_name == #name) {                 \
    knobs.name = value;                     \
    return;                                 \
  } else {                                  \
    allowed.push_back(#name " :: " #value); \
  }

  KNOB(dump_interval_us, int_value);
  KNOB(localization_interval, int_value);
  KNOB(localization_depth, int_value);
  KNOB(hotness_cutoff, int_value);
  KNOB(relocalize, int_value);
  KNOB(relocalize_ratio, float_value);

#undef KNOB


  fprintf(stderr, "Unknown knob %s\n", knob_name.c_str());
  fprintf(stderr, "Allowed knobs are:\n");
  for (auto *knob : allowed) {
    fprintf(stderr, "  %s\n", knob);
  }
  exit(-1);
}

int main(int argc, char **argv) {
  char *run_name = NULL;
  char *tracefile = NULL;
  bool sweep_agressiveness = false;
  float sweep_start = 0;
  bool do_localize = true;

  SimKnobs knobs;

  // Getopt for those values above
  int opt;


  while ((opt = getopt(argc, argv, "br:t:s:k:")) != -1) {
    switch (opt) {
      case 'k': {
        // the optarg is a key=value
        // split it on the =
        std::string arg = optarg;
        size_t pos = arg.find('=');
        if (pos == std::string::npos) {
          fprintf(stderr, "Invalid knob format: %s\n", optarg);
          exit(EXIT_FAILURE);
        }
        std::string key = arg.substr(0, pos);
        std::string value = arg.substr(pos + 1);
        adjust_knobs(knobs, key, value);
        break;
      }

      case 'b':
        do_localize = false;
        break;
      case 'r':
        run_name = optarg;
        break;
      case 't':
        tracefile = optarg;
        break;
      case 's':
        sweep_agressiveness = true;
        sweep_start = atof(optarg);
        break;
      default:
        fprintf(stderr, "Usage: %s [-r run_name] [-t tracefile] [-s sweep_start]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
  }
  if (tracefile == NULL || run_name == NULL) {
    usage();
    exit(EXIT_FAILURE);
  }

  unsigned long start, end;

  std::string output_dir = std::string("htlb_sim/results/") + run_name;

  HTLBTraceRunner runner(tracefile);
  runner.run_name = run_name;
  runner.knobs = knobs;
  runner.do_localization = do_localize;
  runner.run_sweep(sweep_start, 1);

  return 0;
}
