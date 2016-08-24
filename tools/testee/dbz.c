#include <stdio.h>
#include <stdlib.h>

int a[2] = {2, -3};

int m[2][2];

void test() {
    int var = 3;
    void *p = calloc(3, 2);
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            m[i][j] = a[i] * a[j];
            if (m[i][j] < 0) {
                printf("%d\n", m[i][j]);
            }
        }
        var /= i + 4;
    }
    free(p);
}

int main() {
    test();

    return 0;
}
