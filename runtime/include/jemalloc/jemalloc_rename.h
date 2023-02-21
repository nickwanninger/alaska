/*
 * Name mangling for public symbols is controlled by --with-mangling and
 * --with-jemalloc-prefix.  With default settings the je_ prefix is stripped by
 * these macro definitions.
 */
#ifndef JEMALLOC_NO_RENAME
#  define je_aligned_alloc alaska_je_aligned_alloc
#  define je_calloc alaska_je_calloc
#  define je_dallocx alaska_je_dallocx
#  define je_free alaska_je_free
#  define je_mallctl alaska_je_mallctl
#  define je_mallctlbymib alaska_je_mallctlbymib
#  define je_mallctlnametomib alaska_je_mallctlnametomib
#  define je_malloc alaska_je_malloc
#  define je_malloc_conf alaska_je_malloc_conf
#  define je_malloc_message alaska_je_malloc_message
#  define je_malloc_stats_print alaska_je_malloc_stats_print
#  define je_malloc_usable_size alaska_je_malloc_usable_size
#  define je_mallocx alaska_je_mallocx
#  define je_smallocx_0 alaska_je_smallocx_0
#  define je_nallocx alaska_je_nallocx
#  define je_posix_memalign alaska_je_posix_memalign
#  define je_rallocx alaska_je_rallocx
#  define je_realloc alaska_je_realloc
#  define je_sallocx alaska_je_sallocx
#  define je_sdallocx alaska_je_sdallocx
#  define je_xallocx alaska_je_xallocx
#  define je_memalign alaska_je_memalign
#  define je_valloc alaska_je_valloc
#endif
