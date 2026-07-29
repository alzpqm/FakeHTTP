/*
 * srcinfo.c - FakeHTTP: https://github.com/MikeWang000000/FakeHTTP
 *
 * Copyright (C) 2025  MikeWang000000
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE
#include "srcinfo.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "logging.h"

#define CAPACITY 500

struct srcinfo {
    int initialized;
    int ifindex;
    uint8_t ttl;
    uint8_t hwaddr_len;
    uint8_t hwaddr[8];
    struct sockaddr_storage addr;
};

static struct srcinfo *srci = NULL;
static size_t srci_end = 0;

static int sameip(struct sockaddr *addr1, struct sockaddr *addr2)
{
    struct sockaddr_in *addr_in_1, *addr_in_2;
    struct sockaddr_in6 *addr_in6_1, *addr_in6_2;

    if (addr1->sa_family != addr2->sa_family) {
        return 0;
    }

    if (addr1->sa_family == AF_INET) {
        addr_in_1 = (struct sockaddr_in *) addr1;
        addr_in_2 = (struct sockaddr_in *) addr2;

        return addr_in_1->sin_addr.s_addr == addr_in_2->sin_addr.s_addr;
    } else if (addr1->sa_family == AF_INET6) {
        addr_in6_1 = (struct sockaddr_in6 *) addr1;
        addr_in6_2 = (struct sockaddr_in6 *) addr2;

        return memcmp(&addr_in6_1->sin6_addr, &addr_in6_2->sin6_addr,
                      sizeof(addr_in6_1->sin6_addr)) == 0;
    }
    return 0;
}


int fh_srcinfo_setup(void)
{
    srci = calloc(CAPACITY, sizeof(*srci));
    if (!srci) {
        E("ERROR: calloc(): %s", strerror(errno));
        return -1;
    }
    srci_end = 0;

    return 0;
}


void fh_srcinfo_cleanup(void)
{
    free(srci);
    srci = NULL;
    srci_end = 0;
}


int fh_srcinfo_put(struct sockaddr *addr, int ifindex, uint8_t ttl,
                   uint8_t hwaddr[8], uint8_t hwaddr_len)
{
    struct srcinfo *info;

    info = &srci[srci_end];

    if (hwaddr_len > sizeof(info->hwaddr)) {
        E("ERROR: invalid hardware address length: %u", (unsigned) hwaddr_len);
        return -1;
    }

    if (addr->sa_family == AF_INET) {
        memcpy(&info->addr, addr, sizeof(struct sockaddr_in));
    } else if (addr->sa_family == AF_INET6) {
        memcpy(&info->addr, addr, sizeof(struct sockaddr_in6));
    } else {
        E("ERROR: Unknown sa_family: %d", (int) addr->sa_family);
        return -1;
    }

    info->ifindex = ifindex;
    info->ttl = ttl;
    info->hwaddr_len = hwaddr_len;
    memset(info->hwaddr, 0, sizeof(info->hwaddr));
    memcpy(info->hwaddr, hwaddr, hwaddr_len);
    info->initialized = 1;

    srci_end = (srci_end + 1) % CAPACITY;

    return 0;
}


int fh_srcinfo_get(struct sockaddr *addr, int ifindex, uint8_t *ttl,
                   uint8_t hwaddr[8], uint8_t *hwaddr_len)
{
    size_t i, idx;
    struct srcinfo *info;

    for (i = 0; i < CAPACITY; i++) {
        idx = (srci_end + CAPACITY - i - 1) % CAPACITY;
        info = &srci[idx];
        if (!info->initialized) {
            return 1;
        }
        if (info->ifindex == ifindex &&
            sameip(addr, (struct sockaddr *) &info->addr)) {
            *ttl = info->ttl;
            *hwaddr_len = info->hwaddr_len;
            memcpy(hwaddr, info->hwaddr, sizeof(info->hwaddr));
            return 0;
        }
    }
    return 1;
}
