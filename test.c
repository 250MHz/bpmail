/*
 * Temporary file to test that we can use bp library.
 * Should be deleted soon.
 */
#include <stdio.h>

#include "bp.h"

int main(void) {
    if (bp_attach() != 0) {
        fprintf(stderr, "Can't attach to BP\n");
        exit(EXIT_FAILURE);
    }
    printf("hello, world\n");
    bp_detach();
    return EXIT_SUCCESS;
}

