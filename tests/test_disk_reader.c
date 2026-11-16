#include "../src/monitor.h"
#include <sys/types.h>
#include <sys/mount.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

static int scenario, calls, wrong_path;
int statfs(const char *path, struct statfs *fs)
{
    calls++;
    if (strcmp(path, "/") != 0) wrong_path = 1;
    if (scenario == 1) { errno = EIO; return -1; }
    memset(fs, 0, sizeof(*fs));
    fs->f_bsize = 4096;
    fs->f_blocks = 524288;
    fs->f_bfree = 300000;
    fs->f_bavail = 262144;
    if (scenario == 2) fs->f_bavail = -1;
    if (scenario == 3) fs->f_blocks = 0;
    if (scenario == 4) fs->f_bsize = 0;
    if (scenario == 5) fs->f_bavail = 600000;
    if (scenario == 6) fs->f_blocks = -1;
    if (scenario == 7) fs->f_blocks = (__typeof__(fs->f_blocks))((uint64_t)1 << 62);
    return 0;
}

int main(void)
{
    STDisk disk = {7, 3};
    if (st_read_disk(NULL) == 0 || calls != 0 || st_read_disk(&disk) != 0 ||
        wrong_path || calls != 1 || disk.total != 2147483648ULL ||
        disk.available != 1073741824ULL) {
        fprintf(stderr, "FAIL root statfs availability (not total free)\n"); return 1;
    }
    puts("PASS root statfs reports non-superuser available blocks");
    for (scenario = 1; scenario <= 7; scenario++) {
        if (scenario == 7 && sizeof(((struct statfs *)0)->f_blocks) < sizeof(uint64_t)) {
            puts("SKIP 64-bit statfs overflow scenario on narrower block-count ABI");
            continue;
        }
        disk.total = 7; disk.available = 3;
        if (st_read_disk(&disk) == 0 || disk.total != 7 || disk.available != 3) {
            fprintf(stderr, "FAIL statfs error/invalid scenario %d\n", scenario); return 1;
        }
    }
    puts("PASS statfs failures and invalid samples preserve output");
    return 0;
}
