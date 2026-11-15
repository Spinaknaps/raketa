set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

find_program(ARM_GCC NAMES arm-none-eabi-gcc)
if(NOT ARM_GCC)
    message(FATAL_ERROR "Add arm-none-eabi-gcc to PATH")
endif()

set(CMAKE_C_COMPILER "${ARM_GCC}")
set(CMAKE_ASM_COMPILER "${ARM_GCC}")
