/*
    Módulo de ENTRADAS del microondas (Parte 2).

    Este fichero NO contiene lógica de la FSM ni gestión del temporizador ni salidas.
    Su función es únicamente dar servicio de lectura de entradas:

      - Leer botones (+30, -30, START)
      - Leer sensor de puerta (abierta / cerrada)
      - Aplicar antirrebote (debounce) para evitar rebotes mecánicos
      - Botones: detectar flancos para generar “pulsos” de un solo ciclo
      - Puerta: devolver NIVEL estable (estado actual abierto/cerrado)
*/
#ifndef INPUTS_H
#define INPUTS_H

#include <stdbool.h>
#include <stdint.h>

/* Estructura de entradas: flags leídos de botones y sensor */
typedef struct {
    bool suma30;          /* Botón +30 pulsado (pulso por flanco) */
    bool resta30;         /* Botón -30 pulsado (pulso por flanco) */
    bool start;           /* Botón START pulsado (pulso por flanco) */
    bool puerta_abierta;  /* Estado estable: puerta abierta (nivel) */
    bool puerta_cerrada;  /* Estado estable: puerta cerrada (nivel) */
} inputs;

/* Inicializa GPIO de entradas y estados internos de antirrebote */
void inputs_init(void);

/* Lee entradas físicas (debounce + flancos en botones) y devuelve un struct inputs */
inputs read_inputs(void);

#endif


