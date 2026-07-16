/*
 * ipv6pkt.c - FakeHTTP: https://github.com/MikeWang000000/FakeHTTP
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
#include "ipv6pkt.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <libnetfilter_queue/libnetfilter_queue_ipv6.h>
#include <libnetfilter_queue/libnetfilter_queue_tcp.h>

#include "globvar.h"
#include "logging.h"

static int pkt6_total_len(struct ip6_hdr *ip6h, int pkt_len, size_t *total_len)
{
    size_t payload_len;

    payload_len = ntohs(ip6h->ip6_plen);
    *total_len = sizeof(*ip6h) + payload_len;

    if (*total_len > (size_t) pkt_len) {
        E("ERROR: invalid packet length: %d", pkt_len);
        return -1;
    }

    return 0;
}


static int pkt6_find_tcp(void *pkt_data, int pkt_len, struct ip6_hdr **ip6h_ptr,
                         struct tcphdr **tcph_ptr, size_t *tcp_offset,
                         size_t *tcp_len)
{
    struct ip6_hdr *ip6h;
    struct ip6_ext *exth;
    struct ip6_frag *fragh;
    size_t offset, total_len, ext_len;
    uint8_t nexthdr;
    uint16_t frag_offlg;

    if ((size_t) pkt_len < sizeof(*ip6h)) {
        E("ERROR: invalid packet length: %d", pkt_len);
        return -1;
    }

    ip6h = (struct ip6_hdr *) pkt_data;
    if (pkt6_total_len(ip6h, pkt_len, &total_len) < 0) {
        return -1;
    }

    nexthdr = ip6h->ip6_nxt;
    offset = sizeof(*ip6h);

    while (nexthdr != IPPROTO_TCP) {
        switch (nexthdr) {
            case IPPROTO_HOPOPTS:
            case IPPROTO_ROUTING:
            case IPPROTO_DSTOPTS:
                if (total_len - offset < sizeof(*exth)) {
                    E("ERROR: invalid IPv6 extension header length");
                    return -1;
                }
                exth = (struct ip6_ext *) ((uint8_t *) pkt_data + offset);
                ext_len = ((size_t) exth->ip6e_len + 1) * 8;
                if (ext_len < sizeof(*exth) || total_len - offset < ext_len) {
                    E("ERROR: invalid IPv6 extension header length");
                    return -1;
                }
                nexthdr = exth->ip6e_nxt;
                offset += ext_len;
                break;

            case IPPROTO_FRAGMENT:
                if (total_len - offset < sizeof(*fragh)) {
                    E("ERROR: invalid IPv6 fragment header length");
                    return -1;
                }
                fragh = (struct ip6_frag *) ((uint8_t *) pkt_data + offset);
                frag_offlg = ntohs(fragh->ip6f_offlg);
                if (frag_offlg & (IP6F_OFF_MASK | IP6F_MORE_FRAG)) {
                    E("ERROR: unsupported fragmented IPv6 TCP packet");
                    return -1;
                }
                nexthdr = fragh->ip6f_nxt;
                offset += sizeof(*fragh);
                break;

            case IPPROTO_AH:
                if (total_len - offset < sizeof(*exth)) {
                    E("ERROR: invalid IPv6 AH length");
                    return -1;
                }
                exth = (struct ip6_ext *) ((uint8_t *) pkt_data + offset);
                ext_len = ((size_t) exth->ip6e_len + 2) * 4;
                if (ext_len < sizeof(*exth) || total_len - offset < ext_len) {
                    E("ERROR: invalid IPv6 AH length");
                    return -1;
                }
                nexthdr = exth->ip6e_nxt;
                offset += ext_len;
                break;

            default:
                E("ERROR: not a TCP packet (next header %d)", (int) nexthdr);
                return -1;
        }
    }

    if (total_len - offset < sizeof(struct tcphdr)) {
        E("ERROR: invalid packet length: %d", pkt_len);
        return -1;
    }

    *ip6h_ptr = ip6h;
    *tcph_ptr = (struct tcphdr *) ((uint8_t *) pkt_data + offset);
    *tcp_offset = offset;
    *tcp_len = total_len - offset;

    return 0;
}


static uint32_t csum_add(uint32_t sum, const void *data, size_t len)
{
    const uint8_t *p;

    p = data;
    while (len > 1) {
        sum += ((uint16_t) p[0] << 8) | p[1];
        p += 2;
        len -= 2;
    }

    if (len) {
        sum += (uint16_t) p[0] << 8;
    }

    return sum;
}


static uint16_t csum_fold(uint32_t sum)
{
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }

    return ~sum;
}


int fh_pkt6_update_tcp_checksum(void *pkt_data, int pkt_len,
                                struct tcphdr *tcph)
{
    struct ip6_hdr *ip6h;
    struct tcphdr *parsed_tcph;
    size_t tcp_offset, tcp_len;
    uint32_t sum, tcp_len_be, nexthdr_be;

    if (pkt6_find_tcp(pkt_data, pkt_len, &ip6h, &parsed_tcph, &tcp_offset,
                      &tcp_len) < 0 ||
        parsed_tcph != tcph) {
        E(T(pkt6_find_tcp));
        return -1;
    }

    (void) tcp_offset;

    tcp_len_be = htonl(tcp_len);
    nexthdr_be = htonl(IPPROTO_TCP);

    tcph->check = 0;

    sum = 0;
    sum = csum_add(sum, &ip6h->ip6_src, sizeof(ip6h->ip6_src));
    sum = csum_add(sum, &ip6h->ip6_dst, sizeof(ip6h->ip6_dst));
    sum = csum_add(sum, &tcp_len_be, sizeof(tcp_len_be));
    sum = csum_add(sum, &nexthdr_be, sizeof(nexthdr_be));
    sum = csum_add(sum, tcph, tcp_len);

    tcph->check = htons(csum_fold(sum));
    return 0;
}


int fh_pkt6_parse(void *pkt_data, int pkt_len, struct sockaddr *saddr,
                  struct sockaddr *daddr, uint8_t *ttl,
                  struct tcphdr **tcph_ptr, int *tcp_payload_len)
{
    struct ip6_hdr *ip6h;
    struct tcphdr *tcph;
    size_t tcp_offset, tcp_len;
    int tcph_len;
    struct sockaddr_in6 *saddr_in6, *daddr_in6;

    saddr_in6 = (struct sockaddr_in6 *) saddr;
    daddr_in6 = (struct sockaddr_in6 *) daddr;

    if (pkt6_find_tcp(pkt_data, pkt_len, &ip6h, &tcph, &tcp_offset,
                      &tcp_len) < 0) {
        E(T(pkt6_find_tcp));
        return -1;
    }

    tcph_len = tcph->doff * 4;
    if ((size_t) tcph_len < sizeof(*tcph) || tcp_len < (size_t) tcph_len) {
        E("ERROR: invalid packet length: %d", pkt_len);
        return -1;
    }

    memset(saddr_in6, 0, sizeof(*saddr_in6));
    saddr_in6->sin6_family = AF_INET6;
    memcpy(&saddr_in6->sin6_addr, &ip6h->ip6_src, sizeof(struct in6_addr));

    memset(daddr_in6, 0, sizeof(*daddr_in6));
    daddr_in6->sin6_family = AF_INET6;
    memcpy(&daddr_in6->sin6_addr, &ip6h->ip6_dst, sizeof(struct in6_addr));

    *ttl = ip6h->ip6_hlim;
    *tcph_ptr = tcph;
    *tcp_payload_len = (int) (tcp_len - (size_t) tcph_len);

    return 0;
}


int fh_pkt6_make(uint8_t *buffer, size_t buffer_size, struct sockaddr *saddr,
                 struct sockaddr *daddr, uint8_t ttl, uint16_t sport_be,
                 uint16_t dport_be, uint32_t seq_be, uint32_t ackseq_be,
                 int psh, uint8_t *tcp_payload, size_t tcp_payload_size)
{
    size_t pkt_len;
    struct ip6_hdr *ip6h;
    struct tcphdr *tcph;
    uint8_t *tcppl;
    struct sockaddr_in6 *saddr_in6, *daddr_in6;

    if (saddr->sa_family != AF_INET6 || daddr->sa_family != AF_INET6) {
        E("ERROR: Invalid address family");
        return -1;
    }

    saddr_in6 = (struct sockaddr_in6 *) saddr;
    daddr_in6 = (struct sockaddr_in6 *) daddr;

    pkt_len = sizeof(*ip6h) + sizeof(*tcph) + tcp_payload_size;
    if (buffer_size < pkt_len) {
        E("ERROR: %s", strerror(ENOBUFS));
        return -1;
    }

    ip6h = (struct ip6_hdr *) buffer;
    tcph = (struct tcphdr *) (buffer + sizeof(*ip6h));
    tcppl = buffer + sizeof(*ip6h) + sizeof(*tcph);

    memset(ip6h, 0, sizeof(*ip6h));
    ip6h->ip6_flow = htonl((6 << 28) /* version */ | (0 << 20) /* traffic */ |
                           0 /* flow */);
    ip6h->ip6_plen = htons(sizeof(*tcph) + tcp_payload_size);
    ip6h->ip6_nxt = IPPROTO_TCP;
    ip6h->ip6_hops = ttl;
    memcpy(&ip6h->ip6_src, &saddr_in6->sin6_addr, sizeof(struct in6_addr));
    memcpy(&ip6h->ip6_dst, &daddr_in6->sin6_addr, sizeof(struct in6_addr));

    memset(tcph, 0, sizeof(*tcph));
    tcph->source = sport_be;
    tcph->dest = dport_be;
    tcph->seq = seq_be;
    tcph->ack_seq = ackseq_be;
    tcph->doff = sizeof(*tcph) / 4;
    tcph->psh = psh;
    tcph->ack = 1;
    tcph->window = htons(0x0080);
    tcph->check = 0;
    tcph->urg_ptr = 0;

    if (tcp_payload_size) {
        memcpy(tcppl, tcp_payload, tcp_payload_size);
    }

    nfq_tcp_compute_checksum_ipv6(tcph, ip6h);

    return pkt_len;
}
