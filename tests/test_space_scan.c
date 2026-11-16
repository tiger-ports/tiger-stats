#include "../src/space_scan.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* Compile with -DST_SPACE_TESTING. Inject failures without relying on UID,
 * ACLs, giant sparse files, or privileges. Paths stay inside our fixture. */
static int deny_secret, deny_root, swap_root, fail_root_recheck, root_lstat_count;
static int huge_files, foreign_dir, foreign_file, swap_dir;
static char swap_target[1024], root_target[1024];
int st_test_lstat(const char *p, struct stat *s)
{
    int r;
    if (fail_root_recheck && strcmp(p, root_target) == 0 && ++root_lstat_count == 2) {
        errno = EACCES; return -1;
    }
    r = lstat(p, s);
    if (!r && huge_files && strstr(p, "/huge")) s->st_size = INT64_MAX;
    if (!r && foreign_dir && strstr(p, "/foreign")) s->st_dev += 1;
    if (!r && foreign_file && strstr(p, "/beta")) s->st_dev += 1;
    return r;
}
DIR *st_test_opendir(const char *p)
{
    if (deny_root && strcmp(p, root_target) == 0) { errno = EACCES; return NULL; }
    if (swap_root && strcmp(p, root_target) == 0) return opendir(swap_target);
    if (deny_secret && strstr(p, "/secret")) { errno = EACCES; return NULL; }
    if (swap_dir && strstr(p, "/secret")) return opendir(swap_target);
    return opendir(p);
}
static void path(char *out, size_t n, const char *dir, const char *name)
{
    int r = snprintf(out, n, "%s/%s", dir, name);
    assert(r > 0 && (size_t)r < n);
}
static void file(const char *p, const char *data)
{
    FILE *f = fopen(p, "wb"); assert(f);
    assert(fwrite(data, 1, strlen(data), f) == strlen(data));
    assert(fclose(f) == 0);
}
static STSpaceNode *child(STSpaceNode *p, const char *name)
{
    size_t i;
    for (i = 0; i < p->child_count; ++i)
        if (!strcmp(p->children[i]->name, name)) return p->children[i];
    return NULL;
}
static void check_sorted(STSpaceNode *n)
{
    size_t i;
    for (i = 1; i < n->child_count; ++i) {
        STSpaceNode *a = n->children[i-1], *b = n->children[i];
        assert(a->bytes > b->bytes || (a->bytes == b->bytes && strcmp(a->name, b->name) < 0));
    }
    for (i = 0; i < n->child_count; ++i) check_sorted(n->children[i]);
}
int main(void)
{
    char dir[1024];
    const char *tmp = getenv("TMPDIR");
    char a[1024], b[1024], c[1024], d[1024];
    STSpaceNode *n, *x;
    STSpaceScanOptions opt = { 100, 8, 1 };
    assert(tmp && *tmp);
    path(dir, sizeof(dir), tmp, "space-scan-XXXXXX");
    assert(mkdtemp(dir));
    assert(strlen(dir) < sizeof(root_target));
    strcpy(root_target, dir);
    deny_root = 1; n = (STSpaceNode *)1; errno = 0;
    if (st_space_scan(dir, NULL, &n) != -1 || n != NULL || errno != EACCES) {
        fprintf(stderr, "FAIL inaccessible root must return EACCES and no tree\n");
        if (n && n != (STSpaceNode *)1) st_space_free(n);
        rmdir(dir);
        return 1;
    }
    deny_root = 0;
    path(a, sizeof(a), dir, "nested"); assert(!mkdir(a, 0700));
    fail_root_recheck = 1; root_lstat_count = 0; n = (STSpaceNode *)1; errno = 0;
    assert(st_space_scan(dir, NULL, &n) == -1 && n == NULL && errno == EACCES);
    fail_root_recheck = 0;
    strcpy(swap_target, a); swap_root = 1; n = (STSpaceNode *)1; errno = 0;
    assert(st_space_scan(dir, NULL, &n) == -1 && n == NULL && errno == ESTALE);
    swap_root = 0;
    path(b, sizeof(b), a, "inside"); file(b, "1234567");
    path(a, sizeof(a), dir, "alpha"); file(a, "abcd");
    path(b, sizeof(b), dir, "beta"); file(b, "abcd");
    path(c, sizeof(c), dir, "hard"); assert(!link(a, c));
    path(c, sizeof(c), dir, "loop"); assert(!symlink(".", c));
    path(c, sizeof(c), dir, "outside"); assert(!symlink("/", c));
    path(c, sizeof(c), dir, "empty"); file(c, "");
    path(c, sizeof(c), dir, "secret"); assert(!mkdir(c, 0700));
    path(d, sizeof(d), c, "hidden"); file(d, "12");
    path(c, sizeof(c), dir, "foreign"); assert(!mkdir(c, 0700));
    path(d, sizeof(d), c, "item"); file(d, "xyz");
    assert(!st_space_scan(dir, NULL, &n));
    assert(n->bytes == 20 && n->flags == ST_SPACE_DIRECTORY);
    assert(child(n, "nested")->bytes == 7);
    assert(child(n, "alpha")->bytes == 4 && child(n, "hard")->bytes == 0);
    assert(child(n, "empty")->bytes == 0);
    assert(child(n, "loop")->flags == ST_SPACE_SYMLINK && child(n, "loop")->bytes == 0);
    assert(child(n, "outside")->flags == ST_SPACE_SYMLINK);
    check_sorted(n); st_space_free(n);
    deny_secret = 1;
    assert(!st_space_scan(dir, &opt, &n)); x = child(n, "secret");
    assert((x->flags & (ST_SPACE_UNREADABLE|ST_SPACE_INCOMPLETE)) == (ST_SPACE_UNREADABLE|ST_SPACE_INCOMPLETE));
    assert(n->flags & ST_SPACE_INCOMPLETE); st_space_free(n); deny_secret = 0;
    path(swap_target, sizeof(swap_target), dir, "nested"); swap_dir = 1;
    assert(!st_space_scan(dir, &opt, &n)); x = child(n, "secret");
    assert(x->flags & ST_SPACE_UNREADABLE); assert(!x->child_count); st_space_free(n); swap_dir = 0;
    foreign_dir = 1;
    assert(!st_space_scan(dir, &opt, &n)); x = child(n, "foreign");
    assert(x->flags & ST_SPACE_INCOMPLETE); assert(!x->child_count); st_space_free(n);
    opt.one_filesystem = 0; /* injected device differs between lstat and fstat, so mismatch is rejected */
    assert(!st_space_scan(dir, &opt, &n)); assert(child(n, "foreign")->flags & ST_SPACE_UNREADABLE); st_space_free(n);
    foreign_dir = 0; opt.one_filesystem = 1;
    foreign_file = 1;
    assert(!st_space_scan(dir, &opt, &n));
    assert(child(n, "beta")->bytes == 0 && (child(n, "beta")->flags & ST_SPACE_INCOMPLETE));
    assert(n->bytes == 16 && (n->flags & ST_SPACE_INCOMPLETE)); st_space_free(n);
    foreign_file = 0;
    opt.max_depth = 1;
    assert(!st_space_scan(dir, &opt, &n)); assert(n->flags & ST_SPACE_INCOMPLETE);
    assert(child(n, "nested")->flags & ST_SPACE_INCOMPLETE); st_space_free(n);
    opt.max_depth = 0; assert(!st_space_scan(dir, &opt, &n));
    assert(n->flags & ST_SPACE_INCOMPLETE); assert(n->child_count == 0); st_space_free(n);
    opt.max_depth = 8; opt.max_nodes = 2;
    assert(!st_space_scan(dir, &opt, &n)); assert(n->flags & ST_SPACE_INCOMPLETE); assert(n->child_count == 1); st_space_free(n);
    opt.max_depth = 257; n = (STSpaceNode *)1;
    assert(st_space_scan(dir, &opt, &n) == -1 && !n);
    path(a, sizeof(a), dir, "loop"); n = (STSpaceNode *)1;
    assert(st_space_scan(a, NULL, &n) == -1 && !n);
    assert(strlen(a) + 1 < sizeof(a)); strcat(a, "/");
    assert(st_space_scan(a, NULL, &n) == -1 && !n);
    assert(st_space_scan("/no/such/path/st-space-test", NULL, &n) == -1 && !n);
    assert(st_space_scan(dir, NULL, NULL) == -1);
    path(a, sizeof(a), dir, "huge1"); file(a, "x");
    path(a, sizeof(a), dir, "huge2"); file(a, "x");
    path(a, sizeof(a), dir, "huge3"); file(a, "x");
    huge_files = 1; assert(!st_space_scan(dir, NULL, &n));
    assert(n->bytes == UINT64_MAX && (n->flags & ST_SPACE_INCOMPLETE)); st_space_free(n); huge_files = 0;
    /* Delete only known fixture entries, never recursively delete user paths. */
    path(a, sizeof(a), dir, "huge1"); assert(!unlink(a));
    path(a, sizeof(a), dir, "huge2"); assert(!unlink(a));
    path(a, sizeof(a), dir, "huge3"); assert(!unlink(a));
    path(a, sizeof(a), dir, "alpha"); assert(!unlink(a));
    path(a, sizeof(a), dir, "beta"); assert(!unlink(a));
    path(a, sizeof(a), dir, "hard"); assert(!unlink(a));
    path(a, sizeof(a), dir, "empty"); assert(!unlink(a));
    path(a, sizeof(a), dir, "loop"); assert(!unlink(a));
    path(a, sizeof(a), dir, "outside"); assert(!unlink(a));
    path(a, sizeof(a), dir, "nested"); path(b, sizeof(b), a, "inside"); assert(!unlink(b)); assert(!rmdir(a));
    path(a, sizeof(a), dir, "secret"); path(b, sizeof(b), a, "hidden"); assert(!unlink(b)); assert(!rmdir(a));
    path(a, sizeof(a), dir, "foreign"); path(b, sizeof(b), a, "item"); assert(!unlink(b)); assert(!rmdir(a));
    assert(!rmdir(dir));
    puts("space scan tests passed: fixture, sorting, links, errors, caps, overflow, races");
    return 0;
}
