# --- PARTE 4 (outputs + SH1106) ---
target_sources(${PROJECT_NAME} PRIVATE
    parte4_outputs/outputs.c
    parte4_outputs/lib/sh1106_i2c.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/parte4_outputs
    ${CMAKE_CURRENT_LIST_DIR}/parte4_outputs/lib
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    pico_stdlib
    hardware_i2c
)

