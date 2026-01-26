#include "outputs.h"
#include <stdbool.h>

#include "pico/stdlib.h"          // GPIO + tiempos + alarmas
#include "hardware/i2c.h"         // I2C del RP2040

#include "lib/sh1106_i2c.h"       // driver SH1106 (I2C)
#include "lib/font_inconsolata.h" // fuente (bitmap caracteres)

/* ---------- Parámetros ajustables (según montaje) ---------- */
#define OLED_I2C          i2c0      // bus I2C usado (i2c0 / i2c1)
#define OLED_SDA_PIN      4         // GPIO conectado a SDA
#define OLED_SCL_PIN      5         // GPIO conectado a SCL
#define OLED_BAUDRATE     400000    // velocidad I2C (Hz)

#define OLED_ADDR         0x3C      // dirección I2C de la OLED (típica 0x3C)
#define OLED_W            128       // ancho de pantalla (px)
#define OLED_H            64        // alto de pantalla (px)

#define BUZZER_PIN        15        // GPIO del buzzer
/* ---------------------------------------------------------- */

#define OLED_COLOR_ON     1         // “1” = pixel encendido para este driver

/* Objeto principal del driver (incluye buffer y config I2C) */
static sh1106_t oled;

/* Último valor de segundos dibujado (para evitar repintar sin cambios) */
static int cached_seconds = -1;

/* Control de refresco: si no cambia nada, no redibujamos sin parar */
static bool force_redraw = true;
static absolute_time_t next_refresh;

/* -------------------- BUZZER (no bloqueante) -------------------- */
/*
   El FSM en STATE_DONE puede NO llamar outputs_update().
   Si el buzzer se apaga “dentro” de outputs_update, se quedaría sonando.
   Por eso lo apagamos con una alarma (callback) independiente.
*/
static bool buzzer_active = false;     // estado lógico del buzzer
static alarm_id_t buzzer_alarm_id = -1; // id de la alarma activa (si hay)

/* Encender/apagar buzzer por GPIO */
static inline void buzzer_set(bool on) {
    gpio_put(BUZZER_PIN, on ? 1 : 0);
}

/* Callback: se ejecuta cuando pasa el tiempo del pitido */
static int64_t buzzer_alarm_cb(alarm_id_t id, void *user_data) {
    (void)id;
    (void)user_data;

    buzzer_set(false);          // apaga físicamente
    buzzer_active = false;      // estado lógico
    buzzer_alarm_id = -1;       // ya no hay alarma pendiente

    return 0;                   // 0 = no repetir
}
/* ---------------------------------------------------------------- */

/* -------------------- OLED (init y dibujo) -------------------- */

/* Inicializa el bus I2C y la OLED SH1106 */
static void oled_init_hw(void) {
    i2c_init(OLED_I2C, OLED_BAUDRATE);                 // arranca I2C

    gpio_set_function(OLED_SDA_PIN, GPIO_FUNC_I2C);    // SDA como I2C
    gpio_set_function(OLED_SCL_PIN, GPIO_FUNC_I2C);    // SCL como I2C

    // Pull-ups típicos para I2C (según montaje pueden ser externos)
    gpio_pull_up(OLED_SDA_PIN);
    gpio_pull_up(OLED_SCL_PIN);

    // Inicializa el driver y limpia pantalla
    SH1106_init(&oled, OLED_I2C, OLED_ADDR, OLED_W, OLED_H);
    SH1106_clear(&oled);
    SH1106_draw(&oled);
}

/* Convierte segundos a "MM:SS" y lo dibuja en la pantalla */
static void draw_time_mmss(int seconds) {
    if (seconds < 0) seconds = 0;      // por seguridad, no negativos

    int m = seconds / 60;              // minutos
    int s = seconds % 60;              // segundos

    if (m > 99) m = 99;                // límite visual (2 dígitos)

    // buffer "MM:SS" + '\0'
    char buf[6];
    buf[0] = '0' + (m / 10);
    buf[1] = '0' + (m % 10);
    buf[2] = ':';
    buf[3] = '0' + (s / 10);
    buf[4] = '0' + (s % 10);
    buf[5] = '\0';

    // Pantalla: SOLO números y ":"
    SH1106_clear(&oled);
    SH1106_drawString(&oled, (char*)buf, 0, 0, OLED_COLOR_ON, font_inconsolata);
    SH1106_draw(&oled);
}
/* ------------------------------------------------------------- */

/* ===================== API DEL MÓDULO ===================== */

/* Se llama una vez al inicio del programa */
void outputs_init(void) {
    // Buzzer
    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);
    buzzer_set(false);

    // OLED
    oled_init_hw();

    // Estado inicial
    cached_seconds = -1;
    force_redraw = true;
    next_refresh = make_timeout_time_ms(0); // refresco inmediato al arrancar
}

/*
   Se llama desde el main en:
   STATE_CONFIG / STATE_HEATING / STATE_PAUSE

   Recibe un snapshot del temporizador (timer t).
*/
void outputs_update(timer t) {
    // Limito refresco aprox a 20 Hz si no hay cambios
    if (!force_redraw && absolute_time_diff_us(get_absolute_time(), next_refresh) > 0) {
        return;
    }
    next_refresh = make_timeout_time_ms(50);

    // Si cambia el tiempo, marcamos redibujo
    if (t.segundos != cached_seconds) {
        cached_seconds = t.segundos;
        force_redraw = true;
    }

    // Si no hay nada que hacer, salimos
    if (!force_redraw) return;
    force_redraw = false;

    // Dibujar tiempo
    draw_time_mmss(cached_seconds);
}

/* Se llama desde el main en STATE_OFF */
void outputs_off(void) {
    // Reinicio cache para que al volver a encender se redibuje sí o sí
    cached_seconds = -1;
    force_redraw = false;

    // Limpia OLED
    SH1106_clear(&oled);
    SH1106_draw(&oled);
}

/* ===================== ACTIONS (FSM) ===================== */

/* Mostrar 00:00 (normalmente en DONE) */
void action_show_zero(void) {
    cached_seconds = 0;
    force_redraw = true;
    draw_time_mmss(0);
}

/* Pitido de 800 ms sin bloquear (no depende de outputs_update) */
void action_buzzer_on(void) {
    // Si había una alarma previa, la cancelamos para no solapar pitidos
    if (buzzer_alarm_id >= 0) {
        cancel_alarm(buzzer_alarm_id);
        buzzer_alarm_id = -1;
    }

    buzzer_active = true;
    buzzer_set(true);

    // Programa apagado automático en 800 ms
    buzzer_alarm_id = add_alarm_in_ms(800, buzzer_alarm_cb, NULL, false);
}

/* Por si se quiere apagar manualmente desde fuera */
void action_buzzer_off(void) {
    // Cancela alarma si existía
    if (buzzer_alarm_id >= 0) {
        cancel_alarm(buzzer_alarm_id);
        buzzer_alarm_id = -1;
    }

    buzzer_active = false;
    buzzer_set(false);
}


