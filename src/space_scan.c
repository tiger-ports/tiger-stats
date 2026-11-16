#include "space_scan.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef ST_SPACE_TESTING
extern int st_test_lstat(const char *, struct stat *);
extern DIR *st_test_opendir(const char *);
#define scan_lstat st_test_lstat
#define scan_opendir st_test_opendir
#else
#define scan_lstat lstat
#define scan_opendir opendir
#endif

#define ST_MAX_DEPTH 256u
#define ST_DEFAULT_NODES 100000u

typedef struct {
    dev_t dev;
    ino_t ino;
    int used;
} Seen;
typedef struct {
    STSpaceNode *node;
    char *path;
    size_t next;
    unsigned depth;
    int entered;
} Frame;
typedef struct {
    Seen *slots;
    size_t cap, count;
} SeenSet;

static char *copy_string(const char *s)
{
    size_t n = strlen(s);
    char *p;
    if (n == (size_t)-1) return NULL;
    p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}
static STSpaceNode *new_node(const char *name)
{
    STSpaceNode *p = (STSpaceNode *)calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->name = copy_string(name);
    if (!p->name) { free(p); return NULL; }
    return p;
}
static STSpaceIdentity identity(const struct stat *st)
{
    STSpaceIdentity i;
    i.dev = st->st_dev; i.ino = st->st_ino; i.mode = st->st_mode; i.valid = 1;
    return i;
}
/* A noncanonical or inaccessible root is still scannable, but not trashable. */
static void capture_prefixes(STSpaceNode *root, const char *path, const struct stat *rootstat)
{
    size_t i, n, count = 1, index = 0;
    char *prefix;
    STSpaceIdentity *ids;
    struct stat st;
    if (!path || path[0] != '/') return;
    n = strlen(path);
    if (!n || (n > 1 && path[n-1] == '/')) return;
    for (i = 1; i < n; ++i) {
        if (path[i] == '/') {
            if (path[i-1] == '/') return;
            ++count;
        }
    }
    if (n > 1) ++count;
    prefix = copy_string(path);
    ids = (STSpaceIdentity *)calloc(count, sizeof(*ids));
    if (!prefix || !ids) { free(prefix); free(ids); return; }
    if (n == 1) ids[index++] = identity(rootstat);
    else {
        if (scan_lstat("/", &st) || !S_ISDIR(st.st_mode)) goto fail;
        ids[index++] = identity(&st);
        for (i = 1; i <= n; ++i) if (path[i] == '/' || path[i] == '\0') {
            size_t start = i;
            while (start && path[start-1] != '/') --start;
            if (i == start || (i-start == 1 && path[start] == '.') ||
                (i-start == 2 && path[start] == '.' && path[start+1] == '.')) goto fail;
            prefix[i] = '\0';
            if (i == n) st = *rootstat;
            else if (scan_lstat(prefix, &st) || !S_ISDIR(st.st_mode)) goto fail;
            ids[index++] = identity(&st); prefix[i] = path[i];
        }
    }
    free(prefix); root->prefixes = ids; root->prefix_count = index; return;
fail:
    free(prefix); free(ids);
}
void st_space_free(STSpaceNode *node)
{
    /* Iterative postorder by using the already-owned child arrays as a stack:
     * each child is freed after its own children. No call-stack depth risk. */
    STSpaceNode *stack[ST_MAX_DEPTH + 2];
    size_t level = 0;
    if (!node) return;
    stack[0] = node;
    for (;;) {
        STSpaceNode *p = stack[level];
        if (p->child_count) {
            STSpaceNode *child = p->children[--p->child_count];
            stack[++level] = child;
        } else {
            free(p->children); free(p->prefixes); free(p->name); free(p);
            if (!level) break;
            --level;
        }
    }
}
static size_t hash_key(dev_t dev, ino_t ino)
{
    uint64_t x = (uint64_t)ino ^ ((uint64_t)dev * UINT64_C(0x9e3779b97f4a7c15));
    x ^= x >> 33; x *= UINT64_C(0xff51afd7ed558ccd); x ^= x >> 33;
    return (size_t)x;
}
/* Return 1 if already seen, 0 if inserted, -1 on allocation failure. */
static int seen_add(SeenSet *set, dev_t dev, ino_t ino)
{
    size_t i;
    if (!set->cap || set->count >= set->cap / 2) {
        size_t oldcap = set->cap, newcap = oldcap ? oldcap * 2 : 64;
        Seen *old = set->slots, *slots;
        if (newcap < oldcap || newcap > (size_t)-1 / sizeof(*slots)) return -1;
        slots = (Seen *)calloc(newcap, sizeof(*slots));
        if (!slots) return -1;
        set->slots = slots; set->cap = newcap;
        for (i = 0; i < oldcap; ++i) if (old[i].used) {
            size_t j = hash_key(old[i].dev, old[i].ino) & (newcap - 1);
            while (slots[j].used) j = (j + 1) & (newcap - 1);
            slots[j] = old[i];
        }
        free(old);
    }
    i = hash_key(dev, ino) & (set->cap - 1);
    while (set->slots[i].used) {
        if (set->slots[i].dev == dev && set->slots[i].ino == ino) return 1;
        i = (i + 1) & (set->cap - 1);
    }
    set->slots[i].used = 1; set->slots[i].dev = dev; set->slots[i].ino = ino;
    ++set->count;
    return 0;
}
static int by_name(const void *a, const void *b)
{
    const STSpaceNode *x = *(STSpaceNode * const *)a;
    const STSpaceNode *y = *(STSpaceNode * const *)b;
    return strcmp(x->name, y->name);
}
static int by_size(const void *a, const void *b)
{
    const STSpaceNode *x = *(STSpaceNode * const *)a;
    const STSpaceNode *y = *(STSpaceNode * const *)b;
    if (x->bytes != y->bytes) return x->bytes > y->bytes ? -1 : 1;
    return strcmp(x->name, y->name);
}
static char *join_path(const char *parent, const char *name)
{
    size_t a = strlen(parent), b = strlen(name), slash = a && parent[a-1] != '/';
    char *p;
    if (a > (size_t)-1 - slash || a + slash > (size_t)-1 - b - 1) return NULL;
    p = (char *)malloc(a + slash + b + 1);
    if (!p) return NULL;
    memcpy(p, parent, a); if (slash) p[a++] = '/';
    memcpy(p + a, name, b + 1);
    return p;
}
static int collect(Frame *f, uint64_t *count, uint64_t limit, const struct stat *before)
{
    DIR *d;
    struct stat opened;
    struct dirent *entry;
    size_t cap = 0;
    d = scan_opendir(f->path);
    if (!d) {
        if (f->depth == 0) return -2;
        f->node->flags |= ST_SPACE_UNREADABLE | ST_SPACE_INCOMPLETE;
        return 0;
    }
    if (fstat(dirfd(d), &opened)) {
        int saved = errno;
        closedir(d);
        if (f->depth == 0) { errno = saved; return -2; }
        f->node->flags |= ST_SPACE_UNREADABLE | ST_SPACE_INCOMPLETE;
        return 0;
    }
    if (opened.st_dev != before->st_dev || opened.st_ino != before->st_ino || !S_ISDIR(opened.st_mode)) {
        closedir(d);
        if (f->depth == 0) { errno = ESTALE; return -2; }
        f->node->flags |= ST_SPACE_UNREADABLE | ST_SPACE_INCOMPLETE;
        return 0;
    }
    for (;;) {
        STSpaceNode *child;
        errno = 0; entry = readdir(d);
        if (!entry) {
            if (errno) f->node->flags |= ST_SPACE_UNREADABLE | ST_SPACE_INCOMPLETE;
            break;
        }
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        if (*count >= limit) { f->node->flags |= ST_SPACE_INCOMPLETE; break; }
        if (f->node->child_count == cap) {
            size_t n = cap ? cap * 2 : 8;
            STSpaceNode **p;
            if (n < cap || n > (size_t)-1 / sizeof(*p)) { closedir(d); return -1; }
            p = (STSpaceNode **)realloc(f->node->children, n * sizeof(*p));
            if (!p) { closedir(d); return -1; }
            f->node->children = p; cap = n;
        }
        child = new_node(entry->d_name);
        if (!child) { closedir(d); return -1; }
        f->node->children[f->node->child_count++] = child;
        ++*count;
    }
    if (closedir(d)) f->node->flags |= ST_SPACE_UNREADABLE | ST_SPACE_INCOMPLETE;
    if (f->node->child_count > 1)
        qsort(f->node->children, f->node->child_count, sizeof(*f->node->children), by_name);
    return 0;
}
int st_space_scan(const char *root, const STSpaceScanOptions *options, STSpaceNode **out)
{
    STSpaceScanOptions opt;
    struct stat rootstat;
    STSpaceNode *result = NULL;
    char *rootpath;
    size_t rootlen;
    Frame stack[ST_MAX_DEPTH + 1];
    SeenSet seen = { NULL, 0, 0 };
    size_t level = 0;
    uint64_t count = 1;
    int error = 0;
    if (!out) { errno = EINVAL; return -1; }
    *out = NULL;
    if (!root || !*root) { errno = EINVAL; return -1; }
    if (options) opt = *options;
    else { opt.max_nodes = ST_DEFAULT_NODES; opt.max_depth = 128; opt.one_filesystem = 1; }
    if (!opt.max_nodes) opt.max_nodes = ST_DEFAULT_NODES;
    if (opt.max_depth > ST_MAX_DEPTH) { errno = EINVAL; return -1; }
    rootpath = copy_string(root);
    if (!rootpath) { errno = ENOMEM; return -1; }
    rootlen = strlen(rootpath);
    while (rootlen > 1 && rootpath[rootlen-1] == '/') rootpath[--rootlen] = '\0';
    if (scan_lstat(rootpath, &rootstat)) { int saved = errno; free(rootpath); errno = saved; return -1; }
    if (!S_ISDIR(rootstat.st_mode)) { free(rootpath); errno = ENOTDIR; return -1; }
    result = new_node(root);
    if (!result) { free(rootpath); errno = ENOMEM; return -1; }
    capture_prefixes(result, root, &rootstat);
    memset(stack, 0, sizeof(stack));
    stack[0].node = result; stack[0].path = rootpath;
    for (;;) {
        Frame *f = &stack[level];
        if (!f->entered) {
            struct stat st;
            int stat_result;
            f->entered = 1;
            stat_result = scan_lstat(f->path, &st);
            if (stat_result || (level == 0 &&
                (st.st_dev != rootstat.st_dev || st.st_ino != rootstat.st_ino || !S_ISDIR(st.st_mode)))) {
                if (level == 0) { error = stat_result ? errno : ESTALE; break; }
                f->node->flags |= ST_SPACE_UNREADABLE | ST_SPACE_INCOMPLETE;
            } else if ((f->node->identity = identity(&st)), S_ISLNK(st.st_mode)) {
                f->node->flags |= ST_SPACE_SYMLINK;
            } else if (S_ISDIR(st.st_mode)) {
                f->node->flags |= ST_SPACE_DIRECTORY;
                if ((opt.one_filesystem && st.st_dev != rootstat.st_dev) || f->depth >= opt.max_depth)
                    f->node->flags |= ST_SPACE_INCOMPLETE;
                else {
                    int collected = collect(f, &count, opt.max_nodes, &st);
                    if (collected) { error = collected == -2 ? errno : ENOMEM; break; }
                }
            } else if (S_ISREG(st.st_mode)) {
                int duplicate;
                if (opt.one_filesystem && st.st_dev != rootstat.st_dev) {
                    f->node->flags |= ST_SPACE_INCOMPLETE;
                    continue;
                }
                duplicate = seen_add(&seen, st.st_dev, st.st_ino);
                if (duplicate < 0) { error = ENOMEM; break; }
                if (st.st_size < 0) f->node->flags |= ST_SPACE_INCOMPLETE;
                else if (!duplicate) f->node->bytes = (uint64_t)st.st_size;
            }
        }
        if (f->next < f->node->child_count) {
            STSpaceNode *child = f->node->children[f->next++];
            char *path = join_path(f->path, child->name);
            if (!path) { error = ENOMEM; break; }
            memset(&stack[level + 1], 0, sizeof(stack[level + 1]));
            stack[level + 1].node = child; stack[level + 1].path = path;
            stack[level + 1].depth = f->depth + 1;
            ++level;
        } else {
            if (f->node->child_count > 1)
                qsort(f->node->children, f->node->child_count, sizeof(*f->node->children), by_size);
            free(f->path); f->path = NULL;
            if (!level) break;
            {
                STSpaceNode *parent = stack[level-1].node;
                if (UINT64_MAX - parent->bytes < f->node->bytes) {
                    parent->bytes = UINT64_MAX; parent->flags |= ST_SPACE_INCOMPLETE;
                } else parent->bytes += f->node->bytes;
                if (f->node->flags & ST_SPACE_INCOMPLETE) parent->flags |= ST_SPACE_INCOMPLETE;
            }
            --level;
        }
    }
    free(seen.slots);
    if (error) {
        size_t i;
        for (i = 0; i <= level; ++i) free(stack[i].path);
        st_space_free(result); errno = error; return -1;
    }
    *out = result;
    return 0;
}
