#ifndef ST_SPACE_CHART_H
#define ST_SPACE_CHART_H
#include "space_scan.h"
/* Returns 1 for a positive slice, with counterclockwise angles [0,360]. */
int st_space_chart_slice(const STSpaceNode *node, size_t index, double *start, double *end);
#endif
