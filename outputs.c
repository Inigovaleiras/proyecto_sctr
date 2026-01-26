#include "outputs.h"            // interfaz del módulo (actions + init/update)
#include <stdbool.h>            // tipo bool

#include "pico/stdlib.h"        // GPIO + tiempos (absolute_time_t, etc.)
#include "hardware/i2c.h"       // I2C del RP2040

#include "lib/sh1106_i2c.h"     // driver SH1106 (I2C)
#include "lib/font_inconsolata.h" // fuente (bitmap caracteres)

/* ---------- Parámetros ajustables (según montaje) ---------- */
#define OLED_I2C          i2c0      // bus I2C usado
#define OLED_SDA_PIN      4         // pin GPIO SDA
#define OLED_SCL_PIN      5         // pin GPIO SCL
#define OLED_BAUDRATE     400000    // velocidad I2C

#define OLED_ADDR         0x3C      // dirección I2C OLED
#define OLED_W            128       // ancho OLED
#define OLED_H            64        // alto OLED

#define BUZZER_PIN        15        // pin GPIO buzzer
/* ---------------------------------------------------------- */

#define OLED_COLOR_ON     1         // píxel encendido

/* Objeto principal del driver (pantalla + buffer) */
static sh1106_t oled;

/* Modo de pantalla: apagada o MM:SS */
typedef enum { DISP_OFF, DISP_TIME } disp_mode_t;
static disp_mode_t disp_mode = DISP_OFF;

/* Último valor de segundos dibujado */
static int cached_seconds = -1;

/* Control de refresco (evitar redibujar sin parar) */
static bool force_redraw = true;
static absolute_time_t next_refresh;

/* Control de buzzer sin bloquear */
static bool buzzer_active = false;
static absolute_time_t buzzer_end;

/* Encender/apagar buzzer por GPIO */
static inline void buzzer_set(bool on) {
    gpio_put(BUZZER_PIN, on ? 1 : 0);
}

/* Inicialización I2C + OLED */
static void oled_init_hw(void) {
    i2c_init(OLED_I2C, OLED_BAUDRATE);           // arranca I2C

    gpio_set_function(OLED_SDA_PIN, GPIO_FUNC_I2C); // SDA como I2C
    gpio_set_function(OLED_SCL_PIN, GPIO_FUNC_I2C); // SCL como I2C

    gpio_pull_up(OLED_SDA_PIN);                  // pull-up SDA
    gpio_pull_up(OLED_SCL_PIN);                  // pull-up SCL

    SH1106_init(&oled, OLED_I2C, OLED_ADDR, OLED_W, OLED_H); // init driver
    SH1106_clear(&oled);                         // buffer a 0
    SH1106_draw(&oled);                          // manda buffer
}

/* Dibuja el string "MM:SS" usando la fuente */
static void oled_draw_digits(uint8_t x, uint8_t y, const char *str) {
    SH1106_drawString(&oled, (char*)str, x, y, OLED_COLOR_ON, font_inconsolata);
}

/* Convierte segundos a "MM:SS" y lo pinta */
static void draw_time_mmss(int seconds) {
    if (seconds < 0) seconds = 0;                // evita negativos

    int m = seconds / 60;                        // minutos
    int s = seconds % 60;                        // segundos

    if (m > 99) m = 99;                          // limita a 2 dígitos

    char buf[6];                                 // "MM:SS" + '\0'
    buf[0] = '0' + (m / 10);
    buf[1] = '0' + (m % 10);
    buf[2] = ':';
    buf[3] = '0' + (s / 10);
    buf[4] = '0' + (s % 10);
    buf[5] = '\0';

    SH1106_clear(&oled);                         // limpia buffer
    oled_draw_digits(0, 0, buf);                 // dibuja texto
    SH1106_draw(&oled);                          // actualiza OLED
}

/* Init del módulo (llamar 1 vez) */
void outputs_init(void) {
    gpio_init(BUZZER_PIN);                       // prepara buzzer
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);
    buzzer_set(false);                           // buzzer apagado

    oled_init_hw();                              // prepara OLED

    next_refresh = make_timeout_time_ms(0);      // refresco inmediato
    force_redraw = true;                         // fuerza primer dibujo
}

/* Action: mostrar tiempo (llamada desde FSM) */
void action_show_time(void) {
    disp_mode = DISP_TIME;                       // modo tiempo
    cached_seconds = temporizador.segundos;      // copia valor actual
    force_redraw = true;                         // redibuja ya
}

/* Action: mostrar 00:00 (llamada desde FSM) */
void action_show_zero(void) {
    disp_mode = DISP_TIME;                       // modo tiempo
    cached_seconds = 0;                          // fija a cero
    force_redraw = true;                         // redibuja ya
}

/* Action: iniciar pitido (llamada desde FSM) */
void action_buzzer_on(void) {
    buzzer_active = true;                        // marca activo
    buzzer_set(true);                            // enciende buzzer
    buzzer_end = make_timeout_time_ms(800);      // apagar en 800ms
}

/* Action: apagar pitido (llamada desde FSM o auto) */
void action_buzzer_off(void) {
    buzzer_active = false;                       // desactiva
    buzzer_set(false);                           // apaga buzzer
}

/* Update del módulo (llamar en cada iteración del while) */
void outputs_update(void) {
    /* Apagado automático del buzzer */
    if (buzzer_active && absolute_time_diff_us(get_absolute_time(), buzzer_end) <= 0) {
        action_buzzer_off();
    }

    /* Limita refresco a ~20 Hz si no hay cambios */
    if (!force_redraw && absolute_time_diff_us(get_absolute_time(), next_refresh) > 0) {
        return;
    }
    next_refresh = make_timeout_time_ms(50);

    /* Si estamos en modo tiempo, detecta cambios de segundos */
    if (disp_mode == DISP_TIME) {
        int now_s = temporizador.segundos;
        if (now_s != cached_seconds) {
            cached_seconds = now_s;
            force_redraw = true;
        }
    }

    /* Si no hay nada que actualizar, salir */
    if (!force_redraw) return;
    force_redraw = false;

    /* Dibujo final según modo */
    if (disp_mode == DISP_OFF) {
        SH1106_clear(&oled);
        SH1106_draw(&oled);
    } else {
        draw_time_mmss(cached_seconds);
    }
}


