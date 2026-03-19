#pragma once
#include <stdlib.h>
#include <malloc.h>
#define s_malloc(sz)             malloc(sz)
#define s_realloc(p,sz)          realloc(p,sz)
#define s_free(p)                free(p)
#define s_malloc_usable(sz,u)    ({ void *_p = malloc(sz); *(u) = malloc_usable_size(_p); _p; })
#define s_trymalloc_usable(sz,u) ({ void *_p = malloc(sz); *(u) = malloc_usable_size(_p); _p; })
#define s_realloc_usable(p,sz,u) ({ void *_q = realloc(p,sz); *(u) = malloc_usable_size(_q); _q; })
