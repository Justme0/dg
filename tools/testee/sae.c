#include <stdio.h>

int *local() {
  int a[33] = {3, 2};
  return a;
}

int *g_p;
void global() {
  int a = 2;
  g_p = &a;
}

void argument(int **pp) {
  int a = 3;
  *pp = &a;
}

int main() {
  local();

  global();

  int **pp = NULL;
  argument(pp);

  return 0;
}
