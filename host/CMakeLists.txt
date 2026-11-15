cmake_minimum_required(VERSION 3.16)
project(raketa_host LANGUAGES C CXX)

set(RAKETA_QT_MAJOR "6" CACHE STRING "Qt major version (5 or 6)")

if (RAKETA_QT_MAJOR STREQUAL "5")
    find_package(Qt5 5.15 REQUIRED COMPONENTS Network Widgets)
elseif (RAKETA_QT_MAJOR STREQUAL "6")
    find_package(Qt6 REQUIRED COMPONENTS Network Widgets)
else ()
    message(FATAL_ERROR "RAKETA_QT_MAJOR must be 5 or 6")
endif ()

add_executable(raketa_ping main.cpp)
target_compile_features(raketa_ping PRIVATE cxx_std_17)
target_link_libraries(raketa_ping PRIVATE Qt${RAKETA_QT_MAJOR}::Network)

add_executable(raketa_binary_check
        binary_check.cpp
        ../firmware/protocol.c
)
target_include_directories(raketa_binary_check PRIVATE ../firmware)
target_compile_features(raketa_binary_check PRIVATE c_std_11 cxx_std_17)
target_link_libraries(raketa_binary_check PRIVATE Qt${RAKETA_QT_MAJOR}::Network)

add_executable(device_client_check
        device_client_check.cpp
        device_client.cpp
        test_runner.cpp
        ../firmware/protocol.c
)

target_include_directories(device_client_check PRIVATE ../firmware)
target_compile_features(device_client_check PRIVATE c_std_11 cxx_std_17)
target_link_libraries(device_client_check PRIVATE Qt${RAKETA_QT_MAJOR}::Network)

add_executable(raketa_runner
        runner_main.cpp
        device_client.cpp
        test_runner.cpp
        ../firmware/protocol.c
)
target_include_directories(raketa_runner PRIVATE ../firmware)
target_compile_features(raketa_runner PRIVATE c_std_11 cxx_std_17)
target_link_libraries(raketa_runner PRIVATE Qt${RAKETA_QT_MAJOR}::Network)

find_package(Threads REQUIRED)

add_executable(device_client_transport_check
        device_client_transport_check.cpp
        device_client.cpp
        ../firmware/protocol.c
)
target_include_directories(device_client_transport_check PRIVATE ../firmware)
target_compile_features(device_client_transport_check PRIVATE c_std_11 cxx_std_17)
target_link_libraries(device_client_transport_check PRIVATE Qt${RAKETA_QT_MAJOR}::Network Threads::Threads)

add_executable(raketa_dashboard
        dashboard_main.cpp
        device_client.cpp
        test_runner.cpp
        ../firmware/protocol.c
)
target_include_directories(raketa_dashboard PRIVATE ../firmware)
target_compile_features(raketa_dashboard PRIVATE c_std_11 cxx_std_17)
target_link_libraries(raketa_dashboard PRIVATE Qt${RAKETA_QT_MAJOR}::Widgets Qt${RAKETA_QT_MAJOR}::Network)