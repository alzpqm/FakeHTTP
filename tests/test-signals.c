/*
 * Helper executable for testing /proc executable replacement handling.
 */

#define _GNU_SOURCE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "globvar.h"
#include "signals.h"

int main(int argc, char *argv[])
{
    FILE *ready;

    if (argc < 2) {
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "hold") == 0 && argc == 3) {
        if (fh_signal_setup() < 0) {
            return EXIT_FAILURE;
        }
        ready = fopen(argv[2], "w");
        if (!ready || fclose(ready) < 0) {
            return EXIT_FAILURE;
        }
        while (!g_ctx.exit) {
            pause();
        }
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "kill") == 0 && argc == 2) {
        return fh_kill_running(SIGTERM) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    return EXIT_FAILURE;
}
