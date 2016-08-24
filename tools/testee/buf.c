#include <stdio.h>

int main() {
    int arr[3];
    int a;
    scanf("%d", &a);
    int b = 4;
    if (a > 0) {
        arr[1] = 3;
    } else {
        b = 3;
    }

    if (b > 0) {
        printf("%d\n", arr[1]);
    } else {
        b = 2;
    }

    return 0;
}
