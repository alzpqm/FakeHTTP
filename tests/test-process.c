/* Regression tests for command execution interrupted by a signal. */

#define _GNU_SOURCE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "process.h"

static void alarm_handler(int signo)
{
    (void) signo;
}


static int test_closed_stdin(void)
{
    char *command[] = {"/bin/true", NULL};
    size_t len = 1024 * 1024;
    char *input;
    int res;

    input = malloc(len + 1);
    if (!input) {
        perror("malloc");
        return -1;
    }
    memset(input, 'x', len);
    input[len] = 0;

    res = fh_execute_command(command, 1, input);
    free(input);

    return res < 0 ? 0 : -1;
}


int main(void)
{
    char *command[] = {"/bin/sleep", "2", NULL};
    struct sigaction action, old_action;
    int res;

    memset(&action, 0, sizeof(action));
    action.sa_handler = alarm_handler;
    if (sigemptyset(&action.sa_mask) < 0 ||
        sigaction(SIGALRM, &action, &old_action) < 0) {
        perror("sigaction");
        return 1;
    }

    alarm(1);
    res = fh_execute_command(command, 1, NULL);
    alarm(0);
    if (sigaction(SIGALRM, &old_action, NULL) < 0) {
        perror("sigaction restore");
        return 1;
    }

    if (res < 0) {
        fprintf(stderr, "waitpid interrupted command failed\n");
        return 1;
    }

    if (test_closed_stdin() < 0) {
        fprintf(stderr, "closed stdin was not reported as a failure\n");
        return 1;
    }

    puts("Process EINTR test passed.");
    return 0;
}
