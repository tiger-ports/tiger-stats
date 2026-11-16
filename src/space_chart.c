#include "space_chart.h"
int st_space_chart_slice(const STSpaceNode *node, size_t index, double *start, double *end)
{
    size_t i;
    double total = 0, before = 0;
    if (!node || !start || !end || index >= node->child_count ||
        !node->children || !node->children[index] || !node->children[index]->bytes) return 0;
    for (i = 0; i < node->child_count; ++i) {
        double value = node->children[i] ? (double)node->children[i]->bytes : 0;
        total += value;
        if (i < index) before += value;
    }
    if (total <= 0) return 0;
    *start = 360.0 * (before / total);
    *end = 360.0 * ((before + (double)node->children[index]->bytes) / total);
    if (*end > 360.0) *end = 360.0;
    return 1;
}
