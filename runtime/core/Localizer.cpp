
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


#include <alaska/Runtime.hpp>
#include <alaska/Localizer.hpp>
#include "alaska/liballoc.h"

namespace alaska {


  Localizer::Localizer(alaska::Configuration &config, alaska::ThreadCache &tc)
      : tc(tc) {}

  handle_id_t *Localizer::get_hotness_buffer(size_t count) {
    void *x = alaska_internal_malloc(count * sizeof(handle_id_t));
    return (handle_id_t *)x;
  }

  void Localizer::feed_hotness_buffer(size_t count, handle_id_t *handle_ids) {
    auto &rt = alaska::Runtime::get();

    unsigned long moved_objects = 0;
    unsigned long unmoved_objects = 0;
    unsigned long bytes_in_dump = 0;
    unsigned long bytes_reach = 0;

    rt.with_barrier([&]() {
      for (size_t i = 0; i < count; i++) {
        auto hid = handle_ids[i];
        if (hid == 0) continue;
        auto handle = reinterpret_cast<void *>((1LU << 63) | (hid << ALASKA_SIZE_BITS));
        auto *m = alaska::Mapping::from_handle_safe(handle);

        bool moved = false;
        if (m == NULL or m->is_free() or m->is_pinned()) {
          moved = false;
        } else {
          void *ptr = m->get_pointer();
          auto *source_page = rt.heap.pt.get_unaligned(m->get_pointer());
          moved = tc.localize(*m, rt.localization_epoch);
        }

        auto size = tc.get_size(handle);

        bytes_reach += size;

        if (moved) {
          moved_objects++;
          bytes_in_dump += size;
        } else {
          unmoved_objects++;
        }
      }


      // rt.heap.compact_locality_pages();
      // rt.heap.compact_sizedpages();
    });

    printf("moved:%5lu unmoved:%5lu bytes:%12lu reach:%12lu\n", moved_objects, unmoved_objects,
        bytes_in_dump, bytes_reach);

    alaska_internal_free(reinterpret_cast<void *>(handle_ids));
  }
}  // namespace alaska
