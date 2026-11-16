#include "../src/space_scan.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* Trash validation never invokes NSWorkspace; only these fixtures are removed. */
static void path(char *out, size_t cap, const char *dir, const char *name)
{
    int n = snprintf(out, cap, "%s/%s", dir, name);
    assert(n > 0 && (size_t)n < cap);
}
static void touch(const char *p)
{
    FILE *f = fopen(p, "w"); assert(f); assert(!fclose(f));
}
static STSpaceNode *child(STSpaceNode *node, const char *name)
{
    size_t i;
    for (i = 0; i < node->child_count; ++i)
        if (!strcmp(node->children[i]->name, name)) return node->children[i];
    return NULL;
}
int main(void)
{
    char dir[2048], parent[2048], moved[2048], file[2048], old[2048];
    char hard[2048], linkpath[2048], rootmoved[2048];
    const char *tmp = getenv("TMPDIR");
    STSpaceNode *root, *folder, *leaf, *hardnode, *linknode;
    const STSpaceNode *ancestors[2];
    STSpaceIdentity saved;
    char *savedname;
    assert(tmp); path(dir, sizeof(dir), tmp, "trash-test-XXXXXX"); assert(mkdtemp(dir));
    path(parent, sizeof(parent), dir, "parent"); assert(!mkdir(parent, 0700));
    path(moved, sizeof(moved), dir, "moved");
    path(file, sizeof(file), parent, "file"); touch(file);
    path(old, sizeof(old), parent, "old");
    path(hard, sizeof(hard), dir, "hard"); assert(!link(file, hard));
    path(linkpath, sizeof(linkpath), dir, "symlink"); assert(!symlink("parent/file", linkpath));
    assert(!st_space_scan(dir, NULL, &root));
    folder = child(root, "parent"); hardnode = child(root, "hard"); linknode = child(root, "symlink");
    assert(folder && hardnode && linknode); leaf = child(folder, "file"); assert(leaf);
    ancestors[0] = root; ancestors[1] = folder;
    assert(st_space_trash_validate(root, ancestors, 2, leaf));
    assert(st_space_trash_validate(root, ancestors, 1, hardnode));
    assert(!st_space_trash_validate(root, ancestors, 1, folder));
    assert(!st_space_trash_validate(root, ancestors, 1, linknode));
    assert(!st_space_trash_validate(root, ancestors, 1, leaf)); /* wrong membership */
    {
        size_t count = root->prefix_count;
        root->prefix_count = 0;
        assert(!st_space_trash_validate(root, ancestors, 2, leaf));
        root->prefix_count = count;
    }
    saved = leaf->identity; leaf->identity.valid = 0;
    assert(!st_space_trash_validate(root, ancestors, 2, leaf)); leaf->identity = saved;
    leaf->identity.dev++; assert(!st_space_trash_validate(root, ancestors, 2, leaf)); leaf->identity = saved;
    saved = folder->identity; folder->identity.valid = 0;
    assert(!st_space_trash_validate(root, ancestors, 2, leaf)); folder->identity = saved;
    folder->identity.dev++;
    assert(!st_space_trash_validate(root, ancestors, 2, leaf)); folder->identity = saved;
    savedname = leaf->name; leaf->name = "..";
    assert(!st_space_trash_validate(root, ancestors, 2, leaf)); leaf->name = "../file";
    assert(!st_space_trash_validate(root, ancestors, 2, leaf)); leaf->name = savedname;
    assert(!rename(file, old)); touch(file); /* original inode remains allocated */
    assert(!st_space_trash_validate(root, ancestors, 2, leaf));
    assert(!unlink(file)); assert(!st_space_trash_validate(root, ancestors, 2, leaf));
    assert(!symlink("old", file)); assert(!st_space_trash_validate(root, ancestors, 2, leaf));
    assert(!unlink(file));
    assert(!rename(old, file)); assert(st_space_trash_validate(root, ancestors, 2, leaf));
    assert(!rename(parent, moved)); assert(!mkdir(parent, 0700));
    assert(!st_space_trash_validate(root, ancestors, 2, leaf));
    assert(!rmdir(parent)); assert(!symlink("moved", parent));
    assert(!st_space_trash_validate(root, ancestors, 2, leaf));
    assert(!unlink(parent)); assert(!rename(moved, parent));
    assert(st_space_trash_validate(root, ancestors, 2, leaf));
    assert(snprintf(rootmoved, sizeof(rootmoved), "%s-moved", dir) > 0);
    assert(!rename(dir, rootmoved)); assert(!mkdir(dir, 0700));
    assert(!st_space_trash_validate(root, ancestors, 2, leaf));
    assert(!rmdir(dir)); assert(!rename(rootmoved, dir));
    st_space_free(root);
    assert(!unlink(linkpath)); assert(!unlink(hard)); assert(!unlink(file));
    assert(!rmdir(parent)); assert(!rmdir(dir));
    puts("PASS trash identities, links, traversal and replacement"); return 0;
}
