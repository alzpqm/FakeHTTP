/*
 * mainfun.c - FakeHTTP: https://github.com/MikeWang000000/FakeHTTP
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
#include "mainfun.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/resource.h>
#include <sys/socket.h>

#include "globvar.h"
#include "logging.h"
#include "nfqueue.h"
#include "nfrules.h"
#include "payload.h"
#include "process.h"
#include "rawsend.h"
#include "signals.h"
#include "srcinfo.h"

#ifndef PROGNAME
#define PROGNAME "fakehttp"
#endif /* PROGNAME */

#ifndef VERSION
#define VERSION "dev"
#endif /* VERSION */

static void print_usage(const char *name)
{
    static const char *usage_fmt =
        "Usage: %s [options]\n"
        "\n"
        "Interface Options:\n"
        "  -a                 work on all network interfaces (ignores -i)\n"
        "  -i <interface>     work on specified network interface\n"
        "\n"
        "Payload Options:\n"
        "  -b <file>          use TCP payload from binary file\n"
        "  -e <hostname>      hostname for HTTPS obfuscation\n"
        "  -h <hostname>      hostname for HTTP obfuscation\n"
        "\n"
        "General Options:\n"
        "  -0                 process inbound connections\n"
        "  -1                 process outbound connections\n"
        "  -4                 process IPv4 connections\n"
        "  -6                 process IPv6 connections\n"
        "  -d                 run as a daemon\n"
        "  -k                 kill the running process\n"
        "  -s                 enable silent mode\n"
        "  -w <file>          write log to <file> instead of stderr\n"
        "\n"
        "Advanced Options:\n"
        "  -f                 skip firewall rules\n"
        "  -g                 disable hop count estimation\n"
        "  -m <mark>          fwmark for bypassing the queue\n"
        "  -n <number>        netfilter queue number (0-65535)\n"
        "  -p <port>          bypass destination TCP port (repeatable)\n"
        "  -r <repeat>        duplicate generated packets for <repeat> times\n"
        "  -t <ttl>           TTL for generated packets\n"
        "  -x <mask>          set the mask for fwmark\n"
        "  -y <pct>           raise TTL dynamically to <pct>%% of estimated "
        "hops\n"
        "  -z                 use iptables commands instead of nft\n"
        "  Numeric values are decimal unless prefixed with 0x or 0X.\n"
        "\n"
        "FakeHTTP version " VERSION "\n";

    fprintf(stderr, usage_fmt, name);
}


static int parse_ull_arg(const char *value, unsigned long long min,
                         unsigned long long max, unsigned long long *result)
{
    const char *digits;
    char *end;
    int base;
    unsigned long long parsed;

    if (!value || !value[0] || value[0] == '-' ||
        isspace((unsigned char) value[0])) {
        return -1;
    }

    digits = value + (value[0] == '+');
    base = digits[0] == '0' &&
           (digits[1] == 'x' || digits[1] == 'X') ? 16 : 10;

    errno = 0;
    end = NULL;
    parsed = strtoull(value, &end, base);
    if (errno || end == value || *end || parsed < min || parsed > max) {
        return -1;
    }

    *result = parsed;
    return 0;
}


int main(int argc, char *argv[])
{
    unsigned long long tmp;
    int res, opt, exitcode;
    size_t plinfo_cap, iface_cap, plinfo_cnt, iface_cnt;
    const char *iface_info, *direction_info, *ipproto_info;
    struct payload_info *plinfo_new;
    const char **iface_new;

    exitcode = EXIT_FAILURE;

    if (!argc || !argv[0]) {
        print_usage(PROGNAME);
        return EXIT_FAILURE;
    } else if (argc == 1) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    plinfo_cap = 32;
    g_ctx.plinfo = calloc(plinfo_cap, sizeof(*g_ctx.plinfo));
    if (!g_ctx.plinfo) {
        fprintf(stderr, "%s: calloc(): %s.\n", argv[0], strerror(errno));
        goto free_mem;
    }

    iface_cap = 32;
    g_ctx.iface = calloc(iface_cap, sizeof(*g_ctx.iface));
    if (!g_ctx.iface) {
        fprintf(stderr, "%s: calloc(): %s.\n", argv[0], strerror(errno));
        goto free_mem;
    }

    plinfo_cnt = iface_cnt = 0;

    while ((opt = getopt(argc, argv, "0146ab:de:fgh:i:km:n:p:r:st:w:x:y:z")) !=
           -1) {
        switch (opt) {
            case '0':
                g_ctx.inbound = 1;
                break;

            case '1':
                g_ctx.outbound = 1;
                break;

            case '4':
                g_ctx.use_ipv4 = 1;
                break;

            case '6':
                g_ctx.use_ipv6 = 1;
                break;

            case 'a':
                g_ctx.alliface = 1;
                break;

            case 'b':
            case 'e':
            case 'h':
                if (!optarg[0]) {
                    fprintf(stderr, "%s: value of -%c cannot be empty.\n",
                            argv[0], opt);
                    print_usage(argv[0]);
                    goto free_mem;
                }

                plinfo_cnt++;
                if (plinfo_cnt >= plinfo_cap - 1) {
                    plinfo_new = realloc(
                        g_ctx.plinfo, 2 * plinfo_cap * sizeof(*g_ctx.plinfo));
                    if (!plinfo_new) {
                        fprintf(stderr, "%s: realloc(): %s.\n", argv[0],
                                strerror(errno));
                        goto free_mem;
                    }
                    g_ctx.plinfo = plinfo_new;
                    memset(&g_ctx.plinfo[plinfo_cap], 0,
                           plinfo_cap * sizeof(*g_ctx.plinfo));
                    plinfo_cap *= 2;
                }

                g_ctx.plinfo[plinfo_cnt - 1].type = opt == 'b'
                                                        ? FH_PAYLOAD_CUSTOM
                                                    : opt == 'e'
                                                        ? FH_PAYLOAD_HTTPS
                                                        : FH_PAYLOAD_HTTP;
                g_ctx.plinfo[plinfo_cnt - 1].info = optarg;
                break;

            case 'd':
                g_ctx.daemon = 1;
                break;

            case 'f':
                g_ctx.skipfw = 1;
                break;

            case 'g':
                g_ctx.nohopest = 1;
                break;

            case 'i':
                iface_cnt++;
                if (iface_cnt >= iface_cap - 1) {
                    iface_new = realloc(g_ctx.iface,
                                        2 * iface_cap * sizeof(*g_ctx.iface));
                    if (!iface_new) {
                        fprintf(stderr, "%s: realloc(): %s.\n", argv[0],
                                strerror(errno));
                        goto free_mem;
                    }
                    g_ctx.iface = iface_new;
                    memset(&g_ctx.iface[iface_cap], 0,
                           iface_cap * sizeof(*g_ctx.iface));
                    iface_cap *= 2;
                }

                if (!optarg[0]) {
                    fprintf(stderr, "%s: interface name cannot be empty.\n",
                            argv[0]);
                    print_usage(argv[0]);
                    goto free_mem;
                }

                if (strlen(optarg) > IFNAMSIZ - 1) {
                    fprintf(stderr, "%s: interface name is too long.\n",
                            argv[0]);
                    print_usage(argv[0]);
                    goto free_mem;
                }

                g_ctx.iface[iface_cnt - 1] = optarg;
                break;

            case 'k':
                g_ctx.killproc = 1;
                break;

            case 'm':
                if (parse_ull_arg(optarg, 1, UINT32_MAX, &tmp) < 0) {
                    fprintf(stderr, "%s: invalid value for -m.\n", argv[0]);
                    print_usage(argv[0]);
                    goto free_mem;
                }
                g_ctx.fwmark = tmp;
                break;

            case 'n':
                if (parse_ull_arg(optarg, 0, UINT16_MAX, &tmp) < 0) {
                    fprintf(stderr, "%s: invalid value for -n.\n", argv[0]);
                    print_usage(argv[0]);
                    goto free_mem;
                }
                g_ctx.nfqnum = tmp;
                break;

            case 'p':
                if (parse_ull_arg(optarg, 1, UINT16_MAX, &tmp) < 0 ||
                    g_ctx.bypass_port_cnt >= FH_MAX_BYPASS_PORTS) {
                    fprintf(stderr, "%s: invalid value for -p.\n", argv[0]);
                    print_usage(argv[0]);
                    goto free_mem;
                }
                g_ctx.bypass_ports[g_ctx.bypass_port_cnt++] = (uint16_t)tmp;
                break;

            case 'r':
                if (parse_ull_arg(optarg, 1, 10, &tmp) < 0) {
                    fprintf(stderr, "%s: invalid value for -r.\n", argv[0]);
                    print_usage(argv[0]);
                    goto free_mem;
                }
                g_ctx.repeat = tmp;
                break;

            case 's':
                g_ctx.silent = 1;
                break;

            case 't':
                if (parse_ull_arg(optarg, 1, UINT8_MAX, &tmp) < 0) {
                    fprintf(stderr, "%s: invalid value for -t.\n", argv[0]);
                    print_usage(argv[0]);
                    goto free_mem;
                }
                g_ctx.ttl = tmp;
                break;

            case 'w':
                g_ctx.logpath = optarg;
                if (strlen(g_ctx.logpath) > PATH_MAX - 1) {
                    fprintf(stderr, "%s: path of log file is too long.\n",
                            argv[0]);
                    print_usage(argv[0]);
                    goto free_mem;
                }
                break;

            case 'x':
                if (parse_ull_arg(optarg, 1, UINT32_MAX, &tmp) < 0) {
                    fprintf(stderr, "%s: invalid value for -x.\n", argv[0]);
                    print_usage(argv[0]);
                    goto free_mem;
                }
                g_ctx.fwmask = tmp;
                break;

            case 'y':
                if (parse_ull_arg(optarg, 1, 99, &tmp) < 0) {
                    fprintf(stderr, "%s: invalid value for -y.\n", argv[0]);
                    print_usage(argv[0]);
                    goto free_mem;
                }
                g_ctx.dynamic_pct = tmp;
                break;

            case 'z':
                g_ctx.use_iptables = 1;
                break;

            default:
                print_usage(argv[0]);
                goto free_mem;
        }
    }

    if (g_ctx.killproc) {
        res = fh_logger_setup();
        if (res < 0) {
            EE(T(fh_logger_setup));
            goto free_mem;
        }
        res = fh_kill_running(SIGTERM);
        fh_logger_cleanup();

        return res < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
    }

    if (!g_ctx.inbound && !g_ctx.outbound) {
        g_ctx.inbound = g_ctx.outbound = 1;
    }

    if (!g_ctx.use_ipv4 && !g_ctx.use_ipv6) {
        g_ctx.use_ipv4 = g_ctx.use_ipv6 = 1;
    }

    if (!g_ctx.fwmask) {
        g_ctx.fwmask = g_ctx.fwmark;
    } else if ((g_ctx.fwmark & g_ctx.fwmask) != g_ctx.fwmark) {
        fprintf(stderr, "%s: invalid value for -m/-x.\n", argv[0]);
        print_usage(argv[0]);
        goto free_mem;
    }

    if (!plinfo_cnt) {
        fprintf(stderr, "%s: option -h or -b is required.\n", argv[0]);
        print_usage(argv[0]);
        goto free_mem;
    }

    if (!g_ctx.alliface && !iface_cnt) {
        fprintf(stderr, "%s: option -i is required.\n", argv[0]);
        print_usage(argv[0]);
        goto free_mem;
    }

    if (g_ctx.dynamic_pct && g_ctx.nohopest) {
        fprintf(stderr, "%s: option -y cannot be used with -g.\n", argv[0]);
        print_usage(argv[0]);
        goto free_mem;
    }

    if (g_ctx.daemon) {
        res = daemon(0, 0);
        if (res < 0) {
            fprintf(stderr, "%s: failed to daemonize: %s\n", argv[0],
                    strerror(errno));
            goto free_mem;
        }

        if (!g_ctx.logpath) {
            g_ctx.silent = 1;
        }
    }

    srand(time(NULL));

    res = fh_logger_setup();
    if (res < 0) {
        EE(T(fh_logger_setup));
        goto free_mem;
    }

    E("FakeHTTP version " VERSION);
    E("%s", "");
    E("FakeHTTP is free software licensed under the GPLv3.");
    E("Distribution without the accompanying source code is not permitted.");
    E("%s", "");
    E("Home page: https://github.com/MikeWang000000/FakeHTTP");
    E("%s", "");

    res = fh_payload_setup();
    if (res < 0) {
        EE(T(fh_payload_setup));
        goto cleanup_logger;
    }

    res = fh_srcinfo_setup();
    if (res < 0) {
        EE(T(fh_srcinfo_setup));
        goto cleanup_payload;
    }

    res = fh_rawsend_setup();
    if (res < 0) {
        EE(T(fh_rawsend_setup));
        goto cleanup_srcinfo;
    }

    res = fh_nfq_setup();
    if (res < 0) {
        EE(T(fh_nfq_setup));
        goto cleanup_rawsend;
    }

    res = fh_nfrules_setup();
    if (res < 0) {
        EE(T(fh_nfrules_setup));
        goto cleanup_nfq;
    }

    res = fh_signal_setup();
    if (res < 0) {
        EE(T(fh_signal_setup));
        goto cleanup_nfrules;
    }

    res = setpriority(PRIO_PROCESS, getpid(), -20);
    if (res < 0) {
        EE("WARNING: setpriority(): %s", strerror(errno));
    }

    if (g_ctx.alliface) {
        iface_info = "all interfaces";
    } else if (iface_cnt > 1) {
        iface_info = "multiple interfaces";
    } else {
        iface_info = g_ctx.iface[0];
    }

    if (g_ctx.use_ipv4 && !g_ctx.use_ipv6) {
        ipproto_info = " (IPv4 only)";
    } else if (!g_ctx.use_ipv4 && g_ctx.use_ipv6) {
        ipproto_info = " (IPv6 only)";
    } else {
        ipproto_info = "";
    }

    if (g_ctx.inbound && !g_ctx.outbound) {
        direction_info = " (inbound only)";
    } else if (!g_ctx.inbound && g_ctx.outbound) {
        direction_info = " (outbound only)";
    } else {
        direction_info = "";
    }

    E("listening on %s%s%s, netfilter queue number %" PRIu32 "...", iface_info,
      ipproto_info, direction_info, g_ctx.nfqnum);

    /*
        Main Loop
    */
    res = fh_nfq_loop();
    if (res < 0) {
        EE(T(fh_nfq_loop));
        goto cleanup_nfrules;
    }

    E("exiting normally...");
    exitcode = EXIT_SUCCESS;

cleanup_nfrules:
    fh_nfrules_cleanup();

cleanup_nfq:
    fh_nfq_cleanup();

cleanup_rawsend:
    fh_rawsend_cleanup();

cleanup_srcinfo:
    fh_srcinfo_cleanup();

cleanup_payload:
    fh_payload_cleanup();

cleanup_logger:
    fh_logger_cleanup();

free_mem:
    if (g_ctx.plinfo) {
        free(g_ctx.plinfo);
    }

    if (g_ctx.iface) {
        free(g_ctx.iface);
    }

    return exitcode;
}
