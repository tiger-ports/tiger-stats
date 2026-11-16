#include "space_navigation.h"
#include <string.h>
void st_space_nav_init(STSpaceNavigation *nav, const STSpaceNode *root)
{
    if (!nav) return;
    memset(nav, 0, sizeof(*nav)); nav->stack[0] = root;
}
const STSpaceNode *st_space_nav_current(const STSpaceNavigation *nav)
{
    return nav && nav->depth < 65 ? nav->stack[nav->depth] : NULL;
}
int st_space_nav_enter(STSpaceNavigation *nav, size_t index)
{
    const STSpaceNode *node = st_space_nav_current(nav), *child;
    if (!node || nav->depth >= 64 || index >= node->child_count || !node->children) return 0;
    child = node->children[index];
    if (!child || !(child->flags & ST_SPACE_DIRECTORY) || (child->flags & ST_SPACE_SYMLINK)) return 0;
    nav->stack[++nav->depth] = child;
    return 1;
}
int st_space_nav_up(STSpaceNavigation *nav)
{
    if (!nav || !nav->depth || nav->depth >= 65) return 0;
    nav->stack[nav->depth--] = NULL;
    return 1;
}
