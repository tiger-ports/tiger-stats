#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
/* Readable logical sizes must be deterministic integer math in binary units
 * (B, KiB, MiB, GiB, TiB, PiB, EiB), rounded half-up to one decimal place.
 * A round-up may carry into the next unit, so 1023.99... KiB shows 1 MiB
 * and UINT64_MAX (15.99... EiB) shows 16 EiB. Values are logical st_size
 * bytes, never allocated blocks; the test checks the pure formatting only. */
int st_space_format_size(uint64_t bytes, char *buf, size_t cap);
static int expect(uint64_t bytes, const char *want)
{
    char buf[64];
    int r = st_space_format_size(bytes, buf, sizeof buf);
    if (r != 1 || strcmp(buf, want) != 0) {
        printf("FAIL %llu -> %s (expected \"%s\")\n",
               (unsigned long long)bytes, r ? buf : "(no output)", want);
        return 0;
    }
    return 1;
}
int main(void)
{
    char shortbuf[16];
    size_t i;
    int ok = 1;
    ok &= expect(0, "0 B");
    ok &= expect(1, "1 B");
    ok &= expect(640, "640 B");
    ok &= expect(1023, "1023 B");
    ok &= expect(1024, "1 KiB");
    ok &= expect(2048, "2 KiB");
    ok &= expect((1023u << 10) + 511, "1023.5 KiB");
    ok &= expect((1024u << 10) - 1, "1 MiB");
    ok &= expect(1 << 20, "1 MiB");
    ok &= expect(3u << 20, "3 MiB");
    ok &= expect((3ULL << 20) - 1, "3 MiB");
    ok &= expect(1 << 30, "1 GiB");
    ok &= expect(5ULL << 30, "5 GiB");
    ok &= expect((5ULL << 30) + (1ULL << 29), "5.5 GiB");
    ok &= expect(1ULL << 40, "1 TiB");
    ok &= expect(1ULL << 50, "1 PiB");
    ok &= expect(1ULL << 60, "1 EiB");
    ok &= expect((15ULL << 60) + (1ULL << 59), "15.5 EiB");
    ok &= expect((1ULL << 63) - 1, "8 EiB");
    ok &= expect(UINT64_MAX, "16 EiB");
    ok &= expect(1536, "1.5 KiB");
    ok &= expect(1074, "1 KiB");
    assert(ok);
    memset(shortbuf, 0x5A, sizeof shortbuf);
    assert(st_space_format_size(1024, shortbuf, sizeof shortbuf) == 1);
    assert(!strcmp(shortbuf, "1 KiB"));
    memset(shortbuf, 0x5A, sizeof shortbuf);
    assert(st_space_format_size(1536, shortbuf, 7) == 0);
    for (i = 0; i < sizeof shortbuf; ++i) assert(shortbuf[i] == 0x5A);
    assert(st_space_format_size(1536, shortbuf, 8) == 1);
    assert(!strcmp(shortbuf, "1.5 KiB"));
    memset(shortbuf, 0x5A, sizeof shortbuf);
    assert(st_space_format_size(0, shortbuf, 3) == 0);
    for (i = 0; i < sizeof shortbuf; ++i) assert(shortbuf[i] == 0x5A);
    assert(st_space_format_size(0, shortbuf, 4) == 1);
    assert(!strcmp(shortbuf, "0 B"));
    assert(st_space_format_size(0, NULL, 16) == 0);
    assert(st_space_format_size(UINT64_MAX, NULL, 16) == 0);
    puts("PASS logical size formatting: zero, boundaries, units, rounding, UINT64_MAX, buffers");
    return 0;
}
