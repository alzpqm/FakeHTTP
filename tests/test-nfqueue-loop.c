/* Exercise the real loop using a socketpair, without a live firewall/queue. */
#include "../src/nfqueue.c"

#include <poll.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

enum scenario {
    STOP_BEFORE_POLL,
    STOP_BEFORE_RECV,
    LOST_READINESS,
    PACKET
};
static enum scenario scenario;
static int poll_calls, recv_calls, handled;
static volatile sig_atomic_t watchdog_fired;

static void watchdog(int signo)
{
    (void) signo;
    watchdog_fired = 1;
    g_ctx.exit = 1;
}

int __real_poll(struct pollfd *fds, nfds_t count, int timeout);
ssize_t __real_recv(int socket_fd, void *buffer, size_t length, int flags);

int __wrap_poll(struct pollfd *fds, nfds_t count, int timeout)
{
    poll_calls++;
    if ((scenario == STOP_BEFORE_POLL && poll_calls == 1) ||
        (scenario == LOST_READINESS && poll_calls == 2))
        raise(SIGTERM);
    return __real_poll(fds, count, timeout);
}

ssize_t __wrap_recv(int socket_fd, void *buffer, size_t length, int flags)
{
    recv_calls++;
    if (recv_calls == 1 &&
        (scenario == STOP_BEFORE_RECV || scenario == LOST_READINESS)) {
        char discarded;
        if (__real_recv(socket_fd, &discarded, 1, MSG_DONTWAIT) != 1)
            return -1;
        if (scenario == STOP_BEFORE_RECV)
            raise(SIGTERM);
    }
    return __real_recv(socket_fd, buffer, length, flags);
}

int __wrap_nfq_handle_packet(struct nfq_handle *handle, char *packet,
                             int length)
{
    (void) handle;
    if (length != 1 || packet[0] != 'x')
        return -1;
    handled++;
    g_ctx.exit = 1;
    return 0;
}

int fh_rawsend_handle(struct sockaddr_ll *sll, uint8_t *packet, int length,
                      int *modified)
{
    (void) sll;
    (void) packet;
    (void) length;
    *modified = 0;
    return NF_ACCEPT;
}

int main(void)
{
    int pair[2], result, failures = 0;
    struct sigaction action = {0};
    struct timespec before, after;
    g_ctx.logfp = stderr;
    if (fh_signal_setup() < 0)
        return 2;
    action.sa_handler = watchdog;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGALRM, &action, NULL) < 0)
        return 2;

    for (scenario = STOP_BEFORE_POLL; scenario <= PACKET; scenario++) {
        double elapsed_ms;
        if (socketpair(AF_UNIX, SOCK_DGRAM, 0, pair) < 0)
            return 2;
        fd = pair[0];
        if (scenario != STOP_BEFORE_POLL && send(pair[1], "x", 1, 0) != 1)
            return 2;
        poll_calls = recv_calls = handled = 0;
        watchdog_fired = g_ctx.exit = 0;
        alarm(2);
        clock_gettime(CLOCK_MONOTONIC, &before);
        result = fh_nfq_loop();
        clock_gettime(CLOCK_MONOTONIC, &after);
        alarm(0);
        close(pair[0]);
        close(pair[1]);
        elapsed_ms = 1000.0 * (after.tv_sec - before.tv_sec) +
                     (after.tv_nsec - before.tv_nsec) / 1000000.0;
        if (result != 0 || watchdog_fired || elapsed_ms >= 1000.0 ||
            handled != (scenario == PACKET)) {
            fprintf(stderr,
                    "queue case %d failed: result=%d watchdog=%d "
                    "elapsed=%.1fms handled=%d\n",
                    scenario, result, (int) watchdog_fired, elapsed_ms,
                    handled);
            failures++;
        }
    }
    if (failures)
        return 1;
    puts("NFQUEUE stop, lost-readiness and packet-dispatch tests passed.");
    return 0;
}
