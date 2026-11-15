cmake_minimum_required(VERSION 3.16)
project(raketa_host LANGUAGES C CXX)

find_package(Qt6 REQUIRED COMPONENTS Network)

add_executable(raketa_ping main.cpp)
target_compile_features(raketa_ping PRIVATE cxx_std_17)
target_link_libraries(raketa_ping PRIVATE Qt6::Network)

add_executable(raketa_binary_check
        binary_check.cpp
        ../firmware/protocol.c
)
target_include_directories(raketa_binary_check PRIVATE ../firmware)
target_compile_features(raketa_binary_check PRIVATE c_std_11 cxx_std_17)
target_link_libraries(raketa_binary_check PRIVATE Qt6::Network)