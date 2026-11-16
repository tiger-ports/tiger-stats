#include "../src/monitor.h"
#include <sys/types.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

int sysctlbyname(const char *name, void *oldp, size_t *oldlenp, void *newp, size_t newlen)
{
    uint32_t misleading = 2147483648U;
    (void)newp; (void)newlen;
    if (strcmp(name, "hw.physmem") == 0 && oldp && oldlenp && *oldlenp >= sizeof(misleading)) {
        memcpy(oldp, &misleading, sizeof(misleading));
        *oldlenp = sizeof(misleading);
        return 0;
    }
    errno = ENOENT;
    return -1;
}

int main(void)
{
    STMemory memory;
    if (st_read_memory(&memory) == 0) {
        fprintf(stderr, "FAIL unreliable 32-bit physical-memory fallback accepted\n");
        return 1;
    }
    puts("PASS unavailable memory when accurate total cannot be read");
    return 0;
}
