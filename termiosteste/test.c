#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "minimum.h"
 
int main(void)
{
    int c;
 
    if (terminal_init()) {
        if (errno == ENOTTY)
            fprintf(stderr, "This program requires a terminal.\n");
        else
            fprintf(stderr, "Cannot initialize terminal: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }
 
    printf("Press CTRL+C or Q to quit.\n");
 
    while ((c = getc(stdin)) != EOF) {
        if (c >= 33 && c <= 126)
            printf("0x%02x = 0%03o = %3d = '%c'\n", c, c, c, c);
        else
            printf("0x%02x = 0%03o = %3d\n", c, c, c);
 
        if (c == 3 || c == 'Q' || c == 'q')
            break;
    }
 
    printf("Done.\n");
 
    return EXIT_SUCCESS;
}
