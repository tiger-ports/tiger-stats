#ifndef ST_SPACE_NAVIGATION_H
#define ST_SPACE_NAVIGATION_H
#include "space_scan.h"
typedef struct { const STSpaceNode *stack[65]; size_t depth; } STSpaceNavigation;
void st_space_nav_init(STSpaceNavigation *, const STSpaceNode *);
const STSpaceNode *st_space_nav_current(const STSpaceNavigation *);
int st_space_nav_enter(STSpaceNavigation *, size_t);
int st_space_nav_up(STSpaceNavigation *);
#endif
