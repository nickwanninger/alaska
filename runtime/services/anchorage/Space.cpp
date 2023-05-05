/*
 * this file is part of the alaska handle-based memory management system
 *
 * copyright (c) 2023, nick wanninger <ncw@u.northwestern.edu>
 * copyright (c) 2023, the constellation project
 * all rights reserved.
 *
 * this is free software.  you are permitted to use, redistribute,
 * and modify it as specified in the file "license".
 */

#include <anchorage/Space.hpp>

anchorage::Space *youngest_space = NULL;

void anchorage::initialize_spaces(int num_spaces) {
  youngest_space = new anchorage::Space(num_spaces);

  youngest_space->dump();
}

anchorage::Space::Space(int depth) {
  if (depth > 1) {
    m_older = new anchorage::Space(depth - 1);
  }
}

void anchorage::Space::dump() {
  printf("digraph {\n");
  printf("  label=\"Anchorage Spaces\";\n");
  printf("  compound=true;\n");
  printf("  start=1;\n");


  for (anchorage::Space *cur = this; cur != NULL; cur = cur->older()) {
    if (cur->older() != NULL) {
      printf("  space%p -> space%p\n", cur, cur->older());
    }
  }

  printf("}\n");
}
