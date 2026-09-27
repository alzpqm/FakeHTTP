/* Pure-function regression: does not create sockets or change firewall rules.
 */
#include "../src/rawsend.c"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    int ttl, initial, hops, pct;

    for (ttl = 1; ttl <= 255; ttl++) {
        initial = ttl <= 32 ? 32 : ttl <= 64 ? 64 : ttl <= 128 ? 128 : 255;
        hops = hop_estimate((uint8_t) ttl);
        if (hops != initial - ttl) {
            fprintf(stderr, "unexpected hop estimate for TTL %d: %d\n", ttl,
                    hops);
            return EXIT_FAILURE;
        }
        g_ctx.ttl = 3;
        for (pct = 0; pct <= 50; pct += 25) {
            g_ctx.dynamic_pct = pct;
            if (hops > g_ctx.ttl && calc_snd_ttl(hops) >= hops) {
                fprintf(stderr, "decoy TTL not below estimated hops\n");
                return EXIT_FAILURE;
            }
        }
    }
    puts("TTL estimation: 255 boundary cases passed");
    return EXIT_SUCCESS;
}
