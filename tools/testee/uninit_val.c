#include <stdio.h>

int a[2] = {2, -2};
int m[2][2];

void foo() {
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            m[i][j] = a[i] * a[j];
            if (m[i][j] < 0) {
                printf("%d\n", m[i][j]);
            }
        }
    }
}

int main() {
    int b;
    int *p = &b;
    foo();
    printf("%d\n", *p);

    return 0;
}
