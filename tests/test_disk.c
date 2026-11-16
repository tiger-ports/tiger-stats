#include "../src/monitor.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    STDisk disk;
    char text[128];
    if (st_disk_from_blocks(524288, 262144, 4096, &disk) != 0 ||
        disk.total != 2147483648ULL || disk.available != 1073741824ULL ||
        st_format_disk(text, sizeof(text), &disk) != 0 ||
        strcmp(text, "Root disk available: 1.0 / 2.0 GiB") != 0) {
        fprintf(stderr, "FAIL disk available capacity and formatting\n"); return 1;
    }
    puts("PASS disk available capacity and formatting");
    disk.total = 7; disk.available = 3;
    if (st_disk_from_blocks(0, 0, 4096, &disk) == 0 ||
        st_disk_from_blocks(1, 0, 0, &disk) == 0 ||
        st_disk_from_blocks(1, 2, 4096, &disk) == 0 ||
        st_disk_from_blocks(UINT64_MAX, 0, 2, &disk) == 0 ||
        disk.total != 7 || disk.available != 3) {
        fprintf(stderr, "FAIL disk invalid/overflow values or modified output\n"); return 1;
    }
    if (st_disk_from_blocks(1, 0, 4096, &disk) != 0 || disk.available != 0 || disk.total != 4096) {
        fprintf(stderr, "FAIL zero available disk capacity\n"); return 1;
    }
    puts("PASS disk invalid values, overflow and zero availability");
    disk.total = 0; disk.available = 0;
    strcpy(text, "unchanged");
    if (st_format_disk(text, sizeof(text), &disk) == 0 || strcmp(text, "unchanged") != 0) {
        fprintf(stderr, "FAIL disk format accepts zero total\n"); return 1;
    }
    disk.total = 1; disk.available = 2;
    if (st_format_disk(text, sizeof(text), &disk) == 0 || strcmp(text, "unchanged") != 0 ||
        st_format_disk(text, sizeof(text), NULL) == 0 ||
        st_format_disk(NULL, sizeof(text), &disk) == 0 ||
        st_disk_from_blocks(1, 0, 1, NULL) == 0) {
        fprintf(stderr, "FAIL disk format invalid inputs\n"); return 1;
    }
    disk.available = 0;
    if (st_format_disk(text, 4, &disk) == 0 || strcmp(text, "unchanged") != 0) {
        fprintf(stderr, "FAIL disk format small buffer\n"); return 1;
    }
    puts("PASS disk formatting errors");
    return 0;
}
