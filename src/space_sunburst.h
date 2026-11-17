#ifndef ST_SPACE_SUNBURST_H
#define ST_SPACE_SUNBURST_H

#include "space_scan.h"
#include <CoreGraphics/CoreGraphics.h>

/* Sunburst visualization parameters */
typedef struct {
    float inner_radius;      /* Innermost ring radius in points */
    float ring_width;        /* Width of each ring in points */
    float center_x, center_y; /* Center point */
    int max_depth;           /* Maximum depth to render (0=unlimited) */
    int color_mode;          /* 0=by_size, 1=by_type (future) */
} STSunburstConfig;

/* Segment in sunburst - represents a file/folder in hierarchy */
typedef struct {
    const STSpaceNode *node;  /* The data node */
    int depth;                /* Depth from root (0=center) */
    double start_angle;       /* Start angle in degrees [0,360) */
    double end_angle;         /* End angle in degrees */
    float inner_r;            /* Inner radius for this ring */
    float outer_r;            /* Outer radius for this ring */
    CGColorRef color;         /* Color for this segment */
    char *label;              /* Formatted label "Name (Size)" */
    size_t label_len;
} STSunburstSegment;

/* Generate sunburst segments for a node tree.
   Caller must free each segment's label with free(). */
typedef void (*STSunburstCallback)(const STSunburstSegment *seg, void *context);
void st_space_sunburst_generate(const STSpaceNode *root,
                                 const STSunburstConfig *config,
                                 STSunburstCallback callback,
                                 void *context);

/* Color selection by size (hue gradient) */
CGColorRef st_space_sunburst_color_by_size(unsigned long bytes, unsigned long total_bytes);

/* Color selection by type (file extension or directory) */
CGColorRef st_space_sunburst_color_by_type(const char *name);

/* Format size in human-readable form (allocates buffer) */
char *st_space_sunburst_format_bytes(unsigned long bytes);

#endif
