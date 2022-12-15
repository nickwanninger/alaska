/* Test C2X UTF-8 string literal type.  */
/* { dg-do compile } */
/* { dg-options "-std=c2x" } */

_Static_assert (_Generic (u8"text", unsigned char*: 1, default: 2) == 1, "UTF-8 string literals have an unexpected type");
_Static_assert (_Generic (u8"x"[0], unsigned char:  1, default: 2) == 1, "UTF-8 string literal elements have an unexpected type");
