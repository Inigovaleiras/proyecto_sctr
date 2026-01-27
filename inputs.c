
#include "inputs.h"
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"

/*
    PARTE 2 — ENTRADAS

    - Lee botones y puerta (micro-switch) por GPIO.
    - Debounce.
    - Botones (+30/-30/start): PULSO (true solo 1 iteración al pulsar).
    - Puerta: NIVEL (puerta_abierta / puerta_cerrada reflejan el estado actual estable).
*/

/* ========= PINES DE ENTRADA (propuestos) =========
   OLED I2C: GPIO 4(SDA), 5(SCL)
   BUZZER:   GPIO 15
   => NO usamos 4,5,15 aquí.
*/
#define PIN_BTN_PLUS30      10
#define PIN_BTN_MINUS30     11
#define PIN_BTN_START       12

/* Puerta como micro-switch */
#define PIN_DOOR_SWITCH     13

/* Botones a GND + pull-up interno: pulsado = 0 */
#define BTN_ACTIVE_LOW      1

/* Puerta a GND + pull-up:
   - Puerta CERRADA  => switch pulsado => GPIO = 0
   - Puerta ABIERTA  => switch suelto  => GPIO = 1
   Por tanto: "activo" = puerta cerrada => active_low = 1
*/
#define DOOR_ACTIVE_LOW     1

/* Antirrebote en ms */
#define DEBOUNCE_MS         30

static uint32_t now_ms(void)
{
    return to_ms_since_boot(get_absolute_time());
}

static bool read_active(uint pin, bool active_low)
{
    bool level = gpio_get(pin);
    return active_low ? !level : level;
}

typedef struct {
    uint pin;
    bool active_low;

    bool stable;        // estado estable actual (ya debounced)
    bool last_stable;   // estado estable anterior
    bool last_raw;      // última lectura raw
    uint32_t last_change;
} debounced_input;

/* Actualiza el debouncer. Devuelve true si cambió el estado estable. */
static bool debounce_update(debounced_input *d, bool raw_active)
{
    uint32_t t = now_ms();

    if (raw_active != d->last_raw) {
        d->last_raw = raw_active;
        d->last_change = t;
    }

    if ((t - d->last_change) >= DEBOUNCE_MS) {
        if (d->stable != d->last_raw) {
            d->last_stable = d->stable;
            d->stable = d->last_raw;
            return true;
        }
    }
    return false;
}

static bool rising_edge (const debounced_input *d) { return (!d->last_stable && d->stable); }
static bool falling_edge(const debounced_input *d) { return ( d->last_stable && !d->stable); }

/* Botones: activo cuando está pulsado */
static debounced_input btn_plus  = { .pin = PIN_BTN_PLUS30,  .active_low = BTN_ACTIVE_LOW };
static debounced_input btn_minus = { .pin = PIN_BTN_MINUS30, .active_low = BTN_ACTIVE_LOW };
static debounced_input btn_start = { .pin = PIN_BTN_START,   .active_low = BTN_ACTIVE_LOW };

/* Puerta: activo cuando está CERRADA */
static debounced_input door      = { .pin = PIN_DOOR_SWITCH, .active_low = DOOR_ACTIVE_LOW };

void inputs_init(void)
{
    gpio_init(PIN_BTN_PLUS30);  gpio_set_dir(PIN_BTN_PLUS30, GPIO_IN);  gpio_pull_up(PIN_BTN_PLUS30);
    gpio_init(PIN_BTN_MINUS30); gpio_set_dir(PIN_BTN_MINUS30, GPIO_IN); gpio_pull_up(PIN_BTN_MINUS30);
    gpio_init(PIN_BTN_START);   gpio_set_dir(PIN_BTN_START, GPIO_IN);   gpio_pull_up(PIN_BTN_START);

    gpio_init(PIN_DOOR_SWITCH); gpio_set_dir(PIN_DOOR_SWITCH, GPIO_IN); gpio_pull_up(PIN_DOOR_SWITCH);

    uint32_t t = now_ms();

    bool p = read_active(btn_plus.pin,  btn_plus.active_low);
    btn_plus.stable = btn_plus.last_stable = btn_plus.last_raw = p;
    btn_plus.last_change = t;

    bool m = read_active(btn_minus.pin, btn_minus.active_low);
    btn_minus.stable = btn_minus.last_stable = btn_minus.last_raw = m;
    btn_minus.last_change = t;

    bool s = read_active(btn_start.pin, btn_start.active_low);
    btn_start.stable = btn_start.last_stable = btn_start.last_raw = s;
    btn_start.last_change = t;

    bool d = read_active(door.pin, door.active_low);   // true si puerta cerrada
    door.stable = door.last_stable = door.last_raw = d;
    door.last_change = t;
}

inputs read_inputs(void)
{
    inputs out = (inputs){0};

    bool raw_plus  = read_active(btn_plus.pin,  btn_plus.active_low);
    bool raw_minus = read_active(btn_minus.pin, btn_minus.active_low);
    bool raw_start = read_active(btn_start.pin, btn_start.active_low);

    bool raw_door_closed = read_active(door.pin, door.active_low); // true si puerta cerrada

    /* Botones: PULSO al pulsar (flanco de subida de "activo") */
    if (debounce_update(&btn_plus, raw_plus) && rising_edge(&btn_plus))
        out.suma30 = true;

    if (debounce_update(&btn_minus, raw_minus) && rising_edge(&btn_minus))
        out.resta30 = true;

    if (debounce_update(&btn_start, raw_start) && rising_edge(&btn_start))
        out.start = true;

    /* Puerta: NIVEL (estado actual estable, no pulso) */
    debounce_update(&door, raw_door_closed);
    out.puerta_cerrada = door.stable;
    out.puerta_abierta = !door.stable;

    return out;
}