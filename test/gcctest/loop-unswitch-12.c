/* { dg-do compile } */
/* { dg-options "-O2 -funswitch-loops -fdump-tree-unswitch-optimized" } */

int
foo(double *a, double *b, double *c, double *d, double *r, int size, int order)
{
  for (int i = 0; i < size; i++)
  {
    double tmp;

    if (order == 1)
      tmp = -8 * a[i];
    else
      tmp = -4 * b[i];

    double x = 3 * tmp + d[i] + tmp;

    if (order == 1)
      x += 2;

    double y = 3.4f * tmp + d[i];
    r[i] = x + y;
  }

  return 0;
}

/* { dg-final { scan-tree-dump-times "unswitching loop . on .if. with condition: order.* == 1" 1 "unswitch" } } */
