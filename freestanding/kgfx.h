#ifndef KGFX_H
#define KGFX_H

#include <stddef.h>
#include <stdint.h>

/* Framebuffer descriptor */
typedef struct {
    uint32_t *buffer;   /* Pixel memory array (ARGB32) */
    uint32_t width;     /* Screen width in pixels */
    uint32_t height;    /* Screen height in pixels */
    uint32_t pitch;     /* Pixels per scanline */
} kgfx_fb_t;

/* Standard 32-bit ARGB color definitions */
#define KGFX_BLACK     0xFF000000
#define KGFX_WHITE     0xFFFFFFFF
#define KGFX_RED       0xFFFF0000
#define KGFX_GREEN     0xFF00FF00
#define KGFX_BLUE      0xFF0000FF
#define KGFX_YELLOW    0xFFFFFF00
#define KGFX_CYAN      0xFF00FFFF
#define KGFX_MAGENTA   0xFFFF00FF
#define KGFX_DARKGRAY  0xFF313244
#define KGFX_NAVY      0xFF181825
#define KGFX_PURPLE    0xFFCBA6F7

/* Macro para criação de cores ARGB */
static inline uint32_t kgfx_argb(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/* Bounding box rectangle */
typedef struct {
    int x, y, w, h;
} kgfx_rect_t;

/* Double buffering descriptor */
typedef struct {
    uint32_t *front_buffer; /* Hardware video memory */
    uint32_t *back_buffer;  /* RAM render target */
    uint32_t width;         /* Screen width */
    uint32_t height;        /* Screen height */
} kgfx_double_buffer_t;

/* Dirty rectangle tracker */
#define KGFX_MAX_DIRTY_RECTS 16

typedef struct {
    kgfx_rect_t rects[KGFX_MAX_DIRTY_RECTS];
    size_t count;
} kgfx_dirty_list_t;

/* Cursor appearance type */
typedef enum {
    KGFX_CURSOR_NORMAL = 0,    /* Standard arrow pointer */
    KGFX_CURSOR_CLICKABLE,     /* Hand/pointer for interactive UI */
    KGFX_CURSOR_TEXT           /* Beam cursor for text input */
} kgfx_cursor_type_t;

/* Mouse input state */
typedef struct {
    int x, y;                  /* Screen coordinates */
    uint8_t buttons;           /* Bit 0: Left, Bit 1: Right, Bit 2: Middle */
    kgfx_cursor_type_t type;   /* Active cursor appearance */
} kgfx_mouse_t;

/* Framebuffer initialization & clearing */
void kgfx_init(kgfx_fb_t *fb, uint32_t *buffer, uint32_t width, uint32_t height);
void kgfx_clear(kgfx_fb_t *fb, uint32_t color);

/* Alpha Blending */
uint32_t kgfx_blend_colors(uint32_t bg, uint32_t fg);
void kgfx_draw_pixel_alpha(kgfx_fb_t *fb, int x, int y, uint32_t color);
void kgfx_draw_rect_alpha(kgfx_fb_t *fb, int x, int y, int w, int h, uint32_t color);

/* Graphics primitives */
void kgfx_draw_pixel(kgfx_fb_t *fb, int x, int y, uint32_t color);
void kgfx_draw_line(kgfx_fb_t *fb, int x0, int y0, int x1, int y1, uint32_t color);
void kgfx_draw_rect(kgfx_fb_t *fb, int x, int y, int w, int h, uint32_t color, int fill);
void kgfx_draw_rounded_rect(kgfx_fb_t *fb, int x, int y, int w, int h, int r, uint32_t color, int fill);
void kgfx_draw_circle(kgfx_fb_t *fb, int cx, int cy, int r, uint32_t color);
void kgfx_draw_filled_circle(kgfx_fb_t *fb, int cx, int cy, int r, uint32_t color);
void kgfx_draw_triangle(kgfx_fb_t *fb, int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color, int fill);

/* Text rendering with scaling */
void kgfx_draw_char(kgfx_fb_t *fb, int x, int y, char c, uint32_t fg, uint32_t bg);
void kgfx_draw_string(kgfx_fb_t *fb, int x, int y, const char *str, uint32_t fg, uint32_t bg);
void kgfx_draw_char_scaled(kgfx_fb_t *fb, int x, int y, char c, uint32_t fg, uint32_t bg, int scale);
void kgfx_draw_string_scaled(kgfx_fb_t *fb, int x, int y, const char *str, uint32_t fg, uint32_t bg, int scale);

/* Bitmap / Sprite Blitting */
void kgfx_blit(kgfx_fb_t *dest, int dx, int dy, const uint32_t *src_buf, int sw, int sh, uint32_t chroma_key);

/* Double buffering API */
void kgfx_double_buffer_init(kgfx_double_buffer_t *db, uint32_t *front, uint32_t *back, uint32_t w, uint32_t h);
void kgfx_swap_buffers(kgfx_double_buffer_t *db);
void kgfx_dirty_clear(kgfx_dirty_list_t *list);
void kgfx_add_dirty_rect(kgfx_dirty_list_t *list, int x, int y, int w, int h);
void kgfx_flush_dirty(kgfx_double_buffer_t *db, kgfx_dirty_list_t *list);

/* Mouse cursor & hit-test API */
void kgfx_draw_cursor(kgfx_fb_t *fb, const kgfx_mouse_t *mouse);
int kgfx_rect_contains(const kgfx_rect_t *rect, int x, int y);

#endif
