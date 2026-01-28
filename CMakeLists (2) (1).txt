cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(microondas C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(microondas
    # Código del proyecto (en src/)
    src/FSM_MAIN_2.c
    src/inputs.c
    src/timer.c
    src/outputs.c

    # Driver SH1106 (se compila)
    lib/sh1106_i2c.c
)

# IMPORTANTE:
# Como tú incluyes "lib/archivo.h", el compilador debe buscar desde la raíz del repo.
target_include_directories(microondas PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}        # raíz del repo -> para "lib/..."
    ${CMAKE_CURRENT_LIST_DIR}/src    # para headers del proyecto
    ${CMAKE_CURRENT_LIST_DIR}/lib    # útil si algún include no lleva "lib/"
)

target_link_libraries(microondas
    pico_stdlib
    hardware_i2c
)


pico_add_extra_outputs(microondas)
