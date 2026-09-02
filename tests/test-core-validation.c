/*
 * Local unit tests for payload boundaries and source-interface caching.
 */

#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include "globvar.h"
#include "payload.h"
#include "srcinfo.h"

static int write_payload_file(int fd, size_t size)
{
    static const uint8_t data[128] = {0};

    size_t chunk, written;
    ssize_t res;

    written = 0;
    while (written < size) {
        chunk = size - written;
        if (chunk > sizeof(data)) {
            chunk = sizeof(data);
        }

        res = write(fd, data, chunk);
        if (res < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (!res) {
            return -1;
        }
        written += res;
    }

    return 0;
}


static int payload_case(size_t file_size, int expect_success)
{
    char path[] = "/tmp/fakehttp-payload-test.XXXXXX";
    struct payload_info payloads[2];
    uint8_t *payload;
    size_t payload_len;
    int fd, res, success;

    fd = mkstemp(path);
    if (fd < 0) {
        perror("mkstemp");
        return -1;
    }
    if (write_payload_file(fd, file_size) < 0) {
        perror("write");
        close(fd);
        unlink(path);
        return -1;
    }
    if (close(fd) < 0) {
        perror("close");
        unlink(path);
        return -1;
    }

    payloads[0].type = FH_PAYLOAD_CUSTOM;
    payloads[0].info = path;
    payloads[1].type = FH_PAYLOAD_END;
    payloads[1].info = NULL;
    g_ctx.plinfo = payloads;

    res = fh_payload_setup();
    success = res == 0;
    if (success) {
        th_payload_get(&payload, &payload_len);
        if (payload_len != file_size) {
            fprintf(stderr, "payload length mismatch: %zu != %zu\n",
                    payload_len, file_size);
            success = 0;
        }
        fh_payload_cleanup();
    }

    if (unlink(path) < 0) {
        perror("unlink");
        return -1;
    }

    return success == expect_success ? 0 : -1;
}


static int run_payload_case(size_t file_size, int expect_success)
{
    int status;
    pid_t pid;

    pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (!pid) {
        _exit(payload_case(file_size, expect_success) == 0 ? 0 : 1);
    }

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return -1;
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}


static int test_srcinfo_interfaces(void)
{
    static const uint8_t hwaddr_1[8] = {0x00, 0x11, 0x22, 0x33,
                                        0x44, 0x55, 0x00, 0x00};
    static const uint8_t hwaddr_2[8] = {0xaa, 0xbb, 0xcc, 0xdd,
                                        0xee, 0xff, 0x00, 0x00};

    struct sockaddr_in addr;
    uint8_t hwaddr[8], hwaddr_len, ttl;
    int res;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, "192.0.2.10", &addr.sin_addr) != 1) {
        return -1;
    }

    if (fh_srcinfo_setup() < 0) {
        return -1;
    }
    if (fh_srcinfo_put((struct sockaddr *) &addr, 7, 51, (uint8_t *) hwaddr_1,
                       6) < 0 ||
        fh_srcinfo_put((struct sockaddr *) &addr, 8, 62, (uint8_t *) hwaddr_2,
                       6) < 0) {
        fh_srcinfo_cleanup();
        return -1;
    }

    res = fh_srcinfo_get((struct sockaddr *) &addr, 7, &ttl, hwaddr,
                         &hwaddr_len);
    if (res != 0 || ttl != 51 || hwaddr_len != 6 ||
        memcmp(hwaddr, hwaddr_1, 6) != 0) {
        fh_srcinfo_cleanup();
        return -1;
    }

    res = fh_srcinfo_get((struct sockaddr *) &addr, 8, &ttl, hwaddr,
                         &hwaddr_len);
    if (res != 0 || ttl != 62 || hwaddr_len != 6 ||
        memcmp(hwaddr, hwaddr_2, 6) != 0) {
        fh_srcinfo_cleanup();
        return -1;
    }

    res = fh_srcinfo_get((struct sockaddr *) &addr, 9, &ttl, hwaddr,
                         &hwaddr_len);
    fh_srcinfo_cleanup();

    return res == 1 ? 0 : -1;
}


int main(void)
{
    if (run_payload_case(1200, 1) < 0 || run_payload_case(1201, 0) < 0) {
        fprintf(stderr, "payload boundary validation failed\n");
        return EXIT_FAILURE;
    }

    if (test_srcinfo_interfaces() < 0) {
        fprintf(stderr, "source-interface cache validation failed\n");
        return EXIT_FAILURE;
    }

    puts("Core validation tests passed.");
    return EXIT_SUCCESS;
}
