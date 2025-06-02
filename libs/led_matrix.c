#include "led_matrix.h"
#include "hardware/clocks.h"
#include "pio_matrix.h"
#include <stdio.h>

#define PIXELS 25
#define COLOR 6
#define LED_MATRIX_PIN 7
#define ENTRANCE_COLOR 1638400
#define ROOM_COLOR 419430400
#define KITCHEN_COLOR 6400
#define BEDROOM_COLOR 1644800
#define OFF 0
#define BRIGHTNESS_FACTOR 0.1

static const PIO PIO_ID = pio0;
static unsigned int sm;

void led_matrix_init() {
    set_sys_clock_khz(128000, false);
    unsigned int offset = pio_add_program(PIO_ID, &pio_matrix_program);
    sm = pio_claim_unused_sm(PIO_ID, true);
    pio_matrix_program_init(PIO_ID, sm, offset, LED_MATRIX_PIN);
    for (unsigned int i = 0; i < PIXELS; i++) pio_sm_put_blocking(PIO_ID, sm, OFF);
}

static unsigned int location_color(bool entrance, bool room, bool kitchen, bool bedroom, int index) {
    switch(index) {
        case 0:
        case 1:
        case 8:
        case 9: return entrance ? ENTRANCE_COLOR : OFF; break;
        case 3:
        case 4:
        case 5:
        case 6: return room ? ROOM_COLOR : OFF; break;
        case 18:
        case 19:
        case 20:
        case 21: return kitchen ? KITCHEN_COLOR : OFF; break;
        case 15:
        case 16:
        case 23:
        case 24: return bedroom ? BEDROOM_COLOR : OFF; break;
        default: return OFF;
    }
}

void led_matrix_lights(bool entrance, bool room, bool kitchen, bool bedroom) {
    for (unsigned int i = 0; i < PIXELS; i++) {
        unsigned int led_value = location_color(entrance, room, kitchen, bedroom, i);
        pio_sm_put_blocking(PIO_ID, sm, led_value);
    }
}