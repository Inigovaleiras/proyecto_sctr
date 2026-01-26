#ifndef OUTPUTS_H
#define OUTPUTS_H

#include <stdint.h>

/* Estructura usada para el temporizador (segundos actuales) */
typedef struct {
    int segundos;
} timer;

/* Temporizador compartido (definido en otro archivo) */
extern volatile timer temporizador;

/* Inicializa OLED SH1106 (I2C) y buzzer */
void outputs_init(void);

/* Actualiza OLED/buzzer de forma no bloqueante */
void outputs_update(void);

/* Funciones llamadas desde la FSM */
void action_show_time(void);
void action_show_zero(void);
void action_buzzer_on(void);
void action_buzzer_off(void);

#endif
