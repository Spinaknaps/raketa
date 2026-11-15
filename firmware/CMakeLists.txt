cmake_minimum_required(VERSION 3.16)
project(raketa_firmware LANGUAGES C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(raketa_firmware
    startup.S
    main.c
    protocol.c
    drivers/uart.c
    drivers/timer.c
    model/pressure_model.c
    commands/command_handler.c
)
set_target_properties(raketa_firmware PROPERTIES
    OUTPUT_NAME firmware
    SUFFIX ".elf"
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)
target_include_directories(raketa_firmware PRIVATE . drivers model commands)
target_compile_options(raketa_firmware PRIVATE
    -mcpu=cortex-m3 -mthumb
    $<$<COMPILE_LANGUAGE:C>:-ffreestanding;-Wall;-Wextra;-Wstrict-prototypes>
)
target_link_options(raketa_firmware PRIVATE
    -mcpu=cortex-m3 -mthumb -nostdlib
    "-T${CMAKE_CURRENT_SOURCE_DIR}/linker.ld"
)
set_property(TARGET raketa_firmware APPEND PROPERTY
    LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld"
)
