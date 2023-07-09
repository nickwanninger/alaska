/*
 * Name mangling for public symbols is controlled by --with-mangling and
 * --with-jemalloc-prefix.  With default settings the je_ prefix is stripped by
 * these macro definitions.
 */
#ifndef JEMALLOC_NO_RENAME
#  define je_aligned_alloc aje_aligned_alloc
#  define je_calloc aje_calloc
#  define je_dallocx aje_dallocx
#  define je_free aje_free
#  define je_mallctl aje_mallctl
#  define je_mallctlbymib aje_mallctlbymib
#  define je_mallctlnametomib aje_mallctlnametomib
#  define je_malloc aje_malloc
#  define je_malloc_conf aje_malloc_conf
#  define je_malloc_conf_2_conf_harder aje_malloc_conf_2_conf_harder
#  define je_malloc_message aje_malloc_message
#  define je_malloc_stats_print aje_malloc_stats_print
#  define je_malloc_usable_size aje_malloc_usable_size
#  define je_mallocx aje_mallocx
#  define je_smallocx_0bd1a3a4a89d34ddbc5ad9c7b62f8ac3db1c7ede aje_smallocx_0bd1a3a4a89d34ddbc5ad9c7b62f8ac3db1c7ede
#  define je_nallocx aje_nallocx
#  define je_posix_memalign aje_posix_memalign
#  define je_rallocx aje_rallocx
#  define je_realloc aje_realloc
#  define je_sallocx aje_sallocx
#  define je_sdallocx aje_sdallocx
#  define je_xallocx aje_xallocx
#  define je_memalign aje_memalign
#  define je_valloc aje_valloc
#endif
