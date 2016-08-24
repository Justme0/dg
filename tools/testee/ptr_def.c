#include <stdio.h>

void foo() {
    int* arr[66];
    int remove = 3;
    *arr[2] = 3;
    int i = *arr[2];
}

int main() {
    int a = 2;
    int *p = &a;
    if (a > 1) {
        *p = 2;
    }
    int shit;
    int i = *p;

    foo();

    return 0;
}
