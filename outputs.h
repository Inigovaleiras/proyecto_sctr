#ifndef OUTPUTS_H
#define OUTPUTS_H

#include "timer.h"

/* Init del módulo (1 vez) */
void outputs_init(void);

/* Refresco con snapshot del temporizador */
void outputs_update(timer t);

/* Apagar pantalla */
void outputs_off(void);

/* Acciones que llama la FSM */
void action_show_zero(void);
void action_buzzer_on(void);
void action_buzzer_off(void);

#endif
