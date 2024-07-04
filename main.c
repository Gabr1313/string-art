#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef int32_t i32;
typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int64_t i64;
typedef float f32;
typedef double f64;

typedef struct _BWBuffer {
    u32 width, height;
    u8* buf;
} BWBuffer;

typedef u64 PointCanvas;

typedef struct _Segment {
    u32 size;
    PointCanvas* pp;
} Segment;

typedef struct _Rays {
    u32 nodes, width, height, size;
    Segment* seg;  // @todo: one array for every node, so I can remove quickly the ones i don't use
} Rays;

typedef struct _Point {
    f32 x, y;
} Point;

#define NODES 1000
#define LINE_COLOR 4   // <= 256. Distance from line is like
#define WIDTH_MIN 0.4  //         as follow      __
#define WIDTH_MAX 1.0  // >=1                 __/  \__
#define CHECK_M(mat, k) (mat[(k) / 64] & ((u64)1 << ((k) % 64)))
#define FLIP_M(mat, k) (mat[(k) / 64] ^= ((u64)1 << ((k) % 64)))

BWBuffer bwb_new(u32 width, u32 height);
BWBuffer bwb_new_white(u32 width, u32 height);
void bwb_free(BWBuffer bwb);
BWBuffer bwb_from_ppm_file(char* file_name);
void bwb_to_ppm_file(BWBuffer bwb, char* file_name);
void bwb_draw_line(BWBuffer bwb, Segment seg);

Rays rays_new(u32 nodes, u32 width, u32 height);
void rays_free(Rays rays);

PointCanvas pc_new(u64 x, u64 y, u64 col);
u32 pc_x(PointCanvas p);
u32 pc_y(PointCanvas p);
u8 pc_color(PointCanvas p);
int pc_cmp(const void* p1, const void* p2);

Segment seg_new(u32 size, PointCanvas* pp);
void seg_free(Segment s);

void add_neighbour(u32 x, u32 y, u32* idx, u32 width, u32 height, Point* todo, u64* check_matrix);
u32 draw_best_from_node(u32 idx, Rays ls, BWBuffer canvas, BWBuffer original, u64* check_matrix);
void* copy_malloc(void* ptr, u64 size);

FILE* y4m_init(char* file_name, u32 width, u32 height, u32 frames);
void y4m_close(FILE* video);
void y4m_frame(FILE* video, BWBuffer canvas);

i32 main(i32 argc, char* argv[]) {
    clock_t time1 = clock();
    assert(argc == 2);
    BWBuffer original = bwb_from_ppm_file(argv[1]);
    BWBuffer canvas = bwb_new_white(original.width, original.height);
    Rays ls = rays_new(NODES, original.width, original.height);
    clock_t time2 = clock();
    printf("First part  finished (%.3lfs): rays created\n", (double)(time2 - time1) / CLOCKS_PER_SEC);
    u64* check_matrix = calloc(((NODES * (NODES - 1) / 2) + 63) / 64, sizeof(u64));
    assert(check_matrix);
    FILE* video = y4m_init("output.y4m", canvas.width, canvas.height, 30);
    for (u32 idx = 0, frame = 0; idx != ls.nodes;frame++) {
        idx = draw_best_from_node(idx, ls, canvas, original, check_matrix);
        if (!(frame & 63)) y4m_frame(video, canvas);
    }
    bwb_to_ppm_file(canvas, "test.ppm");
    clock_t time3 = clock();
    printf("Second part finished (%.3lfs): image/video drawn\n", (double)(time3 - time2) / CLOCKS_PER_SEC);
    y4m_close(video);
    free(check_matrix);
    rays_free(ls);
    bwb_free(original);
    bwb_free(canvas);
    clock_t time4 = clock();
    printf("Third part  finished (%.3lfs): free\n", (double)(time4 - time3) / CLOCKS_PER_SEC);
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
        f32 x = 0.2126 * r + 0.7152 * g + 0.0722 * b;
        // bwb.buf[i] = round((atan(10 * (x / 255 - .5)) / 2.746 + .5) * 255);
        bwb.buf[i] = x;
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

Rays rays_new(u32 nodes, u32 width, u32 height) {  // @ugly
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
    assert(check_matrix);
    Point* todo = malloc(sizeof(Point) * width * height);
    assert(todo);
    u64* flipped = malloc(sizeof(Point) * width * height);
    assert(flipped);
    u32 idx = 0;
    PointCanvas* cp =
        malloc(sizeof(*cp) * radius * 2 * (size_t)((WIDTH_MAX + 1) * (WIDTH_MAX + 1)));
    assert(cp);
    for (u32 n = 0, pos = 0; n < nodes; n++) {
        for (u32 m = 0; m < n; m++, pos++) {
            f32 a = points[n].y - points[m].y;
            f32 b = points[m].x - points[n].x;
            f32 c = points[n].x * -a + points[n].y * -b;
            f32 den = sqrt(a * a + b * b);
            assert(cp);
            // bfs
            u32 cnt = 0, cnt_flipped = 0;
            u32 x = points[n].x, y = points[n].y;
            add_neighbour(x, y, &idx, width, height, todo, check_matrix);
            u32 tmp = 0;
            assert(idx);
            while (idx) {
                tmp++;
                u32 x = todo[--idx].x, y = todo[idx].y;
                u32 k = y * width + x;
                if (CHECK_M(check_matrix, k)) continue;
                FLIP_M(check_matrix, k);
                flipped[cnt_flipped++] = k;
                if ((x - x_center) * (x - x_center) + (y - y_center) * (y - y_center) >
                    radius * radius)
                    continue;
                float dist = fabs(a * x + b * y + c) / den;
                if (dist < WIDTH_MIN) cp[cnt++] = pc_new(x, y, LINE_COLOR);
                else if (dist < WIDTH_MAX) {
                    u8 col = LINE_COLOR * (WIDTH_MAX - dist) / (WIDTH_MAX - WIDTH_MIN);
                    // printf("[%u]", cnt), fflush(stdout);
                    if (col == 0) continue;
                    cp[cnt++] = pc_new(x, y, col);
                } else continue;
                assert(pc_x(cp[cnt - 1]) == x && pc_y(cp[cnt - 1]) == y);
                add_neighbour(x, y, &idx, width, height, todo, check_matrix);

                // #define PRINT(xxx) \
//     if (pos == 2775) printf("%u %lu\n", xxx, cp[6])
                //
                //                 PRINT(0);
            }
            // PRINT(1);
            // assert(cnt);
            // PRINT(2);
            seg[pos] = seg_new(cnt, copy_malloc(cp, sizeof(*cp) * cnt));
            for (u32 k = 0; k < cnt_flipped; k++) FLIP_M(check_matrix, flipped[k]);
            // PRINT(3);
            // if (pos == 2775) fflush(stdout), assert(0);
        }
    }
    free(cp);
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

void* copy_malloc(void* ptr, u64 size) {
    void* retval = malloc(size);
    assert(retval);
    memcpy(retval, ptr, size);
    return retval;
}

void add_neighbour(u32 x, u32 y, u32* idx, u32 width, u32 height, Point* todo, u64* check_matrix) {
    u32 min_x = (x > 0 ? x - 1 : 0);
    u32 max_x = (x + 1 < width ? x + 1 : width - 1);
    u32 min_y = (y > 0 ? y - 1 : 0);
    u32 max_y = (y + 1 < height ? y + 1 : height - 1);
    for (u32 xx = min_x; xx <= max_x; xx++)
        for (u32 yy = min_y; yy <= max_y; yy++)
            if (!CHECK_M(check_matrix, yy * width + xx)) todo[(*idx)++] = (Point){xx, yy};
}

void rays_free(Rays rays) {
    for (u32 i = 0; i < rays.size; i++) seg_free(rays.seg[i]);
    free(rays.seg);
}

void bwb_draw_line(BWBuffer bwb, Segment seg) {
    for (u32 i = 0; i < seg.size; i++) {
        u32 x = pc_x(seg.pp[i]);
        u32 y = pc_y(seg.pp[i]);
        u8 col = pc_color(seg.pp[i]);
        if (bwb.buf[x + y * bwb.width] >= col) bwb.buf[x + y * bwb.width] -= col;
        else bwb.buf[x + y * bwb.width] = 0;
    }
}

PointCanvas pc_new(u64 x, u64 y, u64 col) {
    assert(x < ((u64)1 << 28) && y < ((u64)1 << 28) && col < ((u64)1 << 8));
    // printf("(%lu,%lu,%lu)\n", x, y, col), fflush(stdout);
    return (col << 56) + (y << 28) + x;
}

u32 pc_x(PointCanvas p) {
    return p & 0xfffffff;
}

u32 pc_y(PointCanvas p) {
    return (p >> 28) & 0xfffffff;
}

u8 pc_color(PointCanvas p) {
    return p >> 56;
}

Segment seg_new(u32 size, PointCanvas* pp) {
    Segment s = {
        .size = size,
        .pp = pp,
    };
    return s;
}
void seg_free(Segment s) {
    free(s.pp);
}

u32 draw_best_from_node(u32 idx, Rays ls, BWBuffer canvas, BWBuffer original, u64* check_matrix) {
    f64 best = 0;
    u32 best_idx = ls.nodes;
    u32 width = canvas.width;
    for (u32 j = 0; j < ls.nodes; j++) {
        u32 min = idx < j ? idx : j;
        u32 max = idx > j ? idx : j;
        u32 k = (max) * (max - 1) / 2 + min;
        if (idx == j || CHECK_M(check_matrix, k)) continue;
        i64 score = 0;
        for (u32 i = 0; i < ls.seg[k].size; i++) {
            u32 x = pc_x(ls.seg[k].pp[i]), y = pc_y(ls.seg[k].pp[i]);
            u8 color = pc_color(ls.seg[k].pp[i]);
            score += (canvas.buf[x + y * width] - original.buf[x + y * width]) * color;
        }
        // f64 scoref = (f32)score / sqrt(ls.seg[k].size);
        f64 scoref = (f64)score / ls.seg[k].size;
        if (scoref > best) {
            best = scoref;
            best_idx = j;
        }
    }
    if (best_idx == ls.nodes) return best_idx;
    u32 min = idx < best_idx ? idx : best_idx;
    u32 max = idx > best_idx ? idx : best_idx;
    u32 k = (max) * (max - 1) / 2 + min;
    bwb_draw_line(canvas, ls.seg[k]);
    FLIP_M(check_matrix, k);
    return best_idx;
}

FILE* y4m_init(char* file_name, u32 width, u32 height, u32 frames) {
    FILE* video = fopen(file_name, "w");
    assert(video);
    fprintf(video, "YUV4MPEG2 W%u H%u F%u:1 Ip A1:1 Cmono\n", width, height, frames);
    return video;
}

void y4m_close(FILE* video) {
    fclose(video);
}

void y4m_frame(FILE* video, BWBuffer canvas) {
    fprintf(video, "FRAME\n");
    fwrite(canvas.buf, sizeof(u8), canvas.height * canvas.width, video);
}
