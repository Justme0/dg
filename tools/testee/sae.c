#include <stdio.h>

int * a() {
    int a[33] = {3, 2};

    return a + 3;
}

double * b() {
    double a;
    double *p = &a;

    return p;
}

int *gp;

void c() {
    int **q = &gp;
    int val;
    *q = &val;
}


int main() {
    a();
    b();
    c();

    return 0;
}
