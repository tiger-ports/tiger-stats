#include "space_sunburst.h"
#include "space_size.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* Generate a color by size using HSV hue gradient (blue->cyan->green->yellow->red) */
CGColorRef st_space_sunburst_color_by_size(unsigned long bytes, unsigned long total_bytes)
{
    float hue, saturation, brightness;
    CGColorSpaceRef rgb;
    CGColorRef color;
    float r, g, b;
    
    if (total_bytes <= 0) total_bytes = 1;
    /* Map bytes to hue: small=blue(240), large=red(0) */
    float ratio = (float)bytes / (float)total_bytes;
    hue = 240.0f - (ratio * 240.0f); /* 240 (blue) to 0 (red) */
    saturation = 0.65f + (ratio * 0.35f); /* 0.65 to 1.0 */
    brightness = 0.70f + (ratio * 0.30f); /* 0.70 to 1.0 */
    
    /* HSV to RGB */
    if (hue < 0) hue += 360;
    int hi = (int)(hue / 60.0f) % 6;
    float f = (hue / 60.0f) - hi;
    float p = brightness * (1 - saturation);
    float q = brightness * (1 - f * saturation);
    float t = brightness * (1 - (1 - f) * saturation);
    
    switch (hi) {
        case 0: r = brightness; g = t; b = p; break;
        case 1: r = q; g = brightness; b = p; break;
        case 2: r = p; g = brightness; b = t; break;
        case 3: r = p; g = q; b = brightness; break;
        case 4: r = t; g = p; b = brightness; break;
        default: r = brightness; g = p; b = q; break;
    }
    
    rgb = CGColorSpaceCreateDeviceRGB();
    CGFloat components[] = { r, g, b, 1.0f };
    color = CGColorCreate(rgb, components);
    CGColorSpaceRelease(rgb);
    return color;
}

/* Color by type (directory vs file, with extension hints) */
CGColorRef st_space_sunburst_color_by_type(const char *name)
{
    CGColorSpaceRef rgb;
    CGColorRef color;
    CGFloat components[4];
    
    if (!name) {
        /* Directories: neutral gray */
        components[0] = 0.6f; components[1] = 0.6f; components[2] = 0.6f; components[3] = 1.0f;
    } else {
        const char *ext = strrchr(name, '.');
        if (!ext) {
            /* No extension: directory-like gray */
            components[0] = 0.6f; components[1] = 0.6f; components[2] = 0.6f; components[3] = 1.0f;
        } else if (strcmp(ext, ".app") == 0 || strcmp(ext, ".framework") == 0) {
            /* Apps/frameworks: purple */
            components[0] = 0.7f; components[1] = 0.4f; components[2] = 0.9f; components[3] = 1.0f;
        } else if (strcmp(ext, ".mov") == 0 || strcmp(ext, ".mp4") == 0 || strcmp(ext, ".mkv") == 0) {
            /* Video: orange */
            components[0] = 1.0f; components[1] = 0.6f; components[2] = 0.2f; components[3] = 1.0f;
        } else if (strcmp(ext, ".mp3") == 0 || strcmp(ext, ".aac") == 0 || strcmp(ext, ".wav") == 0) {
            /* Audio: cyan */
            components[0] = 0.2f; components[1] = 0.8f; components[2] = 1.0f; components[3] = 1.0f;
        } else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".png") == 0 || strcmp(ext, ".gif") == 0) {
            /* Images: green */
            components[0] = 0.3f; components[1] = 0.9f; components[2] = 0.3f; components[3] = 1.0f;
        } else {
            /* Other files: light blue */
            components[0] = 0.4f; components[1] = 0.7f; components[2] = 1.0f; components[3] = 1.0f;
        }
    }
    
    rgb = CGColorSpaceCreateDeviceRGB();
    color = CGColorCreate(rgb, components);
    CGColorSpaceRelease(rgb);
    return color;
}

/* Format bytes as human-readable size - wrapper around st_space_format_size */
char *st_space_sunburst_format_bytes(unsigned long bytes)
{
    char *result = (char *)malloc(24);
    if (!result) return NULL;
    if (!st_space_format_size((uint64_t)bytes, result, 24)) {
        free(result);
        return NULL;
    }
    return result;
}

/* Recursive helper: generate segments for a node and its children */
static void generate_segments_recursive(const STSpaceNode *node,
                                        int depth,
                                        double parent_start,
                                        double parent_arc,
                                        const STSunburstConfig *config,
                                        STSunburstCallback callback,
                                        void *context)
{
    size_t i;
    double total_bytes = 0;
    double current_angle;
    
    if (!node || !callback) return;
    if (config->max_depth > 0 && depth > config->max_depth) return;
    if (!node->child_count) return;
    
    /* Calculate total bytes of all children */
    for (i = 0; i < node->child_count; ++i) {
        if (node->children && node->children[i]) {
            total_bytes += (double)node->children[i]->bytes;
        }
    }
    if (total_bytes <= 0) return;
    
    current_angle = parent_start;
    
    /* Generate a segment for each child */
    for (i = 0; i < node->child_count; ++i) {
        STSpaceNode *child = node->children[i];
        STSunburstSegment seg;
        double child_arc;
        char *size_str;
        
        if (!child || child->bytes <= 0) continue;
        
        /* Calculate this child's arc */
        child_arc = (child->bytes / total_bytes) * parent_arc;
        
        /* Fill segment */
        seg.node = child;
        seg.depth = depth + 1;
        seg.start_angle = current_angle;
        seg.end_angle = current_angle + child_arc;
        seg.inner_r = config->inner_radius + (depth * config->ring_width);
        seg.outer_r = seg.inner_r + config->ring_width;
        
        /* Color based on mode */
        if (config->color_mode == 1) {
            seg.color = st_space_sunburst_color_by_type(child->name);
        } else {
            seg.color = st_space_sunburst_color_by_size(child->bytes, node->bytes);
        }
        
        /* Label: "Name (Size)" */
        size_str = st_space_sunburst_format_bytes(child->bytes);
        if (size_str) {
            seg.label_len = strlen(child->name) + strlen(size_str) + 4;
            seg.label = (char *)malloc(seg.label_len);
            if (seg.label) {
                snprintf(seg.label, seg.label_len, "%s (%s)", child->name, size_str);
            }
            free(size_str);
        } else {
            seg.label = NULL;
            seg.label_len = 0;
        }
        
        /* Invoke callback */
        callback(&seg, context);
        
        /* Recurse into children */
        generate_segments_recursive(child, depth + 1, current_angle, child_arc, config, callback, context);
        
        current_angle += child_arc;
    }
}

/* Main entry point: generate all sunburst segments */
void st_space_sunburst_generate(const STSpaceNode *root,
                                 const STSunburstConfig *config,
                                 STSunburstCallback callback,
                                 void *context)
{
    if (!root || !config || !callback) return;
    
    /* Start with the full 360-degree arc at depth 0 */
    generate_segments_recursive(root, 0, 0.0, 360.0, config, callback, context);
}
