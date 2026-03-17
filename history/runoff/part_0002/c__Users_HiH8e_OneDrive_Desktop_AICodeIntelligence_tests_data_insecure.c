#include <stdio.h>
#include <string.h>

void bad(char *dst, const char *src) {
    strcpy(dst, src); // unsafe
}
