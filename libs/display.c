#include "display.h"
#include <stdio.h>
#include <stdlib.h>
#include "font.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"

#define DISPLAY_ADDRESS 0x3C
#define WIDTH 128
#define HEIGHT 64
#define I2C_SDA 14
#define I2C_SCL 15
#define I2C_PORT i2c1
#define BOUNDRATE 400000
#define REFRESH 500

typedef enum {
    SET_CONTRAST = 0x81,
    SET_ENTIRE_ON = 0xA4,
    SET_NORM_INV = 0xA6,
    SET_DISP = 0xAE,
    SET_MEM_ADDR = 0x20,
    SET_COL_ADDR = 0x21,
    SET_PAGE_ADDR = 0x22,
    SET_DISP_START_LINE = 0x40,
    SET_SEG_REMAP = 0xA0,
    SET_MUX_RATIO = 0xA8,
    SET_COM_OUT_DIR = 0xC0,
    SET_DISP_OFFSET = 0xD3,
    SET_COM_PIN_CFG = 0xDA,
    SET_DISP_CLK_DIV = 0xD5,
    SET_PRECHARGE = 0xD9,
    SET_VCOM_DESEL = 0xDB,
    SET_CHARGE_PUMP = 0x8D
} command_t;

typedef struct {
    uint8_t width, height, pages, address;
    i2c_inst_t* i2c_port;
    uint8_t* ram_buffer;
    size_t bufsize;
    uint8_t port_buffer[2];
} ssd1306_t;

static ssd1306_t _ssd;
static struct repeating_timer _timer;
static char* _message;

static void init_port() {
    i2c_init(I2C_PORT, BOUNDRATE);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
}

static void init_display() {
    _ssd.width = WIDTH;
    _ssd.height = HEIGHT;
    _ssd.pages = HEIGHT / 8U;
    _ssd.address = DISPLAY_ADDRESS;
    _ssd.i2c_port = I2C_PORT;
    _ssd.bufsize = _ssd.pages * _ssd.width + 1;
    _ssd.ram_buffer = calloc(_ssd.bufsize, sizeof(uint8_t));
    _ssd.ram_buffer[0] = 0x40;
    _ssd.port_buffer[0] = 0x80;
}

static void command(uint8_t command) {
  _ssd.port_buffer[1] = command;
  i2c_write_blocking(_ssd.i2c_port, _ssd.address, _ssd.port_buffer, 2, false);
}

static void config() {
    command(SET_DISP | 0x00);
    command(SET_MEM_ADDR);
    command(0x01);
    command(SET_DISP_START_LINE | 0x00);
    command(SET_SEG_REMAP | 0x01);
    command(SET_MUX_RATIO);
    command(HEIGHT - 1);
    command(SET_COM_OUT_DIR | 0x08);
    command(SET_DISP_OFFSET);
    command(0x00);
    command(SET_COM_PIN_CFG);
    command(0x12);
    command(SET_DISP_CLK_DIV);
    command(0x80);
    command(SET_PRECHARGE);
    command(0xF1);
    command(SET_VCOM_DESEL);
    command(0x30);
    command(SET_CONTRAST);
    command(0xFF);
    command(SET_ENTIRE_ON);
    command(SET_NORM_INV);
    command(SET_CHARGE_PUMP);
    command(0x14);
    command(SET_DISP | 0x01);
}

static void send_data() {
  command(SET_COL_ADDR);
  command(0);
  command(_ssd.width - 1);
  command(SET_PAGE_ADDR);
  command(0);
  command(_ssd.pages - 1);
  i2c_write_blocking( _ssd.i2c_port, _ssd.address, _ssd.ram_buffer, _ssd.bufsize, false);
}

static void pixel( uint8_t x, uint8_t y, bool value) {
    uint16_t index = (y >> 3) + (x << 3) + 1;
    uint8_t pixel = (y & 0b111);
    if (value) _ssd.ram_buffer[index] |= (1 << pixel);
    else _ssd.ram_buffer[index] &= ~(1 << pixel);
}

static void fill(bool value) {
    for (uint8_t y = 0; y < _ssd.height; ++y) {
        for (uint8_t x = 0; x < _ssd.width; ++x) pixel(x, y, value);
    }
}

static void rect(uint8_t top, uint8_t left, uint8_t width, uint8_t height, bool value, bool fill) {
    for (uint8_t x = left; x < left + width; ++x) {
        pixel(x, top, value);
        pixel(x, top + height - 1, value);
    }
    for (uint8_t y = top; y < top + height; ++y) {
        pixel(left, y, value);
        pixel(left + width - 1, y, value);
    }
    if (!fill)  return;
    for (uint8_t x = left + 1; x < left + width - 1; ++x) {
        for (uint8_t y = top + 1; y < top + height - 1; ++y) pixel(x, y, value);
    }
}

static void line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool value) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    while (true) {
        pixel(x0, y0, value);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void hline(uint8_t x0, uint8_t x1, uint8_t y, bool value) {
    for (uint8_t x = x0; x <= x1; ++x) pixel(x, y, value);
}

static void vline(uint8_t x, uint8_t y0, uint8_t y1, bool value) {
    for (uint8_t y = y0; y <= y1; ++y) pixel(x, y, value);
}

static void draw_char(char c, uint8_t x, uint8_t y) {
    uint16_t index = 0;
    if (c >= ' ' && c <= '~') index = (c - ' ') * 8;
    else index = 0;
    for (uint8_t i = 0; i < 8; ++i) {
        uint8_t line = font[index + i];
        for (uint8_t j = 0; j < 8; ++j) pixel(x + i, y + j, line & (1 << j));
    }
}

static void draw_string(const char* str, uint8_t x, uint8_t y) {
  while (*str) {
    draw_char(*str++, x, y);
    x += 8;
    if (x + 8 >= _ssd.width) {
      x = 0;
      y += 8;
    }
    if (y + 8 >= _ssd.height) break;
  }
}

static bool display_update() {
    bool color = true;
    fill(!color);
    rect(3, 3, 122, 60, color, !color);
    line(3, 25, 123, 25, color);
    draw_string("CEPEDI   TIC37", 8, 6);
    draw_string("EMBARCATECH", 20, 16);
    draw_string(_message, 10, 28);
    send_data();
    return true;
}

void display_init() {
    init_port();
    init_display();
    config();
    send_data();
    add_repeating_timer_ms(REFRESH, display_update, NULL, &_timer);
}

void display_set_message(char* message) {
    _message = message;
}