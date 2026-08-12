#include "ps2.h"
#include "../freestanding/kprintf.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__ ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__ ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void ps2_mouse_wait_write(void) {
    int timeout = 100000;
    while (timeout-- && (inb(0x64) & 2));
}

static void ps2_mouse_wait_read(void) {
    int timeout = 100000;
    while (timeout-- && !(inb(0x64) & 1));
}

static void ps2_mouse_write(uint8_t data) {
    ps2_mouse_wait_write();
    outb(0x64, 0xD4);
    ps2_mouse_wait_write();
    outb(0x60, data);
}

static uint8_t ps2_mouse_read(void) {
    ps2_mouse_wait_read();
    return inb(0x60);
}

static kgfx_mouse_t mouse_state = { .x = 400, .y = 300, .buttons = 0, .type = KGFX_CURSOR_NORMAL };
static uint8_t mouse_cycle = 0;
static int8_t mouse_packet[3];

static char input_buf[128] = "";
static size_t input_pos = 0;

static const char scancode_ascii[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

void ps2_init(void) {
    ps2_mouse_wait_write();
    outb(0x64, 0xA8);

    ps2_mouse_wait_write();
    outb(0x64, 0x20);
    ps2_mouse_wait_read();
    uint8_t status = inb(0x60) | 2;
    ps2_mouse_wait_write();
    outb(0x64, 0x60);
    ps2_mouse_wait_write();
    outb(0x60, status);

    ps2_mouse_write(0xF4);
    ps2_mouse_read();
}

void ps2_keyboard_handler(void) {
    uint8_t scancode = inb(0x60);
    if (!(scancode & 0x80)) {
        char c = scancode_ascii[scancode];
        if (c == '\b') {
            if (input_pos > 0) input_buf[--input_pos] = '\0';
        } else if (c == '\n') {
            kprintf("\n  \033[1;32m[Keyboard Input Entered]:\033[0m %s\n", input_buf);
            input_pos = 0;
            input_buf[0] = '\0';
        } else if (c && input_pos < sizeof(input_buf) - 1) {
            input_buf[input_pos++] = c;
            input_buf[input_pos] = '\0';
            kprintf("%c", c);
        }
    }
}

void ps2_mouse_handler(void) {
    uint8_t data = inb(0x60);

    switch (mouse_cycle) {
        case 0:
            if ((data & 0x08) == 0x08) {
                mouse_packet[0] = (int8_t)data;
                mouse_cycle = 1;
            }
            break;
        case 1:
            mouse_packet[1] = (int8_t)data;
            mouse_cycle = 2;
            break;
        case 2:
            mouse_packet[2] = (int8_t)data;
            mouse_cycle = 0;

            mouse_state.buttons = mouse_packet[0] & 0x07;
            int rel_x = mouse_packet[1];
            int rel_y = mouse_packet[2];

            mouse_state.x += rel_x;
            mouse_state.y -= rel_y;

            if (mouse_state.x < 0) mouse_state.x = 0;
            if (mouse_state.x >= 800) mouse_state.x = 799;
            if (mouse_state.y < 0) mouse_state.y = 0;
            if (mouse_state.y >= 600) mouse_state.y = 599;

            if (mouse_state.buttons & 1) {
                mouse_state.type = KGFX_CURSOR_CLICKABLE;
            } else {
                mouse_state.type = KGFX_CURSOR_NORMAL;
            }
            break;
    }
}

kgfx_mouse_t *ps2_get_mouse_state(void) {
    return &mouse_state;
}

const char *ps2_get_input_buffer(void) {
    return input_buf;
}
