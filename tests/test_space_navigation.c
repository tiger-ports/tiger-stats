#include "../src/space_navigation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    STSpaceNode root, dir, file, link;
    memset(&root, 0, sizeof(root));
    memset(&dir, 0, sizeof(dir));
    memset(&file, 0, sizeof(file));
    memset(&link, 0, sizeof(link));
    STSpaceNode *children[] = {&dir, &file, &link};
    STSpaceNavigation nav;
    root.flags = dir.flags = ST_SPACE_DIRECTORY;
    link.flags = ST_SPACE_SYMLINK;
    root.children = children; root.child_count = 3;
    st_space_nav_init(&nav, &root);
    assert(st_space_nav_current(&nav) == &root);
    assert(!st_space_nav_up(&nav));
    assert(!st_space_nav_enter(&nav, 1));
    assert(!st_space_nav_enter(&nav, 2));
    assert(!st_space_nav_enter(&nav, 3));
    assert(st_space_nav_enter(&nav, 0));
    assert(st_space_nav_current(&nav) == &dir);
    assert(!st_space_nav_enter(&nav, 0));
    assert(st_space_nav_up(&nav) && st_space_nav_current(&nav) == &root);
    puts("PASS disk navigation stays inside scanned tree");
    return 0;
}
