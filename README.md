# proyecto_sctr
Proyecto SCTR: microondas con Raspberry Pi Pico (FSM + inputs + timer + outputs)

Integrantes:
- Adri (FSM / lógica central)
- Miguel (Inputs / entradas)
- Alan (Timer / tiempo)
- Íñigo (Outputs / pantalla + buzzer)

Descripción:
Simulación de microondas con arquitectura modular (FSM table-driven + módulos de entradas, temporizador y salidas).

- Esquema eléctrico y montaje: [docs/Esquema-eléctrico-y-montaje.pdf](docs/Esquema-eléctrico-y-montaje.pdf)

- Breve descripción técnica:
Este proyecto simula un microondas con una Raspberry Pi Pico siguiendo una estructura modular. La lógica principal se organiza con una máquina de estados que gestiona el apagado, la configuración del tiempo, el calentamiento, la pausa y el fin. Las entradas se leen desde botones físicos: +30, -30 y start funcionan como pulsaciones (con antirrebote), y la puerta se representa con un botón mantenido que actúa como estado abierto/cerrado. El temporizador actualiza la cuenta atrás cada segundo y el módulo de salidas muestra el tiempo en una pantalla OLED SH1106 por I2C y activa el buzzer cuando el tiempo llega a cero.

