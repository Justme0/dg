#include <stdlib.h>

void foo() {
  void *p = malloc(3);
  int a = 0;
  int b = 0;
  if (a > 3) {
    free(p);
    b = a;
  }
  free(p);
}

int main() {
  foo();

  return 0;
}
