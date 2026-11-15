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

add_executable(device_client_check
        device_client_check.cpp
        device_client.cpp
        test_runner.cpp
        ../firmware/protocol.c
)

target_include_directories(device_client_check PRIVATE ../firmware)
target_compile_features(device_client_check PRIVATE c_std_11 cxx_std_17)
target_link_libraries(device_client_check PRIVATE Qt6::Network)

add_executable(raketa_runner
        runner_main.cpp
        device_client.cpp
        test_runner.cpp
        ../firmware/protocol.c
)
target_include_directories(raketa_runner PRIVATE ../firmware)
target_compile_features(raketa_runner PRIVATE c_std_11 cxx_std_17)
target_link_libraries(raketa_runner PRIVATE Qt6::Network)

find_package(Threads REQUIRED)

add_executable(device_client_transport_check
        device_client_transport_check.cpp
        device_client.cpp
        ../firmware/protocol.c
)
target_include_directories(device_client_transport_check PRIVATE ../firmware)
target_compile_features(device_client_transport_check PRIVATE c_std_11 cxx_std_17)
target_link_libraries(device_client_transport_check PRIVATE Qt6::Network Threads::Threads)