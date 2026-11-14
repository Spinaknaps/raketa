cmake_minimum_required(VERSION 3.16)
project(raketa_host LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Network)

add_executable(raketa_ping main.cpp)
target_compile_features(raketa_ping PRIVATE cxx_std_17)
target_link_libraries(raketa_ping PRIVATE Qt6::Network)