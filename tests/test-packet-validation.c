/*
 * IPv4 TCP header-length validation tests.
 */

#define _GNU_SOURCE

#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "ipv4pkt.h"

static void make_packet(uint8_t *packet, uint8_t tcp_doff)
{
    struct iphdr *iph;
    struct tcphdr *tcph;

    memset(packet, 0, sizeof(struct iphdr) + sizeof(struct tcphdr));
    iph = (struct iphdr *) packet;
    tcph = (struct tcphdr *) (packet + sizeof(*iph));

    iph->version = 4;
    iph->ihl = sizeof(*iph) / 4;
    iph->tot_len = htons(sizeof(*iph) + sizeof(*tcph));
    iph->protocol = IPPROTO_TCP;
    tcph->doff = tcp_doff;
    tcph->syn = 1;
}


static int parse_packet(uint8_t *packet, int packet_len)
{
    struct sockaddr_storage saddr, daddr;
    struct tcphdr *tcph;
    uint8_t ttl;
    int payload_len;

    return fh_pkt4_parse(packet, packet_len, (struct sockaddr *) &saddr,
                         (struct sockaddr *) &daddr, &ttl, &tcph,
                         &payload_len);
}


int main(void)
{
    uint8_t packet[sizeof(struct iphdr) + sizeof(struct tcphdr)];
    uint8_t doff;

    for (doff = 0; doff < 5; doff++) {
        make_packet(packet, doff);
        if (parse_packet(packet, sizeof(packet)) == 0) {
            fprintf(stderr, "accepted invalid TCP data offset %u\n",
                    (unsigned) doff);
            return EXIT_FAILURE;
        }
    }

    make_packet(packet, 5);
    if (parse_packet(packet, sizeof(packet)) < 0) {
        fprintf(stderr, "rejected valid TCP data offset 5\n");
        return EXIT_FAILURE;
    }
    if (parse_packet(packet, sizeof(packet) - 1) == 0) {
        fprintf(stderr, "accepted a truncated TCP header\n");
        return EXIT_FAILURE;
    }

    puts("Packet validation tests passed.");
    return EXIT_SUCCESS;
}
