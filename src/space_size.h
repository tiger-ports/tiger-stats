#ifndef ST_SPACE_SIZE_H
#define ST_SPACE_SIZE_H

#include <stddef.h>
#include <stdint.h>

/* Format a logical byte count (st_size bytes, never allocated blocks) in
 * readable binary units: B, KiB, MiB, GiB, TiB, PiB, EiB. The largest unit
 * with an integral part of at least one is chosen. Values are rounded
 * half-up to one decimal place with exact integer arithmetic. A round-up may
 * carry into the next unit (1023.99... KiB shows "1 MiB"; UINT64_MAX shows
 * "16 EiB"). Whole values show no decimal point. Output is ASCII and
 * fits in 24 bytes including NUL. Returns 1 on success; returns 0 without
 * touching *buf when buf is NULL or cap is too small.
 */
int st_space_format_size(uint64_t bytes, char *buf, size_t cap);

#endif
