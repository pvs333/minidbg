// Exits with the status given as argv[1] (default 42) so tests can check exit reporting.
#include <stdlib.h>

int main(int argc, char** argv) {
    if (argc > 1) {
        return atoi(argv[1]);
    }
    return 42;
}
