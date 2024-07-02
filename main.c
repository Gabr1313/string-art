#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NODES 128
#define LINE_COLOR 32  // 256 max. Distance from line is like
#define WIDTH_MAX 0.5  //    __                     as follow
#define WIDTH_MIN 1.0  // __/  \__

typedef int32_t i32;
typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;

typedef struct _BWBuffer {
    u32 width, height;
    u8* buf;
} BWBuffer;

typedef u64 CanvasPoint;

typedef struct _Segment {
    u32 size;
    CanvasPoint* pp;
} Segment;

typedef struct _Rays {
    u32 nodes, width, height, size;
    Segment* seg;  // @todo: one array for every node, so I can remove quickly the ones i don't use
} Rays;

typedef struct _Point {
    f32 x, y;
} Point;

// @todo find better names
BWBuffer bwb_new(u32 width, u32 height);
BWBuffer bwb_new_white(u32 width, u32 height);
void bwb_free(BWBuffer bwb);
BWBuffer bwb_from_ppm_file(char* file_name);
Rays rays_new(u32 nodes, u32 width, u32 height);
void rays_free(Rays rays);
void bwb_to_ppm_file(BWBuffer bwb, char* file_name);
void bwb_draw_line(BWBuffer bwb, Segment seg);
CanvasPoint cp_new(u32 x, u32 y, u8 col);
u32 cp_x(CanvasPoint p);
u32 cp_y(CanvasPoint p);
u8 cp_color(CanvasPoint p);
Segment seg_new(u32 size, CanvasPoint* pp);
void seg_free(Segment s);
void draw_best_from_node(u32 idx, Rays ls, BWBuffer canvas, BWBuffer original);
u32 dfs_line(u32 x, u32 y, u64* check_matrix, CanvasPoint* cp);

i32 main(i32 argc, char* argv[]) {
    assert(argc == 2);
    BWBuffer original = bwb_from_ppm_file(argv[1]);
    BWBuffer canvas = bwb_new_white(original.width, original.height);
    Rays ls = rays_new(NODES, original.width, original.height);
    for (int i = 0; i < ls.size; i++) {
        bwb_draw_line(canvas, ls.seg[i]);
    }
    // draw_best_from_node(0, ls, canvas, original);
    rays_free(ls);
    bwb_to_ppm_file(canvas, "test.ppm");
    bwb_free(original);
    bwb_free(canvas);
    return 0;
}

BWBuffer bwb_new(u32 width, u32 height) {
    BWBuffer bwb = {
        .width = width,
        .height = height,
        .buf = malloc(sizeof(*bwb.buf) * width * height),
    };
    assert(bwb.buf);
    return bwb;
}

BWBuffer bwb_new_white(u32 width, u32 height) {
    BWBuffer bwb = bwb_new(width, height);
    memset(bwb.buf, 0xff, width * height);
    return bwb;
}

void bwb_free(BWBuffer bwb) {
    free(bwb.buf);
}

BWBuffer bwb_from_ppm_file(char* file_name) {
    FILE* image = fopen(file_name, "r");
    char format[3], cr;
    u32 max_col_val, width, height;
    fscanf(image, "%2s %u %u %u", format, &width, &height, &max_col_val);
    cr = getc(image);
    assert(!strncmp(format, "P6", 2));
    assert(max_col_val == 255);
    assert(cr == '\n');
    BWBuffer bwb = bwb_new(width, height);
    for (u32 i = 0; i < width * height; i++) {
        u8 r = getc(image);
        u8 g = getc(image);
        u8 b = getc(image);
        bwb.buf[i] = 0.2126 * r + 0.7152 * g + 0.0722 * b;
        bwb.buf[i] = 0.2126 * r + 0.7152 * g + 0.0722 * b;
        // bwb.buf[i] = 0.3333 * r + 0.3333 * g + 0.3333 * b;
    }
    fclose(image);
    return bwb;
}

void bwb_to_ppm_file(BWBuffer bwb, char* file_name) {
    FILE* image = fopen(file_name, "w");
    fprintf(image, "P6\n%u %u\n255\n", bwb.width, bwb.height);
    // u32 x_center = bwb.width / 2;
    // u32 y_center = bwb.height / 2;
    // u32 radius = x_center < y_center ? x_center : y_center;
    for (u32 y = 0, k = 0; y < bwb.height; y++) {
        for (u32 x = 0; x < bwb.width; x++, k++) {
            // if ((x - x_center) * (x - x_center) + (y - y_center) * (y - y_center) < radius *
            // radius)
            fprintf(image, "%c%c%c", bwb.buf[k], bwb.buf[k], bwb.buf[k]);
            // else fprintf(image, "%c%c%c", 0, 0, 0);
        }
    }
    fclose(image);
}

Rays rays_new(u32 nodes, u32 width, u32 height) { // @ugly
    u32 x_center = width / 2;
    u32 y_center = height / 2;
    u32 radius = x_center < y_center ? x_center : y_center;
    u32 size = nodes * (nodes - 1) / 2;
    Point* points = malloc(sizeof(*points) * nodes);
    assert(points);
    for (u32 n = 0; n < nodes; n++) {
        f32 theta = ((f32)n / nodes) * (2 * M_PI);
        points[n].x = radius * cos(theta) + x_center;
        points[n].y = radius * sin(theta) + y_center;
    }
    Segment* seg = malloc(sizeof(*seg) * size);
    assert(seg);
    u64* check_matrix = calloc(((height * width) + 63) / 64, sizeof(u64));
    Point* todo = malloc(sizeof(Point) * width * height);
    u64* flipped = malloc(sizeof(Point) * width * height);
#define CHECK_M(k) (check_matrix[(k) / 64] & ((u64)1 << ((k) % 64)))
#define FLIP_M(k) (check_matrix[(k) / 64] ^= ((u64)1 << ((k) % 64)))
    u32 idx = 0;
    for (u32 n = 0, pos = 0; n < nodes; n++) {
        for (u32 m = 0; m < n; m++, pos++) {
            f32 a = points[n].y - points[m].y;
            f32 b = points[m].x - points[n].x;
            f32 c = points[n].x * -a + points[n].y * -b;
            f32 den = sqrt(a * a + b * b);
            CanvasPoint* cp = malloc(sizeof(*cp) * width * height);
            // bfs
            u32 cnt = 0, cnt_flipped = 0;
            u32 x = points[n].x, y = points[n].y;
            u32 k = y * width + x; // @todo macro for neighbour?
            for (u32 xx = (x > 0 ? x - 1 : x); xx <= (x + 1 < width ? x + 1 : x); xx++)
                for (u32 yy = (y > 0 ? y - 1 : y); yy <= (y + 1 < height ? y + 1 : y); yy++)
                    todo[idx++] = (Point){xx, yy};
            u32 tmp = 0;
            while (idx) {
                tmp++;
                u32 x = todo[--idx].x, y = todo[idx].y;
                u32 k = y * width + x;
                if (CHECK_M(k)) continue;
                FLIP_M(k);
                flipped[cnt_flipped++] = k;
                if ((x - x_center) * (x - x_center) + (y - y_center) * (y - y_center) >
                    radius * radius)
                    continue;
                float dist = fabs(a * x + b * y + c) / den;
                if (dist < WIDTH_MAX) cp[cnt++] = cp_new(x, y, LINE_COLOR);
                else if (dist < WIDTH_MIN) {
                    u8 col = LINE_COLOR * (WIDTH_MIN - dist) / (WIDTH_MIN - WIDTH_MAX);
                    cp[cnt++] = cp_new(x, y, col);
                } else continue;
                for (u32 xx = (x > 0 ? x - 1 : x); xx <= (x + 1 < width ? x + 1 : x); xx++)
                    for (u32 yy = (y > 0 ? y - 1 : y); yy <= (y + 1 < height ? y + 1 : y); yy++)
                        if (!CHECK_M(y * width + height)) todo[idx++] = (Point){xx, yy};
            }
            assert(cnt);
            seg[pos] = seg_new(cnt, realloc(cp, sizeof(*cp) * cnt));
            for (u32 k = 0; k < cnt_flipped; k++) FLIP_M(flipped[k]);
        }
    }
    free(points);
    free(check_matrix);
    free(todo);
    free(flipped);
    Rays l = {
        .nodes = nodes,
        .width = width,
        .height = height,
        .size = size,
        .seg = seg,
    };
    return l;
}

void rays_free(Rays rays) {
    for (u32 i = 0; i < rays.size; i++) seg_free(rays.seg[i]);
    free(rays.seg);
}

void bwb_draw_line(BWBuffer bwb, Segment seg) {
    for (u32 i = 0; i < seg.size; i++) {
        u32 x = cp_x(seg.pp[i]);
        u32 y = cp_y(seg.pp[i]);
        u8 col = cp_color(seg.pp[i]);
        if (bwb.buf[x + y * bwb.width] >= col) bwb.buf[x + y * bwb.width] -= col;
        else bwb.buf[x + y * bwb.width] = 0;
    }
}

CanvasPoint cp_new(u32 x, u32 y, u8 col) {
    assert(x < (1 << 28) && y < (1 << 28));
    return ((u64)col << 56) + ((u64)y << 28) + (u64)x;
}

u32 cp_x(CanvasPoint p) {
    return p & 0xfffffff;
}

u32 cp_y(CanvasPoint p) {
    return (p >> 28) & 0xfffffff;
}

u8 cp_color(CanvasPoint p) {
    return p >> 56;
}

Segment seg_new(u32 size, CanvasPoint* pp) {
    Segment s = {
        .size = size,
        .pp = pp,
    };
    return s;
}
void seg_free(Segment s) {
    free(s.pp);
}

void draw_best_from_node(u32 idx, Rays ls, BWBuffer canvas, BWBuffer original) {
    u32 best = 0;
    u32 best_idx = ls.nodes;
    for (u32 j = 0; j < ls.nodes; j++) {
        if (idx == j) continue;
        u32 min = idx < j ? idx : j;
        u32 max = idx > j ? idx : j;
        u32 k = (max) * (max - 1) / 2 + min;
        bwb_draw_line(canvas, ls.seg[k]);
    }
}
