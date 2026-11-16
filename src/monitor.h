#ifndef STATS_TIGER_MONITOR_H
#define STATS_TIGER_MONITOR_H
#include <stddef.h>
#include <stdint.h>

typedef struct { uint32_t user, system, idle, nice; } STCPUTicks;
typedef struct { uint64_t total, available; } STMemory;
typedef struct { uint64_t total, available; } STDisk;

/* Returns 0 on success; outputs are not modified on failure. */
int st_disk_from_blocks(uint64_t blocks, uint64_t available_blocks, uint64_t block_size, STDisk *disk);
int st_read_disk(STDisk *disk);
int st_format_disk(char *buffer, size_t size, const STDisk *disk);

int st_cpu_percent(const STCPUTicks *before, const STCPUTicks *after, double *percent);
int st_read_cpu(STCPUTicks *ticks);
int st_read_memory(STMemory *memory);
int st_format_cpu(char *buffer, size_t size, double percent);
int st_format_memory(char *buffer, size_t size, const STMemory *memory);
#endif
