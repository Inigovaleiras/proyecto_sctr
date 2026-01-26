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
   PROTOTIPOS (externos)
   ======================= */

inputs read_inputs(void);

/* timer hardware */
void  timer_init(timer *t_ptr);
timer timer_get(void);

/* acciones (salidas / timer) */
void action_show_zero(void);
void action_start_timer(void);
void action_stop_timer(void);
void action_buzzer_on(void);
void action_reset_all(void);

/* outputs */
void outputs_update(timer t);
void outputs_off(void);

/* funciones de timer */
void timer_add_30(void);
void timer_sub_30(void);

/* =======================
   INPUTS “snapshot” (1 lectura por ciclo)
   ======================= */

static inputs g_in;

/* =======================
   GENERADOR DE EVENTOS (CORREGIDO: depende del estado)
   ======================= */

eventos generador_eventos(estados st, inputs in, timer t)
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
            if (in.puerta_abierta) return EV_PARAR;
            if (t.segundos == 0)  return EV_TERMINADO;
            return EV_NONE;

        case STATE_PAUSE:
            /* reanudar cuando se cierre la puerta y aún quede tiempo */
            if (in.start && in.puerta_cerrada && t.segundos > 0) return EV_REANUDAR;

            /* si quieres que al llegar a 0 en pausa también vaya a DONE, descomenta:
            if (t.segundos == 0) return EV_TERMINADO;
            */
            return EV_NONE;

        case STATE_DONE:
            /* aquí no tienes botón reset en inputs, así que no invento nada */
            return EV_NONE;

        default:
            return EV_NONE;
    }
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
    /* usamos la lectura del ciclo (g_in), no volvemos a leer inputs */
    if (g_in.suma30)  timer_add_30();
    if (g_in.resta30) timer_sub_30();

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

estados trans_config_parar(void)
{
    action_stop_timer();
    return STATE_PAUSE;
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

/* Si activas EV_TERMINADO en PAUSE arriba, necesitas esta transición: */
estados trans_pause_terminado(void)
{
    action_stop_timer();
    action_buzzer_on();
    action_show_zero();
    return STATE_DONE;
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
        [EV_CALENTAR]         = trans_config_calentar,
        [EV_PARAR]            = trans_config_parar,
        [EV_RESET]            = trans_config_reset,
    },

    [STATE_HEATING] = {
        [EV_PARAR]      = trans_heating_parar,
        [EV_TERMINADO]  = trans_heating_terminado,
    },

    [STATE_PAUSE] = {
        [EV_REANUDAR]   = trans_pause_reanudar,
        /* si descomentaste EV_TERMINADO en PAUSE:
        [EV_TERMINADO] = trans_pause_terminado,
        */
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
    if (evento_actual >= N_EVENTS) return estado_actual;

    if (trans_table[estado_actual][evento_actual]) {
        return trans_table[estado_actual][evento_actual]();
    }

    return estado_actual;
}

/* =======================
   MAIN (con timer_init + refresh de temporizador)
   ======================= */

int main(void)
{
    estados estado_actual = STATE_OFF;
    eventos evento_actual;

    timer temporizador = { .segundos = 0 };

    /* arranca el timer hardware apuntando al temporizador real */
    timer_init(&temporizador);

    while (1) {

        /* 1) una sola lectura de inputs por ciclo */
        g_in = read_inputs();

        /* 2) lectura del tiempo actual */
        temporizador = timer_get();

        /* 3) evento según estado + entradas + tiempo */
        evento_actual = generador_eventos(estado_actual, g_in, temporizador);

        /* 4) transición */
        estado_actual = fsm_step(estado_actual, evento_actual);

        /* 5) refresco de temporizador por si en esta transición se hizo +30/-30 */
        temporizador = timer_get();

        /* 6) salidas */
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

}


