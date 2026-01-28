/*
    Módulo de temporización del microondas.

    Este fichero NO contiene lógica de la FSM ni hardware de entradas/salidas.
    Su función es únicamente dar servicio de tiempo:

      - Mantener el contador en segundos
      - Descontar 1s mediante un temporizador hardware (tick cada 1000 ms)
      - Avisar cuando el contador llega a 0 (timeout -> evento para la FSM)
   
*/
#ifndef TIMER_H
#define TIMER_H

#include <stdbool.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"

typedef struct {
    int segundos;
} timer;

// Engancha este módulo a la variable real del main
void timer_init(timer *t_ptr);

// Control, la FSM debe llamar a estas funciones desde las acciones
void action_start_timer(void);
void action_stop_timer(void);

// Servicio de tiempo
void timer_add_30(void); // Suma 30 segundos al contador
void timer_sub_30(void); // Resta 30 segundos
void timer_reset(void); //Pone el contador a 0 y limpia el "timeout"

// Tick real 
void timer_tick_isr(void); //resta 1 segundo si segundos > 0

/* lectura del temporizador (snapshot) */
timer timer_get(void);

#endif
