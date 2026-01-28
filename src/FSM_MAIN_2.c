#include <stdbool.h>

/* =======================
   ESTADOS
   ======================= */

typedef enum {
    STATE_OFF,
    STATE_CONFIG,
    STATE_HEATING,
    STATE_PAUSE,
    STATE_DONE,
    N_STATES
} estados;

/* =======================
   EVENTOS
   ======================= */

typedef enum {
    EV_NONE,
    EV_INTRODUCE_TIEMPO,
    EV_CALENTAR,
    EV_PARAR,
    EV_REANUDAR,
    EV_TERMINADO,
    EV_RESET,
    N_EVENTS
} eventos;

/* =======================
   ESTRUCTURAS EXTERNAS
   ======================= */

typedef struct {
    bool suma30;
    bool resta30;
    bool start;
    bool puerta_abierta;
    bool puerta_cerrada;
} inputs;

typedef struct {
    int segundos;
} timer;

/* =======================
   PROTOTIPOS
   ======================= */

inputs read_inputs(void);
timer  timer_get(void);

/* acciones (salidas / timer) */
void action_show_zero(void);
void action_start_timer(void);
void action_stop_timer(void);
void action_buzzer_on(void);
void action_reset_all(void);

/* funciones de timer (NUEVO) */
void timer_add_30(void);
void timer_sub_30(void);

/* =======================
   GENERADOR DE EVENTOS
   ======================= */

eventos generador_eventos(inputs in, timer t)
{
    if (in.suma30 || in.resta30) {
        return EV_INTRODUCE_TIEMPO;
    }

    if (in.start && in.puerta_cerrada && t.segundos > 0) {
        return EV_CALENTAR;
    }

    if (in.puerta_abierta) {
        return EV_PARAR;
    }

    if (in.puerta_cerrada) {
        return EV_REANUDAR;
    }

    if (t.segundos == 0) {
        return EV_TERMINADO;
    }

    return EV_NONE;
}

/* =======================
   TRANSICIONES
   ======================= */

estados trans_off_introducir_tiempo(void)
{
    return STATE_CONFIG;
}

/* --- CONFIG --- */

estados trans_config_introducir_tiempo(void)
{
    /* la FSM decide, el timer ejecuta */
    inputs in = read_inputs();

    if (in.suma30) {
        timer_add_30();
    }
    if (in.resta30) {
        timer_sub_30();
    }

    return STATE_CONFIG;
}

estados trans_config_calentar(void)
{
    action_start_timer();
    return STATE_HEATING;
}

estados trans_config_reset(void)
{
    action_reset_all();
    return STATE_OFF;
}

/* --- HEATING --- */

estados trans_heating_parar(void)
{
    action_stop_timer();
    return STATE_PAUSE;
}

estados trans_heating_terminado(void)
{
    action_stop_timer();
    action_buzzer_on();
    action_show_zero();
    return STATE_DONE;
}

/* --- PAUSE --- */

estados trans_pause_reanudar(void)
{
    action_start_timer();
    return STATE_HEATING;
}

/* --- DONE --- */

estados trans_done_reset(void)
{
    action_reset_all();
    return STATE_OFF;
}

/* =======================
   TABLA DE TRANSICIONES
   ======================= */

estados (*trans_table[N_STATES][N_EVENTS])(void) = {

    [STATE_OFF] = {
        [EV_INTRODUCE_TIEMPO] = trans_off_introducir_tiempo,
    },

    [STATE_CONFIG] = {
        [EV_INTRODUCE_TIEMPO] = trans_config_introducir_tiempo,
        [EV_CALENTAR]        = trans_config_calentar,
        [EV_RESET]           = trans_config_reset,
    },

    [STATE_HEATING] = {
        [EV_PARAR]     = trans_heating_parar,
        [EV_TERMINADO] = trans_heating_terminado,
    },

    [STATE_PAUSE] = {
        [EV_REANUDAR] = trans_pause_reanudar,
    },

    [STATE_DONE] = {
        [EV_RESET] = trans_done_reset,
    }
};

/* =======================
   FSM STEP
   ======================= */

estados fsm_step(estados estado_actual, eventos evento_actual)
{
    if (evento_actual >= N_EVENTS) {
        return estado_actual;
    }

    if (trans_table[estado_actual][evento_actual]) {
        return trans_table[estado_actual][evento_actual]();
    }

    return estado_actual;
}

/* =======================
   MAIN
   ======================= */

int main(void)
{
    estados estado_actual = STATE_OFF;
    eventos evento_actual;

    inputs entradas;
    timer temporizador;

    while (1) {

        entradas     = read_inputs();
        temporizador = timer_get();

        evento_actual = generador_eventos(entradas, temporizador);
        estado_actual = fsm_step(estado_actual, evento_actual);

        /* refresco de salidas según estado */
        switch (estado_actual) {

            case STATE_CONFIG:
            case STATE_HEATING:
            case STATE_PAUSE:
                outputs_update(temporizador);
                break;

            case STATE_DONE:
                action_show_zero();
                break;

            case STATE_OFF:
            default:
                outputs_off();
                break;
        }
    }
}
