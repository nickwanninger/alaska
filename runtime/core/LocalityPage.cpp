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

#include <alaska/LocalityPage.hpp>



namespace alaska {


  LocalityPage::~LocalityPage() {}

  void *LocalityPage::alloc(const alaska::Mapping &m, alaska::AlignedSize size) {
    log_debug("VariablePage: Alloc %zu into %p", size, &m);
    log_debug(" free space: %zu", get_free_space());

    if (get_free_space() < size) return nullptr;

    void *data = data_bump_next;
    log_trace("data %p", data);
    data_bump_next = (void *)((uintptr_t)data_bump_next + size);
    log_trace("new bump %p", data_bump_next);

    auto md = md_bump_next;
    log_trace("md %p", md);
    md_bump_next--;


    log_trace("setting md to %p", &m);
    md->mapping = const_cast<alaska::Mapping *>(&m);
    md->size = size;
    md->allocated = true;
    log_trace("set md!");

    return data;
  }


  bool LocalityPage::release_local(const alaska::Mapping &m, void *ptr) {
    // Don't do anything other than
    auto md = find_md(ptr);
    bytes_freed += md->size;
    md->mapping = nullptr;
    md->data_raw = ptr;
    md->allocated = false;
    return true;
  }


  LocalityPage::Metadata *LocalityPage::find_md(void *ptr) {
    if (ptr < data || ptr > data_bump_next) {
      log_warn("%p is not in this page.", ptr);
      return nullptr;
    }

    auto num_alloc = num_allocated();
    int scale = used_space() / num_alloc;
    off_t guess_off = ((uintptr_t)ptr - (uintptr_t)data) / scale;
    if (guess_off >= num_alloc) guess_off = num_alloc - 1;

    // Grab the guess metadata
    auto guess_ptr = get_ptr(guess_off);

    // Go left or right to find the right one.
    if (ptr > guess_ptr) {
      while (ptr > guess_ptr) {
        if (guess_off >= num_alloc) return nullptr;
        guess_off++;
        guess_ptr = get_ptr(guess_off);
      }
    } else {
      while (ptr < guess_ptr) {
        if (guess_off <= 0) return nullptr;
        guess_off--;
        guess_ptr = get_ptr(guess_off);
      }
    }

    return get_md(guess_off);
  }


  size_t LocalityPage::compact(void) {
    long moved_count = 0;

    auto end = md_bump_next;
    for (size_t i = 0; true; i++) {
      auto *l = get_md(i);
      auto *r = get_md(i + 1);
      if (l == end or r == end) break;
      // If the left is allocated (the right cannot be moved in), skip
      if (l->allocated) continue;
      // If the right is not allocated, or it cannot move, skip.
      if (!r->allocated or r->mapping->is_pinned()) continue;

      // swap right and left
      // Copy the metadata to the stack
      // auto lv = *l;
      // auto rv = *r;
      auto *mapping = r->mapping;
      ALASKA_ASSERT(mapping != NULL, "Mapping insane.");

      // printf("    mapping = %p -> %p\n", mapping, mapping->get_pointer());


      size_t real_size = (off_t)r->get_data() - (off_t)l->get_data();


      // printf("L:%4d %016p %8d  %016p %zu\n", i, l->data_raw, l->size, l->get_data(), real_size);
      // printf("R:%4d %016p %8d  %016p\n", i+1, r->data_raw, r->size, r->get_data());
      // ALASKA_ASSERT(l->size == real_size, "Sizes dont match\n");

      size_t left_size = l->size;
      size_t right_size = r->size;

      void *dst = l->get_data();
      void *src = r->get_data();
      void *new_free_data = reinterpret_cast<void *>((off_t)dst + r->size);
      // printf("    %p %p %p\n", dst, src, new_free_data);
      if (l->size == r->size) {
        ALASKA_ASSERT(src == new_free_data, "huh 0");
      }

      // printf("move %p to %p\n", src, dst);
      ALASKA_ASSERT(this->contains(src), "huh 1");
      ALASKA_ASSERT(this->contains(dst), "huh 2");

      memmove(dst, src, r->size);



      r->size = left_size;
      r->data_raw = new_free_data;
      r->allocated = false;


      ALASKA_ASSERT(l->data_raw == dst, "ahh");
      l->size = right_size;
      l->mapping = mapping;
      l->allocated = true;

      mapping->set_pointer(dst);

      moved_count++;

      ALASKA_ASSERT(l->allocated, "");
      ALASKA_ASSERT(not r->allocated, "");
      ALASKA_ASSERT(r->get_data() == new_free_data, "");
      ALASKA_ASSERT(l->get_data() == dst, "");
    }

    // if (moved_count == 0) {
    int prune_count = 0;
    while (!md_bump_next[1].allocated) {
      md_bump_next++;
      prune_count++;
      this->bytes_freed -= md_bump_next->size;
    }
    if (prune_count > 0) {
      auto &m = md_bump_next[1];
      data_bump_next = reinterpret_cast<void *>((off_t)m.get_data() + m.size);
    }
    return moved_count;
  }


  size_t LocalityPage::size_of(void *data) {
    auto md = find_md(data);
    return md->size;
  }



  void LocalityPage::dump_html(FILE *stream) {
    fprintf(stream, "<td>LocalityPage util:%f</td>", utilization());
    fprintf(stream, "<td class='heapdata localitydata'>");

    for (size_t i = 0; true; i++) {
      auto *h = get_md(i);
      if (h == md_bump_next) break;
      int page = ((off_t)h->get_data() >> 12) & 1;
      bool pinned = false;
      if (h->allocated && h->mapping->is_pinned()) pinned = true;
      fprintf(stream, "<div class='el %s p%d%s' style='width: %dpx'></div>",
          h->allocated ? "al" : "fr", page, pinned ? " pin" : "", h->size / 16);
    }
    fprintf(stream, "</td>");
  }


  void LocalityPage::dump_json(FILE *stream) {
    fprintf(stream, "{\"name\": \"LocalityPage\", \"utilization\": %f}", this->utilization());
  }
}  // namespace alaska
