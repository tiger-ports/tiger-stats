#include "monitor.h"
#include <stdio.h>
#include <string.h>
#include <mach/mach.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/mount.h>

static int copy_formatted(char *buffer, size_t size, const char *text, int length)
{
    if (!buffer || length < 0 || (size_t)length >= size) return -1;
    memcpy(buffer, text, (size_t)length + 1);
    return 0;
}

int st_disk_from_blocks(uint64_t blocks, uint64_t available_blocks, uint64_t block_size, STDisk *disk)
{
    if (!disk || !blocks || !block_size || available_blocks > blocks ||
        blocks > UINT64_MAX / block_size) return -1;
    disk->total = blocks * block_size;
    disk->available = available_blocks * block_size;
    return 0;
}

int st_read_disk(STDisk *disk)
{
    struct statfs fs;
    uint64_t blocks, available, block_size;
    if (!disk || statfs("/", &fs) != 0) return -1;
    blocks = (uint64_t)fs.f_blocks;
    available = (uint64_t)fs.f_bavail;
    block_size = (uint64_t)fs.f_bsize;
    /* Reject negative values from older signed statfs fields and implausible
       modern values outside the signed range before any multiplication. */
    if (blocks > INT64_MAX || available > INT64_MAX || block_size > INT64_MAX) return -1;
    return st_disk_from_blocks(blocks, available, block_size, disk);
}

int st_format_disk(char *buffer, size_t size, const STDisk *disk)
{
    char text[128];
    int n;
    if (!disk || !disk->total || disk->available > disk->total) return -1;
    n = snprintf(text, sizeof(text), "Root disk available: %.1f / %.1f GiB",
                 (double)disk->available / 1073741824.0,
                 (double)disk->total / 1073741824.0);
    if (n < 0 || (size_t)n >= sizeof(text)) return -1;
    return copy_formatted(buffer, size, text, n);
}

int st_format_cpu(char *buffer, size_t size, double percent)
{
    char text[64];
    int n;
    if (!(percent >= 0.0 && percent <= 100.0)) return -1;
    n = snprintf(text, sizeof(text), "CPU: %.1f%%", percent);
    if (n < 0 || (size_t)n >= sizeof(text)) return -1;
    return copy_formatted(buffer, size, text, n);
}

int st_format_memory(char *buffer, size_t size, const STMemory *memory)
{
    char text[128];
    int n;
    if (!memory || !memory->total || memory->available > memory->total) return -1;
    n = snprintf(text, sizeof(text), "Memory: %.1f / %.1f GiB",
                 (double)(memory->total - memory->available) / 1073741824.0,
                 (double)memory->total / 1073741824.0);
    if (n < 0 || (size_t)n >= sizeof(text)) return -1;
    return copy_formatted(buffer, size, text, n);
}

int st_read_cpu(STCPUTicks *ticks)
{
    host_cpu_load_info_data_t info;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    mach_port_t host;
    kern_return_t result;
    STCPUTicks sample;
    if (!ticks) return -1;
    host = mach_host_self();
    result = host_statistics(host, HOST_CPU_LOAD_INFO, (host_info_t)&info, &count);
    mach_port_deallocate(mach_task_self(), host);
    if (result != KERN_SUCCESS || count < HOST_CPU_LOAD_INFO_COUNT) return -1;
    sample.user = info.cpu_ticks[CPU_STATE_USER];
    sample.system = info.cpu_ticks[CPU_STATE_SYSTEM];
    sample.idle = info.cpu_ticks[CPU_STATE_IDLE];
    sample.nice = info.cpu_ticks[CPU_STATE_NICE];
    *ticks = sample;
    return 0;
}

static int physical_memory(const char *name, uint64_t *total)
{
    unsigned char bytes[8];
    size_t size = sizeof(bytes);
    uint64_t v64;
    if (sysctlbyname(name, bytes, &size, NULL, 0) != 0 || size != sizeof(v64)) return -1;
    memcpy(&v64, bytes, sizeof(v64));
    *total = v64;
    return *total ? 0 : -1;
}

int st_read_memory(STMemory *memory)
{
    vm_statistics_data_t info;
    mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
    vm_size_t page_size;
    mach_port_t host;
    kern_return_t vm_result, page_result;
    STMemory sample;
    if (!memory) return -1;
    if (physical_memory("hw.memsize", &sample.total) != 0) return -1;
    host = mach_host_self();
    vm_result = host_statistics(host, HOST_VM_INFO, (host_info_t)&info, &count);
    page_result = host_page_size(host, &page_size);
    mach_port_deallocate(mach_task_self(), host);
    if (vm_result != KERN_SUCCESS || page_result != KERN_SUCCESS ||
        count < HOST_VM_INFO_COUNT || page_size == 0) return -1;
    sample.available = ((uint64_t)info.free_count + info.inactive_count) * page_size;
    if (sample.available > sample.total) sample.available = sample.total;
    *memory = sample;
    return 0;
}

int st_cpu_percent(const STCPUTicks *before, const STCPUTicks *after, double *percent)
{
    uint64_t user, system, idle, nice, total;
    if (!before || !after || !percent) return -1;
    user = (uint32_t)(after->user - before->user);
    system = (uint32_t)(after->system - before->system);
    idle = (uint32_t)(after->idle - before->idle);
    nice = (uint32_t)(after->nice - before->nice);
    total = user + system + idle + nice;
    if (total == 0) return -1;
    *percent = 100.0 * (double)(user + system + nice) / (double)total;
    return 0;
}
