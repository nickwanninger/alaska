/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2023, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2023, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */
#include <stdio.h>
#include <signal.h>
#include <alaska.h>
#include <alaska/internal.h>
#include <ctype.h>


#define WEAK __attribute__((weak))

int alaska_wrapped_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
  printf("sigaction=%d\n", signum);
  if (signum == SIGSEGV) {
    return -1;
  }
  return sigaction(signum, act, oldact);
}


// # define weak_alias(name, aliasname) _weak_alias (name, aliasname)
// # define _weak_alias(name, aliasname) \
//   extern __typeof (name) aliasname __attribute__ ((weak, alias (#name)));


// // #define alaska_translate(l) l
// // #define

// int memcmp(const void *vl, const void *vr, size_t n) {
//   const unsigned char *l = vl, *r = vr;
//   for (; n && *l == *r; n--, l++, r++)
//     ;
//   return n ? *l - *r : 0;
// }

// int bcmp(const void *s1, const void *s2, size_t n) {
// 	return memcmp(s1, s2, n);
// }


// char *__stpcpy(char *restrict d, const char *restrict s) {
//   for (; (*d = *s); s++, d++)
//     ;

//   return d;
// }

// int strcmp(const char *l, const char *r) {
//   for (; *l == *r && *l; l++, r++)
//     ;

//   return *(unsigned char *)l - *(unsigned char *)r;
// }

// void *memchr(const void *vc, int ch, size_t n) {
//   const unsigned char *c = vc;
//   for (size_t i = 0; i < n; i++) {
//     if (c[i] == (unsigned char)ch) {
//       return (void *)((unsigned char *)c + i);
//     }
//   }
//   return NULL;
// }

// void *memset(void *s, int c, size_t n) {
//   uint8_t *p = (uint8_t *)s;
//   for (size_t i = 0; i < n; i++)
//     p[i] = c;
//   return s;
// }


// void *memcpy(void *vdst, const void *vsrc, size_t n) {
//   uint8_t *dst = (uint8_t *)vdst;
//   uint8_t *src = (uint8_t *)(void *)vsrc;
//   for (size_t i = 0; i < n; i++) {
//     dst[i] = src[i];
//   }
//   return vdst;
// }


// size_t strlen(const char *vsrc) {
//   uint8_t *src = (uint8_t *)vsrc;
//   size_t s = 0;
//   for (s = 0; src[s]; s++) {
//   }

//   return s;
// }

// weak_alias(strlen, __strlen_avx2);
// weak_alias(strlen, __printf);

// char *strcpy(char *vdest, const char *vsrc) {
//   uint8_t *dest = (uint8_t *)vdest;
//   uint8_t *src = (uint8_t *)vsrc;
//   size_t i;

//   for (i = 0; src[i] != '\0'; i++)
//     dest[i] = src[i];



//   return vdest;
// }

// char *strncpy(char *vdest, const char *vsrc, size_t n) {
//   uint8_t *dest = (uint8_t *)vdest;
//   uint8_t *src = (uint8_t *)vsrc;
//   size_t i;

//   for (i = 0; i < n && src[i] != '\0'; i++)
//     dest[i] = src[i];
//   for (; i < n; i++)
//     dest[i] = '\0';



//   return vdest;
// }


// char *strchr(const char *p, int ch) {
//   char c;

//   c = ch;
//   for (;; ++p) {
//     if (*p == c) return ((char *)p);
//     if (*p == '\0') return (NULL);
//   }
//   /* NOTREACHED */
// }

// char *strstr(const char *string, const char *substring) {
//   const char *a, *b;

//   /* First scan quickly through the two strings looking for a
//    * single-character match.  When it's found, then compare the
//    * rest of the substring.
//    */

//   b = substring;
//   if (*b == 0) {
//     return (char *)string;
//   }
//   for (; *string != 0; string += 1) {
//     if (*string != *b) {
//       continue;
//     }
//     a = string;
//     while (1) {
//       if (*b == 0) {
//         return (char *)string;
//       }
//       if (*a++ != *b++) {
//         break;
//       }
//     }
//     b = substring;
//   }
//   return NULL;
// }

// char *strrchr(const char *s, int c) {
//   char *rtnval = 0;

//   do {
//     if (*s == c) rtnval = (char *)s;
//   } while (*s++);
//   return (rtnval);
// }

// typedef unsigned char u_char;

// int strcasecmp(const char *s1, const char *s2) {
//   const u_char *us1 = (const u_char *)s1, *us2 = (const u_char *)s2;

//   while (tolower(*us1) == tolower(*us2++))
//     if (*us1++ == '\0') return (0);
//   return (tolower(*us1) - tolower(*--us2));
// }

// int strncasecmp(const char *s1, const char *s2, size_t n) {
//   if (n != 0) {
//     const u_char *us1 = (const u_char *)s1, *us2 = (const u_char *)s2;

//     do {
//       if (tolower(*us1) != tolower(*us2++)) return (tolower(*us1) - tolower(*--us2));
//       if (*us1++ == '\0') break;
//     } while (--n != 0);
//   }
//   return (0);
// }