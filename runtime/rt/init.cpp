/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2024, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2024, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */


// This file contains the initialization and deinitialization functions for the
// alaska::Runtime instance, as well as some other bookkeeping logic.

#include <alaska/core/Runtime.hpp>
#include <alaska/alaska.hpp>
#include <rt/barrier.hpp>
#include <pthread.h>
#include <stdio.h>
#include <ck/queue.h>
#include <alaska/util/RateCounter.hpp>

static alaska::Runtime *the_runtime = nullptr;


struct CompilerRuntimeBarrierManager : public alaska::BarrierManager {
  ~CompilerRuntimeBarrierManager() override = default;
  bool begin(void) override { return alaska::barrier::begin(); }
  void end(void) override { alaska::barrier::end(); }
};

static CompilerRuntimeBarrierManager the_barrier_manager;

static pthread_t barrier_thread;
static void *barrier_thread_func(void *) {
  bool in_marking_state = true;


  // FILE *log = fopen("heaps.csv", "w");
  // fprintf(log, "timestamp,heapid,last_use_ms\n");

  auto boot_time = alaska::now_ms();

  float toWait = 0.125;  // Seconds to wait before the first barrier.
  while (1) {
    auto &rt = alaska::Runtime::get();
    useconds_t sleep_time = (useconds_t)(toWait * 1000000);
    usleep(sleep_time);

    float total_fragmentation = 0.0f;
    uint64_t old_heaps = 0;
    uint64_t total_heaps = 0;
    uint64_t young_heaps = 0;
    auto start = alaska_timestamp();

    rt.with_barrier([&]() {
      // auto now = alaska::now_ms();
      // auto timestamp = now - boot_time;

      // int i = 0;
      // uintptr_t heap_start = (uintptr_t)rt.heap.base_pointer();
      // auto old_cutoff = now - (sleep_time / 2) / 1000;

      // rt.heap.for_each_page([&](alaska::HeapPage *page) {
      //   uintptr_t page_addr = (uintptr_t)page->start();
      //   uintptr_t page_index = (page_addr - heap_start) / alaska::page_size;
      //   fprintf(log, "%lu,%lu,%lu\n", timestamp, page_index, now - page->time_of_last_use);
      //   total_heaps++;
      //   if (page->time_of_last_use > old_cutoff) {
      //     young_heaps++;
      //   }
      //   // fprintf(log, "%lu,%lu,%lu\n", timestamp, page_index, page->age_resets);
      //   // fprintf(log, "%lu,%lu,%lu\n", timestamp, page_index, page->available());
      // });

      // FILE *arena_log = fopen("arenas", "w");
      // rt.arena_heap.dump(arena_log);
      // fclose(arena_log);


      // for (auto *page : rt.heap.get_aging_pages()) {
      //   if (page->time_of_last_use > old_cutoff) break;

      //   total_fragmentation += page->fragmentation();
      //   old_heaps++;
      //   rt.heap.promote_to_elderly(*page);
      // }

      toWait = rt.scheduler.tick(toWait);
      // fflush(log);
    });


    auto end = alaska_timestamp();
    auto duration_ns = end - start;
    alaska::printf("Barrier took %10.2fms o:%lu y:%lu t:%lu\n", duration_ns / 1e6, old_heaps, young_heaps, total_heaps);
  }

  return NULL;
}


#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static pthread_t cmd_thread;
static void *cmd_thread_function(void *arg) {
  int port = (int)(intptr_t)arg;
  int sockfd;
  struct sockaddr_in servaddr, cliaddr;

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket creation failed");
    return NULL;
  }

  memset(&servaddr, 0, sizeof(servaddr));
  memset(&cliaddr, 0, sizeof(cliaddr));

  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  servaddr.sin_port = htons(port);

  if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    perror("bind failed");
    return NULL;
  }
  printf("Alaska Runtime listening on 127.0.0.1:%d\n", port);

  char buffer[1024];
  while (true) {
    socklen_t len = sizeof(cliaddr);
    int n = recvfrom(sockfd, (char *)buffer, 1024, MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);
    if (n < 0) {
      perror("recvfrom");
      continue;
    }

    while (n > 0 && (buffer[n - 1] == '\n' || buffer[n - 1] == '\r')) {
      n--;
    }

    buffer[n] = '\0';
    // hexdump the buffer
    for (int i = 0; i < n; i++) {
      printf("%02x ", buffer[i]);
    }
    printf("\n");

    // printf("Client : %s\n", buffer);
    //
    if (strcmp(buffer, "dump") == 0) {
      auto &rt = alaska::Runtime::get();
      bool ran = rt.with_barrier([&]() {
        auto &ht = rt.handle_table;
        FILE *out = fopen("dump.bin", "wb");
        ht.for_each_handle([&](alaska::Mapping *h) {
          uint64_t hid = h->handle_id();
          auto header = alaska::ObjectHeader::from(h);
          uint64_t size = header->object_size();
          fwrite(&hid, sizeof(hid), 1, out);
          fwrite(&size, sizeof(size), 1, out);
          fwrite(header->data(), size, 1, out);
        });
        fclose(out);
        return;
      });

      if (ran) {
        const char *response = "success\n";
        sendto(sockfd, response, strlen(response), MSG_CONFIRM, (const struct sockaddr *)&cliaddr,
               len);
      } else {
        const char *response = "failed\n";
        sendto(sockfd, response, strlen(response), MSG_CONFIRM, (const struct sockaddr *)&cliaddr,
               len);
      }
      continue;
    }

    if (strcmp(buffer, "tc") == 0) {
      auto &rt = alaska::Runtime::get();
      char *out_buf = nullptr;
      size_t out_size = 0;
      FILE *mem = open_memstream(&out_buf, &out_size);

      rt.tcs_lock.lock();
      for (auto *tc : rt.tcs) {
        tc->dump_info(mem);
      }
      rt.tcs_lock.unlock();

      fflush(mem);
      fclose(mem);

      sendto(sockfd, out_buf, out_size, MSG_CONFIRM, (const struct sockaddr *)&cliaddr, len);
      free(out_buf);
    } else if (strcmp(buffer, "ping") == 0) {
      const char *response = "pong";
      sendto(sockfd, response, strlen(response), MSG_CONFIRM, (const struct sockaddr *)&cliaddr,
             len);
    } else {
      printf("Unknown command: %s\n", buffer);
    }
  }

  return NULL;
}


#include <fcntl.h>
#include <sys/mman.h>
#include <alaska/dwarf/dwarf_parser.h>

static void test_dwarf(void) {
  DwarfTypeDB db;
  dwarf_parse_self(&db);
  dwarf_dump(&db);
}


void __attribute__((constructor(102))) alaska_init(void) {
  // Allocate the runtime simply by creating a new instance of it. Everywhere
  // we use it, we will use alaska::Runtime::get() to get the singleton instance.
  the_runtime = new alaska::Runtime();
  // Attach the runtime's barrier manager
  the_runtime->barrier_manager = &the_barrier_manager;
  // Trigger the current() bootstrap swap now so the hot path skips init checks.
  alaska::ThreadCache::current();
  pthread_create(&barrier_thread, NULL, barrier_thread_func, NULL);

  char *port_env = getenv("ALASKA_CMD_PORT");
  if (port_env) {
    int port = atoi(port_env);
    pthread_create(&cmd_thread, NULL, cmd_thread_function, (void *)(intptr_t)port);
  }

  test_dwarf();
}

void __attribute__((destructor)) alaska_deinit(void) {}
