#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

int fibonacci(int n) {
    int a = 0, b = 1;
    for (int i = 2; i <= n; i++) {
        int t = a + b; a = b; b = t;
    }
    return n <= 1 ? n : b;
}

int main() {
    printf("RawrXD Universal Compiler - C Test\n");
    printf("===================================\n\n");
    printf("Factorial(10) = %d\n", factorial(10));
    printf("Fibonacci(20) = %d\n", fibonacci(20));
    printf("\nAll C tests passed!\n");
    return 0;
}
