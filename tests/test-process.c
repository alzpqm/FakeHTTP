/* Regression tests for command execution interrupted by a signal. */

#define _GNU_SOURCE

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "globvar.h"
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


static int test_standard_descriptors(void)
{
    int mask, use_log, with_input, failures = 0;
    char *command[] = {"/bin/sh", "-c", NULL, NULL};

    for (with_input = 0; with_input < 2; with_input++) {
        command[2] =
            with_input
                ? "IFS= read -r value && [ \"$value\" = 'test input' ] && "
                  "printf 'output\\n' && printf 'error\\n' >&2"
                : "printf 'output\\n' && printf 'error\\n' >&2";
        for (use_log = 0; use_log < 2; use_log++) {
            for (mask = 0; mask < 8; mask++) {
                int status;
                pid_t child = fork(), waited;
                if (child < 0)
                    return -1;
                if (!child) {
                    FILE *log = NULL;
                    char output[64] = {0};
                    int fd, result;
                    alarm(5);
                    for (fd = 0; fd < 3; fd++) {
                        if (mask & (1 << fd))
                            close(fd);
                    }
                    if (use_log) {
                        log = tmpfile();
                        if (!log)
                            _exit(2);
                        g_ctx.logfp = log;
                    }
                    result = fh_execute_command(
                        command, !use_log, with_input ? "test input\n" : NULL);
                    if (log) {
                        rewind(log);
                        if (fread(output, 1, sizeof(output) - 1, log) != 13 ||
                            strcmp(output, "output\nerror\n"))
                            result = -1;
                        fclose(log);
                    }
                    _exit(result == 0 ? 0 : 1);
                }
                do {
                    waited = waitpid(child, &status, 0);
                } while (waited < 0 && errno == EINTR);
                if (waited < 0)
                    return -1;
                if (!WIFEXITED(status) || WEXITSTATUS(status)) {
                    fprintf(stderr,
                            "descriptor case input=%d log=%d mask=%d failed\n",
                            with_input, use_log, mask);
                    failures++;
                }
            }
        }
    }
    return failures ? -1 : 0;
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

    if (test_standard_descriptors() < 0)
        return 1;

    puts("Process EINTR, broken-pipe and 32 descriptor tests passed.");
    return 0;
}
