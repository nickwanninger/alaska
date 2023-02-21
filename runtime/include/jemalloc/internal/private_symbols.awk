#!/usr/bin/env awk -f

BEGIN {
  sym_prefix = ""
  split("\
        alaska_je_aligned_alloc \
        alaska_je_calloc \
        alaska_je_dallocx \
        alaska_je_free \
        alaska_je_mallctl \
        alaska_je_mallctlbymib \
        alaska_je_mallctlnametomib \
        alaska_je_malloc \
        alaska_je_malloc_conf \
        alaska_je_malloc_message \
        alaska_je_malloc_stats_print \
        alaska_je_malloc_usable_size \
        alaska_je_mallocx \
        alaska_je_smallocx_0 \
        alaska_je_nallocx \
        alaska_je_posix_memalign \
        alaska_je_rallocx \
        alaska_je_realloc \
        alaska_je_sallocx \
        alaska_je_sdallocx \
        alaska_je_xallocx \
        alaska_je_memalign \
        alaska_je_valloc \
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
