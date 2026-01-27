#include "timer.h"
#include "hardware/sync.h"

static struct repeating_timer rt;
static volatile bool running = false;

// “time_seconds” real: apuntamos al temporizador del main
static volatile timer *t = NULL;

// Función a la que la raspberry va a llamar cad 1sg
static bool cb_1s(struct repeating_timer *x) {
    (void)x;  // evitamos warning "unused parameter"
    timer_tick_isr();
    return true;
}


void timer_init(timer *t_ptr) {
    //Guardamos la dirección del temporizador real del main
    t = t_ptr;
    /*
        Creamos un temporizador hardware que llama cb_1s cada 1000 ms.
        &rt es donde el SDK guarda la info del timer creado.
    */
    add_repeating_timer_ms(1000, cb_1s, NULL, &rt);
}

// lectura del temporizador (snapshot) 
timer timer_get(void) {
    timer snap = { .segundos = 0 };
    if (t == NULL) return snap;

    uint32_t s = save_and_disable_interrupts();
    snap.segundos = t->segundos;
    restore_interrupts(s);

    return snap;
}

// Órdenes de control
void action_start_timer(void) { running = true; }
void action_stop_timer(void)  { running = false; }

void timer_add_30(void) {
    if (t == NULL) return;
/*
     Para que no se pisen el callback y el aumento de 30sg, desactivamos interrupciones un instante
    */
    uint32_t s = save_and_disable_interrupts();
    t->segundos += 30;
    restore_interrupts(s); // Las activamos de nuevo
}

// Lo mismo pero esta vez restando 30sg
void timer_sub_30(void) {
    if (t == NULL) return;

    uint32_t s = save_and_disable_interrupts();
    if (t->segundos >= 30) t->segundos -= 30;
    else t->segundos = 0;
    restore_interrupts(s);
}

//Reseteo del tiempo poniendolo a cero
void timer_reset(void) {
    if (t == NULL) return;

    uint32_t s = save_and_disable_interrupts();
    t->segundos = 0;
    restore_interrupts(s);
    running = false;
}

void timer_tick_isr(void) {
    if (t == NULL) return;
    if (!running) return;

    if (t->segundos > 0) {
        t->segundos--;
        if (t->segundos == 0) {
            running = false; // se auto-para al llegar a 0
        }
    }
}




