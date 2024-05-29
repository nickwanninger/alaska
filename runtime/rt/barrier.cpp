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



#include <alaska.h>
#include <alaska/Logger.hpp>

void alaska_blob_init(struct alaska_blob_config* cfg) {
  log_warn("alaska_blob_init: not implemented yet! Barriers will not work!");
  // managed_blob_text_regions.push({cfg->code_start, cfg->code_end});

  // // Not particularly safe, but we will ignore that for now.
  // auto patch_page = (void*)((uintptr_t)cfg->code_start & ~0xFFF);
  // size_t size = round_up(cfg->code_end - cfg->code_start, 4096);
  // mprotect(patch_page, size + 4096, PROT_EXEC | PROT_READ | PROT_WRITE);

  // // printf("%p %p\n", cfg->code_start, cfg->code_end);

  // if (cfg->stackmap) parse_stack_map((uint8_t*)cfg->stackmap);
}