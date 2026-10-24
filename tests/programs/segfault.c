// Dereferences a null pointer. The volatile stops the compiler from proving (and
// "optimizing") the undefined behaviour away.
#include <stdio.h>

int main(void) {
    volatile int* pointer = NULL;
    puts("about to crash");
    fflush(stdout);
    *pointer = 1; /* CRASH_LINE */
    return 0;
}
