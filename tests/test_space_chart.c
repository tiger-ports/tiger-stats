#include "../src/space_scan.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
/* Chart slices must be finite and proportional even at UINT64_MAX; zero
 * children remain in the list but produce no drawable slice. */
int st_space_chart_slice(const STSpaceNode *, size_t, double *, double *);
int main(void)
{
    STSpaceNode a, b, z, root;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    memset(&z, 0, sizeof(z));
    memset(&root, 0, sizeof(root));
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
