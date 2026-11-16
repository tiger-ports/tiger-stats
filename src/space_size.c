#include "space_size.h"
#include <stdio.h>
#include <string.h>

static const char *const ST_UNITS[] = { "B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB" };

int st_space_format_size(uint64_t bytes, char *buf, size_t cap)
{
    unsigned i = 0;
    uint64_t temp = bytes, value, frac = 0, div = 1, rem;
    char out[24];
    size_t n = 0;
    int r;
    if (!buf) return 0;
    while (i < 6 && temp >= 1024) { temp /= 1024; ++i; }
    value = bytes >> (10u * i);
    if (i) {
        div = 1ULL << (10u * i);
        rem = bytes - value * div;
        frac = (rem * 10 + div / 2) / div; /* half-up, rem < 2^60 so rem*10 < 2^64 */
    }
    if (frac >= 10) {
        ++value; frac = 0;
        if (value == 1024 && i < 6) { value = 1; ++i; } /* 1024.0 of unit i is 1.0 of unit i+1 */
    }
    r = snprintf(out, sizeof out, "%llu", (unsigned long long)value);
    if (r < 0) return 0;
    n = (size_t)r;
    if (frac) {
        r = snprintf(out + n, sizeof out - n, ".%llu", (unsigned long long)frac);
        if (r < 0 || n + (size_t)r >= sizeof out) return 0;
        n += (size_t)r;
    }
    r = snprintf(out + n, sizeof out - n, " %s", ST_UNITS[i]);
    if (r < 0 || n + (size_t)r >= sizeof out) return 0;
    n += (size_t)r;
    if (n + 1 > cap) return 0;
    memcpy(buf, out, n + 1);
    return 1;
}
