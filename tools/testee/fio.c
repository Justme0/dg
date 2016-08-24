#include <stdio.h>

void foo()
{
    FILE * data;
    data = NULL;
    data = fopen("BadSource_fopen.txt", "w+");
    if (2 > 1) {
        printf("ok\n");
    }
    data = fopen("BadSink_fopen.txt", "w+");
    if (data != NULL)
    {
        fclose(data);
    }
}

int main() {
    int a = 2;
    int b = a;
    ++a;
    if (a == 2) {
        foo();
    }

    return 0;
}
