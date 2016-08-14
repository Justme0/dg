#include <stdlib.h>
#include <stdio.h>

void foo() {
  int a = 5;
  int *p = (int *)malloc(a);
  double *r = (double *)malloc(3);
  int b = a + 3;
  int *q = p + 1;
  free(r - 1);
  scanf("%d", q);
  printf("%d\n", b);
  free(q);
}

int main() {
  foo();

  return 0;
}
