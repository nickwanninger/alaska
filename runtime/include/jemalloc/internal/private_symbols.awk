#!/usr/bin/env awk -f

BEGIN {
  sym_prefix = ""
  split("\
        aje_aligned_alloc \
        aje_calloc \
        aje_dallocx \
        aje_free \
        aje_mallctl \
        aje_mallctlbymib \
        aje_mallctlnametomib \
        aje_malloc \
        aje_malloc_conf \
        aje_malloc_conf_2_conf_harder \
        aje_malloc_message \
        aje_malloc_stats_print \
        aje_malloc_usable_size \
        aje_mallocx \
        aje_smallocx_0bd1a3a4a89d34ddbc5ad9c7b62f8ac3db1c7ede \
        aje_nallocx \
        aje_posix_memalign \
        aje_rallocx \
        aje_realloc \
        aje_sallocx \
        aje_sdallocx \
        aje_xallocx \
        aje_memalign \
        aje_valloc \
        pthread_create \
        ", exported_symbol_names)
  # Store exported symbol names as keys in exported_symbols.
  for (i in exported_symbol_names) {
    exported_symbols[exported_symbol_names[i]] = 1
  }
}

# Process 'nm -a <c_source.o>' output.
#
# Handle lines like:
#   0000000000000008 D opt_junk
#   0000000000007574 T malloc_initialized
(NF == 3 && $2 ~ /^[ABCDGRSTVW]$/ && !($3 in exported_symbols) && $3 ~ /^[A-Za-z0-9_]+$/) {
  print substr($3, 1+length(sym_prefix), length($3)-length(sym_prefix))
}

# Process 'dumpbin /SYMBOLS <c_source.obj>' output.
#
# Handle lines like:
#   353 00008098 SECT4  notype       External     | opt_junk
#   3F1 00000000 SECT7  notype ()    External     | malloc_initialized
($3 ~ /^SECT[0-9]+/ && $(NF-2) == "External" && !($NF in exported_symbols)) {
  print $NF
}
