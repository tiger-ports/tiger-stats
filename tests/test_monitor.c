#include <stdio.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include "../src/monitor.h"

int main(void) {
    STCPUTicks before = { 100, 50, 200, 10 };
    STCPUTicks after = { 120, 60, 260, 20 };
    double percent = -1;
    if (st_cpu_percent(&before, &after, &percent) != 0 || fabs(percent - 40.0) > 0.0001) {
        fprintf(stderr, "FAIL cpu delta should report 40%% busy\n");
        return 1;
    }
    puts("PASS cpu delta");
    {
        STCPUTicks wrap_before = { UINT32_MAX - 4U, 0, UINT32_MAX - 9U, 0 };
        STCPUTicks wrap_after = { 5, 0, 10, 0 };
        if (st_cpu_percent(&wrap_before, &wrap_after, &percent) != 0 || fabs(percent - 33.3333333333) > 0.0001) {
            fprintf(stderr, "FAIL cpu counter rollover\n"); return 1;
        }
        puts("PASS cpu rollover");
    }
    percent = 123;
    if (st_cpu_percent(&before, &before, &percent) == 0 || percent != 123) {
        fprintf(stderr, "FAIL zero delta must return error and preserve output\n"); return 1;
    }
    puts("PASS cpu zero delta");
    if (st_cpu_percent(NULL, &after, &percent) == 0 ||
        st_cpu_percent(&before, NULL, &percent) == 0 ||
        st_cpu_percent(&before, &after, NULL) == 0) {
        fprintf(stderr, "FAIL null cpu parameters\n"); return 1;
    }
    puts("PASS cpu null arguments");
    {
        char buffer[128];
        STMemory memory = { 2147483648ULL, 1073741824ULL };
        if (st_format_cpu(buffer, sizeof(buffer), 40.24) != 0 || strcmp(buffer, "CPU: 40.2%") != 0) {
            fprintf(stderr, "FAIL cpu formatting: %s\n", buffer); return 1;
        }
        if (st_format_memory(buffer, sizeof(buffer), &memory) != 0 || strcmp(buffer, "Memory: 1.0 / 2.0 GiB") != 0) {
            fprintf(stderr, "FAIL memory formatting: %s\n", buffer); return 1;
        }
        puts("PASS formatting");
        strcpy(buffer, "unchanged");
        if (st_format_cpu(buffer, 3, 10.0) == 0 || strcmp(buffer, "unchanged") ||
            st_format_cpu(buffer, sizeof(buffer), -1.0) == 0 ||
            st_format_cpu(buffer, sizeof(buffer), 101.0) == 0 ||
            st_format_cpu(buffer, sizeof(buffer), NAN) == 0 ||
            st_format_cpu(NULL, 10, 10.0) == 0 ||
            st_format_memory(buffer, sizeof(buffer), NULL) == 0 ||
            st_format_memory(NULL, 10, &memory) == 0 ||
            st_format_memory(buffer, 4, &memory) == 0) {
            fprintf(stderr, "FAIL formatting errors\n"); return 1;
        }
        memory.available = memory.total + 1;
        if (st_format_memory(buffer, sizeof(buffer), &memory) == 0) {
            fprintf(stderr, "FAIL invalid memory\n"); return 1;
        }
        puts("PASS formatting errors");
    }
    {
        STCPUTicks first, second;
        STMemory memory;
        if (st_read_cpu(NULL) == 0 || st_read_memory(NULL) == 0 ||
            st_read_cpu(&first) != 0 || st_read_memory(&memory) != 0 ||
            memory.total == 0 || memory.available > memory.total) {
            fprintf(stderr, "FAIL live Mach sample\n"); return 1;
        }
        sleep(1);
        if (st_read_cpu(&second) != 0 || st_cpu_percent(&first, &second, &percent) != 0 ||
            percent < 0 || percent > 100) {
            fprintf(stderr, "FAIL live CPU delta\n"); return 1;
        }
        puts("PASS live Mach CPU and memory");
    }
    return 0;
}
