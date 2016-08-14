#include <stdlib.h>

int main() {
  void *p = malloc(3);
  int a = 0;
  if (a > 3) {}
  free(p);

  return 0;
}
