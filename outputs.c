#include "outputs.h"
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "lib/sh1106_i2c.h"
#include "lib/font_inconsolata.h"

/* ---------- Parámetros ajustables (según montaje) ---------- */
#define OLED_I2C          i2c0
#define OLED_SDA_PIN      4
#define OLED_SCL_PIN      5
#define OLED_BAUDRATE     400000

#define OLED_ADDR         0x3C
#define OLED_W            128
#define OLED_H            64

#define BUZZER_PIN        15
/* ---------------------------------------------------------- */

#define OLED_COLOR_ON     1

static sh1106_t oled;

/* Cache del último valor pintado */
static int cached_seconds = -1;

/* Control de refresco (evitar redibujar todo el rato) */
static bool force_redraw = true;
static absolute_time_t next_refresh;

/* Buzzer sin bloquear (versión 1: depende de outputs_update) */
static bool buzzer_active = false;
static absolute_time_t buzzer_end;

static inline void buzzer_set(bool on) {
    gpio_put(BUZZER_PIN, on ? 1 : 0);
}

static void oled_init_hw(void) {
    i2c_init(OLED_I2C, OLED_BAUDRATE);

    gpio_set_function(OLED_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL_PIN, GPIO_FUNC_I2C);

    gpio_pull_up(OLED_SDA_PIN);
    gpio_pull_up(OLED_SCL_PIN);

    SH1106_init(&oled, OLED_I2C, OLED_ADDR, OLED_W, OLED_H);
    SH1106_clear(&oled);
    SH1106_draw(&oled);
}

static void draw_time_mmss(int seconds) {
    if (seconds < 0) seconds = 0;

    int m = seconds / 60;
    int s = seconds % 60;
    if (m > 99) m = 99;

    char buf[6];
    buf[0] = '0' + (m / 10);
    buf[1] = '0' + (m % 10);
    buf[2] = ':';
    buf[3] = '0' + (s / 10);
    buf[4] = '0' + (s % 10);
    buf[5] = '\0';

    SH1106_clear(&oled);
    SH1106_drawString(&oled, (char*)buf, 0, 0, OLED_COLOR_ON, font_inconsolata);
    SH1106_draw(&oled);
}

void outputs_init(void) {
    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);
    buzzer_set(false);

    oled_init_hw();

    cached_seconds = -1;
    force_redraw = true;
    next_refresh = make_timeout_time_ms(0);
}

void outputs_update(timer t) {
    /* auto-apagado del buzzer (requiere que se llame outputs_update) */
    if (buzzer_active && absolute_time_diff_us(get_absolute_time(), buzzer_end) <= 0) {
        action_buzzer_off();
    }

    /* refresco ~20 Hz si no hay cambios */
    if (!force_redraw && absolute_time_diff_us(get_absolute_time(), next_refresh) > 0) {
        return;
    }
    next_refresh = make_timeout_time_ms(50);

    /* si cambió el tiempo, redibujo */
    if (t.segundos != cached_seconds) {
        cached_seconds = t.segundos;
        force_redraw = true;
    }

    if (!force_redraw) return;
    force_redraw = false;

    draw_time_mmss(cached_seconds);
}

void outputs_off(void) {
    cached_seconds = -1;
    force_redraw = false;

    SH1106_clear(&oled);
    SH1106_draw(&oled);
}

void action_show_zero(void) {
    cached_seconds = 0;
    force_redraw = true;
    draw_time_mmss(0);
}

void action_buzzer_on(void) {
    buzzer_active = true;
    buzzer_set(true);
    buzzer_end = make_timeout_time_ms(800);
}

void action_buzzer_off(void) {
    buzzer_active = false;
    buzzer_set(false);
}



