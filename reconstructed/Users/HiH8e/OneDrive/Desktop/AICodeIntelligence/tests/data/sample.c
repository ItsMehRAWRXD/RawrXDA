// sample C-like file
#include <stdio.h>

int foo(int x) {
    if (x > 0) {
        return x + 1;
    }
    return 0;
}

struct Bar { int value; };

int main() {
    int y = foo(41);
    printf("%d", y);
    return 0;
}
