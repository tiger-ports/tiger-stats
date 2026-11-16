#include "../src/space_scan.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
/* Chart slices must be finite and proportional even at UINT64_MAX; zero
 * children remain in the list but produce no drawable slice. */
int st_space_chart_slice(const STSpaceNode *, size_t, double *, double *);
int main(void)
{
    STSpaceNode a = {0}, b = {0}, z = {0}, root = {0};
    STSpaceNode *children[] = {&a, &b, &z};
    double start = -1, end = -1;
    root.children = children; root.child_count = 3;
    a.bytes = 3; b.bytes = 1;
    assert(st_space_chart_slice(&root, 0, &start, &end) == 1);
    assert(start == 0 && fabs(end - 270) < 1e-8);
    assert(st_space_chart_slice(&root, 1, &start, &end) == 1);
    assert(fabs(start - 270) < 1e-8 && fabs(end - 360) < 1e-8);
    assert(st_space_chart_slice(&root, 2, &start, &end) == 0);
    assert(st_space_chart_slice(&root, 3, &start, &end) == 0);
    assert(st_space_chart_slice(NULL, 0, &start, &end) == 0);
    a.bytes = UINT64_MAX; b.bytes = UINT64_MAX;
    assert(st_space_chart_slice(&root, 1, &start, &end) == 1);
    assert(isfinite(start) && isfinite(end) && fabs(start - 180) < 1e-8 && fabs(end - 360) < 1e-8);
    a.bytes = b.bytes = 0;
    assert(st_space_chart_slice(&root, 0, &start, &end) == 0);
    puts("PASS disk chart proportions, zero and overflow");
    return 0;
}
