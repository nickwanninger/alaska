/* { dg-options "-Wall -Wextra -O2 -fno-toplevel-reorder -fno-tree-ch -fno-tree-dce -fno-tree-dominator-opts -fno-tree-dse -fno-tree-loop-ivcanon -fpredictive-commoning" } */

short a, b;
int c[9];
void(d)() {}
void e() {
  a = 0;
  for (; a <= 4; a++) {
    short *f = &b;
    c[a] || (*f = 0);
    d(c[a + 2]);
  }
}
int main() {return 0;}
