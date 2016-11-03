#include <stdlib.h>

int main() {
    int a = 2;
    if (a > 0) {
        return 2;
    }
    void *p = malloc(3);
    free(p);

    return 0;
}
