#ifndef ST_SPACE_SCAN_H
#define ST_SPACE_SCAN_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef struct STSpaceIdentity {
    dev_t dev;
    ino_t ino;
    mode_t mode;
    int valid;
} STSpaceIdentity;

/* bytes are logical file sizes (st_size), NOT allocated disk blocks.
 * Symlinks and special files contribute zero. Hard-linked regular files are
 * counted once per scan, attributed to the first path visited in name order.
 * A directory's bytes are the sum of its represented descendants, saturated
 * at UINT64_MAX; INCOMPLETE indicates totals may be lower than reality or
 * saturated. Flags propagate to ancestors only as INCOMPLETE.
 */
enum {
    ST_SPACE_DIRECTORY = 1u << 0,
    ST_SPACE_SYMLINK = 1u << 1,
    ST_SPACE_UNREADABLE = 1u << 2,
    ST_SPACE_INCOMPLETE = 1u << 3
};
typedef struct STSpaceNode {
    char *name;
    uint64_t bytes;
    unsigned flags;
    struct STSpaceNode **children;
    size_t child_count;
    STSpaceIdentity identity;
    /* Root only: identities of each absolute path prefix, including root. */
    STSpaceIdentity *prefixes;
    size_t prefix_count;
} STSpaceNode;

typedef struct STSpaceScanOptions {
    /* Includes root. Zero selects the default (100000). */
    uint64_t max_nodes;
    /* Root is depth 0. Maximum supported depth is 256. */
    unsigned max_depth;
    /* Nonzero excludes entries on other st_dev volumes. */
    int one_filesystem;
} STSpaceScanOptions;

/* NULL options: 100000 nodes, depth 128, one filesystem. With explicit
 * options, max_depth=0 means root only; one_filesystem=0 allows crossing.
 * Nodes are sorted by descending bytes, then bytewise ascending name. If a
 * node limit is reached, included directory entries depend on readdir order;
 * the affected node and ancestors are INCOMPLETE. Depth and filesystem
 * boundaries also mark INCOMPLETE. Failed metadata or directory reads mark
 * UNREADABLE|INCOMPLETE for descendants. Failure to open or verify the root
 * returns -1 with the underlying errno (ESTALE on identity mismatch); invalid
 * root/options or allocation failure also return -1, leaving *out NULL. The root
 * must be a non-symlink directory. Scan is read-only and never follows a
 * symlink intentionally. Opened directories are checked against lstat via
 * fstat(dirfd). Path-based ancestor replacement remains a TOCTOU limitation;
 * this is not a security boundary against an actively changing filesystem.
 */
int st_space_scan(const char *root, const STSpaceScanOptions *options,
                  STSpaceNode **out);
void st_space_free(STSpaceNode *node);
/* Revalidate the scanned path and tree membership; never mutates filesystem. */
int st_space_trash_validate(const STSpaceNode *root,
                            const STSpaceNode *const *ancestors, size_t count,
                            const STSpaceNode *file);

#endif
