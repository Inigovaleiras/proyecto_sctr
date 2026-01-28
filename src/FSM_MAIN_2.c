#include <stdbool.h>
#include <stdint.h>

#include "inputs.h"
#include "timer.h"
#include "outputs.h"

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
   INPUTS snapshot (1 lectura por ciclo)
   ======================= */

static inputs g_in;

/* =======================
   GENERADOR DE EVENTOS
   ======================= */

static eventos generador_eventos(estados st, inputs in, timer t)
{
    switch (st) {

        case STATE_OFF:
            if (in.suma30 || in.resta30) return EV_INTRODUCE_TIEMPO;
            return EV_NONE;

        case STATE_CONFIG:
            if (in.suma30 || in.resta30) return EV_INTRODUCE_TIEMPO;
            if (in.start && in.puerta_cerrada && t.segundos > 0) return EV_CALENTAR;
            return EV_NONE;

        case STATE_HEATING:
            /* microondas real: START suele pausar/stop */
            if (in.start) return EV_PARAR;

            /* pausa por puerta */
            if (in.puerta_abierta) return EV_PARAR;

            /* fin */
            if (t.segundos == 0)  return EV_TERMINADO;

            return EV_NONE;

        case STATE_PAUSE:
            /* FIX crítico: si llegas a PAUSE y el tiempo cae a 0, no te quedas muerto */
            if (t.segundos == 0) return EV_TERMINADO;

            /* reanudar cuando se cierre la puerta y aún quede tiempo */
            if (in.start && in.puerta_cerrada && t.segundos > 0) return EV_REANUDAR;

            return EV_NONE;

        case STATE_DONE:
            /* aquí usamos START como “reset” (tu diseño actual) */
            if (in.start) return EV_RESET;
            return EV_NONE;

        default:
            return EV_NONE;
    }
}

/* =======================
   TRANSICIONES
   ======================= */

static estados trans_off_introducir_tiempo(void)
{
    if (g_in.suma30)  timer_add_30();
    if (g_in.resta30) timer_sub_30();
    return STATE_CONFIG;
}

/* --- CONFIG --- */

static estados trans_config_introducir_tiempo(void)
{
    if (g_in.suma30)  timer_add_30();
    if (g_in.resta30) timer_sub_30();
    return STATE_CONFIG;
}

static estados trans_config_calentar(void)
{
    action_start_timer();
    return STATE_HEATING;
}

static estados trans_config_parar(void)
{
    action_stop_timer();
    return STATE_PAUSE;
}

/* --- HEATING --- */

static estados trans_heating_parar(void)
{
    action_stop_timer();
    return STATE_PAUSE;
}

static estados trans_heating_terminado(void)
{
    action_stop_timer();
    action_buzzer_on();
    action_show_zero();
    return STATE_DONE;
}

/* --- PAUSE --- */

static estados trans_pause_reanudar(void)
{
    action_start_timer();
    return STATE_HEATING;
}

static estados trans_pause_terminado(void)
{
    action_stop_timer();
    action_buzzer_on();
    action_show_zero();
    return STATE_DONE;
}

/* --- DONE --- */

static estados trans_done_reset(void)
{
    action_reset_all();
    timer_reset();
    return STATE_OFF;
}

/* =======================
   TABLA DE TRANSICIONES
   ======================= */

static estados (*trans_table[N_STATES][N_EVENTS])(void) = {

    [STATE_OFF] = {
        [EV_INTRODUCE_TIEMPO] = trans_off_introducir_tiempo,
    },

    [STATE_CONFIG] = {
        [EV_INTRODUCE_TIEMPO] = trans_config_introducir_tiempo,
        [EV_CALENTAR]         = trans_config_calentar,
        [EV_PARAR]            = trans_config_parar,
    },

    [STATE_HEATING] = {
        [EV_PARAR]      = trans_heating_parar,
        [EV_TERMINADO]  = trans_heating_terminado,
    },

    [STATE_PAUSE] = {
        [EV_REANUDAR]   = trans_pause_reanudar,
        [EV_TERMINADO]  = trans_pause_terminado, /* FIX crítico */
    },

    [STATE_DONE] = {
        [EV_RESET] = trans_done_reset,
    }
};

/* =======================
   FSM STEP
   ======================= */

static estados fsm_step(estados estado_actual, eventos evento_actual)
{
    if (evento_actual >= N_EVENTS) return estado_actual;

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
    estados estado_prev   = N_STATES;

    outputs_init();
    inputs_init();

    timer temporizador = { .segundos = 0 };
    timer_init(&temporizador);

    while (1) {

        /* 1) inputs (1 lectura por ciclo) */
        g_in = read_inputs();

        /* 2) snapshot del tiempo */
        temporizador = timer_get();

        /* 3) evento */
        eventos evento_actual = generador_eventos(estado_actual, g_in, temporizador);

        /* 4) transición */
        estado_actual = fsm_step(estado_actual, evento_actual);

        /* 5) refresca tiempo (por si +30/-30 en transición) */
        temporizador = timer_get();

        /* 6) acciones “al entrar” (evitar repintar en bucle OFF/DONE) */
        if (estado_actual != estado_prev) {
            if (estado_actual == STATE_OFF) {
                action_show_zero();
            } else if (estado_actual == STATE_DONE) {
                action_show_zero();
            }
            estado_prev = estado_actual;
        }

        /* 7) salidas continuas */
        switch (estado_actual) {
            case STATE_CONFIG:
                action_show_zero();
                break;
            case STATE_HEATING:
                action_show_zero();
                break;
            case STATE_PAUSE:
                outputs_update(temporizador);
                break;

            case STATE_OFF:
                action_show_zero();
            case STATE_DONE:
            default:
                /* OFF y DONE ya se manejan en “al entrar” */
                break;
        }
    }
}











