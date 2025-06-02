#include "buzzer.h"
#include <stdbool.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

#define BUZZER_PIN 21
#define WRAP 12500 // frequência de 10 KHz
#define LEVEL 50 // valor escolhido para manter o volume do buzzer baixo
#define CLOCK_DIVIDER 100 // beep mais grave

static uint slice;

void buzzer_init() {
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_set_clkdiv(slice, CLOCK_DIVIDER);
    pwm_set_wrap(slice, WRAP);
    pwm_set_gpio_level(BUZZER_PIN, LEVEL);
    pwm_set_enabled(slice, false);
}

void buzzer_set(bool status) {
    pwm_set_enabled(slice, status);
}