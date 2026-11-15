cmake_minimum_required(VERSION 3.16)
project(raketa_host LANGUAGES C CXX)

set(RAKETA_QT_MAJOR "6" CACHE STRING "Qt major version (5 or 6)")

find_package(Qt${RAKETA_QT_MAJOR} REQUIRED COMPONENTS Network Widgets Sql)

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

if(NOT EXISTS "${TCL_INCLUDE_PATH}/tcl.h" OR NOT EXISTS "${TCL_LIBRARY}")
    message(FATAL_ERROR "Set TCL_INCLUDE_PATH and TCL_LIBRARY to your Tcl installation")
endif()

add_executable(raketa_dashboard
        dashboard_main.cpp
        scenario_loader.cpp
        history_store.cpp
        device_client.cpp
        test_runner.cpp
        ../firmware/protocol.c
)
target_include_directories(raketa_dashboard PRIVATE ../firmware "${TCL_INCLUDE_PATH}")
target_compile_features(raketa_dashboard PRIVATE c_std_11 cxx_std_17)
target_link_libraries(raketa_dashboard PRIVATE
        Qt${RAKETA_QT_MAJOR}::Widgets
        Qt${RAKETA_QT_MAJOR}::Network
        Qt${RAKETA_QT_MAJOR}::Sql
        "${TCL_LIBRARY}"
)

target_include_directories(raketa_dashboard PRIVATE
        ../firmware
        "${TCL_INCLUDE_PATH}"
)

option(RAKETA_BUILD_TCL_CHECK "Build the Tcl smoke check" OFF)

if(RAKETA_BUILD_TCL_CHECK)


    add_executable(tcl_check
            tcl_check.cpp
            scenario_loader.cpp
    )
    target_compile_features(tcl_check PRIVATE cxx_std_17)
    target_include_directories(tcl_check PRIVATE "${TCL_INCLUDE_PATH}")
    target_link_libraries(tcl_check PRIVATE "${TCL_LIBRARY}")
endif()