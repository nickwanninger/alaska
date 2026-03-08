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



  float toWait = 1.0;  // Wait a second at the start... Update
  while (1) {
    auto &rt = alaska::Runtime::get();
    useconds_t sleep_time = (useconds_t)(toWait * 1000000);
    usleep(sleep_time);

    continue;

    rt.with_barrier([&]() {
      toWait = rt.scheduler.tick(toWait);
    });
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
}

void __attribute__((destructor)) alaska_deinit(void) {}
