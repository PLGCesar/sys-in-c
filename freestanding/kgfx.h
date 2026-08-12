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
#define KGFX_BLACK   0xFF000000
#define KGFX_WHITE   0xFFFFFFFF
#define KGFX_RED     0xFFFF0000
#define KGFX_GREEN   0xFF00FF00
#define KGFX_BLUE    0xFF0000FF
#define KGFX_YELLOW  0xFFFFFF00
#define KGFX_CYAN    0xFF00FFFF
#define KGFX_MAGENTA 0xFFFF00FF
#define KGFX_DARKGRAY 0xFF313244

/* Framebuffer initialization */
void kgfx_init(kgfx_fb_t *fb, uint32_t *buffer, uint32_t width, uint32_t height);

/* Screen clearing */
void kgfx_clear(kgfx_fb_t *fb, uint32_t color);

/* Pixel primitive */
void kgfx_draw_pixel(kgfx_fb_t *fb, int x, int y, uint32_t color);

/* Line primitive (Bresenham) */
void kgfx_draw_line(kgfx_fb_t *fb, int x0, int y0, int x1, int y1, uint32_t color);

/* Rectangle primitive */
void kgfx_draw_rect(kgfx_fb_t *fb, int x, int y, int w, int h, uint32_t color, int fill);

/* Circle primitive (Bresenham) */
void kgfx_draw_circle(kgfx_fb_t *fb, int cx, int cy, int r, uint32_t color);

/* Character rendering (8x8 ASCII font) */
void kgfx_draw_char(kgfx_fb_t *fb, int x, int y, char c, uint32_t fg, uint32_t bg);

/* String text rendering */
void kgfx_draw_string(kgfx_fb_t *fb, int x, int y, const char *str, uint32_t fg, uint32_t bg);

/* --- VGA Mode 03h Driver (Text Mode 80x25 at 0xB8000) --- */
/*
void kgfx_vga3h_putc(int col, int row, char c, uint8_t fg, uint8_t bg);
void kgfx_vga3h_print(int col, int row, const char *str, uint8_t fg, uint8_t bg);
void kgfx_vga3h_clear(uint8_t bg);
*/

/* --- UART Serial Port Driver (COM1 0x3F8) --- */
/*
void kgfx_serial_init(void);
void kgfx_serial_putc(char c);
void kgfx_serial_print(const char *str);
*/

#endif
