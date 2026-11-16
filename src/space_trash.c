#include "space_scan.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int same(const STSpaceIdentity *id, const struct stat *st)
{
    return id->valid && id->dev == st->st_dev && id->ino == st->st_ino &&
        (id->mode & S_IFMT) == (st->st_mode & S_IFMT);
}
static int component(const char *s)
{
    return s && *s && strcmp(s, ".") && strcmp(s, "..") && !strchr(s, '/');
}
static int contains(const STSpaceNode *parent, const STSpaceNode *child)
{
    size_t i;
    if (!parent || !child || !parent->children) return 0;
    for (i = 0; i < parent->child_count; ++i)
        if (parent->children[i] == child) return 1;
    return 0;
}
static char *append(char *path, const char *name)
{
    size_t a = strlen(path), b = strlen(name), slash = a && path[a-1] != '/';
    char *p;
    if (a > (size_t)-1 - slash || a + slash > (size_t)-1 - b - 1) return NULL;
    p = realloc(path, a + slash + b + 1);
    if (!p) return NULL;
    if (slash) p[a++] = '/';
    memcpy(p + a, name, b + 1);
    return p;
}
int st_space_trash_validate(const STSpaceNode *root,
                            const STSpaceNode *const *ancestors, size_t count,
                            const STSpaceNode *file)
{
    size_t i, n, index = 0;
    char *path, *next;
    struct stat st;
    int ok = 0;
    if (!root || !root->name || root->name[0] != '/' || !root->prefixes ||
        !root->prefix_count || !ancestors || !count || count > 65 ||
        ancestors[0] != root || !file || !S_ISDIR(root->identity.mode) ||
        !root->identity.valid || !S_ISREG(file->identity.mode) ||
        !file->identity.valid || file->identity.dev != root->identity.dev ||
        (file->flags & (ST_SPACE_UNREADABLE | ST_SPACE_SYMLINK | ST_SPACE_DIRECTORY))) return 0;
    n = strlen(root->name);
    if (!n || (n > 1 && root->name[n-1] == '/')) return 0;
    path = strdup(root->name); if (!path) return 0;
    if (lstat("/", &st) || index >= root->prefix_count ||
        !S_ISDIR(st.st_mode) || !same(&root->prefixes[index++], &st)) goto done;
    for (i = 1; i <= n && n > 1; ++i) if (path[i] == '/' || path[i] == '\0') {
        size_t start = i;
        char saved = path[i];
        while (start && path[start-1] != '/') --start;
        if (i == start || (i-start == 1 && path[start] == '.') ||
            (i-start == 2 && path[start] == '.' && path[start+1] == '.')) goto done;
        path[i] = '\0';
        if (lstat(path, &st) || index >= root->prefix_count || !S_ISDIR(st.st_mode) ||
            !same(&root->prefixes[index++], &st)) goto done;
        path[i] = saved;
    }
    if (index != root->prefix_count || !same(&root->identity, &st)) goto done;
    for (i = 0; i < count; ++i) {
        const STSpaceNode *node = ancestors[i];
        if (!node || !node->identity.valid || !S_ISDIR(node->identity.mode) ||
            node->identity.dev != root->identity.dev || (node->flags & ST_SPACE_UNREADABLE)) goto done;
        if (!i) continue;
        if (!contains(ancestors[i-1], node) || !component(node->name)) goto done;
        next = append(path, node->name); if (!next) goto done; path = next;
        if (lstat(path, &st) || !S_ISDIR(st.st_mode) || !same(&node->identity, &st)) goto done;
    }
    if (!contains(ancestors[count-1], file) || !component(file->name)) goto done;
    next = append(path, file->name); if (!next) goto done; path = next;
    if (lstat(path, &st) || !S_ISREG(st.st_mode) || !same(&file->identity, &st)) goto done;
    ok = 1;
done:
    free(path); return ok;
}
