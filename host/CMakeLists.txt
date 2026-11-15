cmake_minimum_required(VERSION 3.16)
project(raketa_host LANGUAGES C CXX)

set(RAKETA_QT_MAJOR "6" CACHE STRING "Qt major version (5 or 6)")
find_package(Qt${RAKETA_QT_MAJOR} QUIET REQUIRED COMPONENTS Network Widgets Sql)
find_package(Threads REQUIRED)

if(NOT EXISTS "${TCL_INCLUDE_PATH}/tcl.h" OR NOT EXISTS "${TCL_LIBRARY}")
    message(FATAL_ERROR "Set TCL_INCLUDE_PATH and TCL_LIBRARY to your Tcl installation")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra)
endif()

add_library(raketa_protocol STATIC ../firmware/protocol.c)
target_include_directories(raketa_protocol PUBLIC ../firmware)
target_compile_features(raketa_protocol PUBLIC c_std_11)

add_library(raketa_device STATIC device/device_client.cpp)
target_include_directories(raketa_device PUBLIC device)
target_link_libraries(raketa_device PRIVATE raketa_protocol PUBLIC Qt${RAKETA_QT_MAJOR}::Network)
target_compile_features(raketa_device PUBLIC cxx_std_17)

add_library(raketa_testing STATIC testing/test_runner.cpp testing/run_worker.cpp)
target_include_directories(raketa_testing PUBLIC testing)
target_link_libraries(raketa_testing PUBLIC raketa_device)

add_library(raketa_history STATIC
    history/history_database.cpp
    history/history_schema.cpp
    history/history_store.cpp
)
target_include_directories(raketa_history PUBLIC history)
target_compile_features(raketa_history PUBLIC cxx_std_17)
target_link_libraries(raketa_history PUBLIC Qt${RAKETA_QT_MAJOR}::Sql)
target_include_directories(raketa_history PUBLIC testing)

add_library(raketa_scenarios STATIC scenarios/scenario_loader.cpp)
target_include_directories(raketa_scenarios PUBLIC scenarios testing "${TCL_INCLUDE_PATH}")
target_compile_features(raketa_scenarios PUBLIC cxx_std_17)
target_link_libraries(raketa_scenarios PUBLIC "${TCL_LIBRARY}")

add_executable(raketa_runner apps/runner_main.cpp)
target_link_libraries(raketa_runner PRIVATE raketa_testing)

add_executable(raketa_dashboard
    apps/dashboard_main.cpp
    ui/dashboard_view.cpp
    ui/dashboard_session.cpp
    ui/history_dialog.cpp
    ui/pressure_chart.cpp
)
target_include_directories(raketa_dashboard PRIVATE ui)
target_link_libraries(raketa_dashboard PRIVATE
    raketa_testing raketa_history raketa_scenarios Qt${RAKETA_QT_MAJOR}::Widgets
)

add_executable(raketa_binary_check ../tests/host/binary_check.cpp)
target_compile_features(raketa_binary_check PRIVATE cxx_std_17)
target_link_libraries(raketa_binary_check PRIVATE raketa_protocol Qt${RAKETA_QT_MAJOR}::Network)

add_executable(device_client_check ../tests/host/device_client_check.cpp)
target_link_libraries(device_client_check PRIVATE raketa_testing Threads::Threads)

add_executable(device_client_transport_check ../tests/host/device_client_transport_check.cpp)
target_link_libraries(device_client_transport_check PRIVATE raketa_device raketa_protocol Threads::Threads)

option(RAKETA_BUILD_TCL_CHECK "Build the Tcl smoke check" OFF)
if(RAKETA_BUILD_TCL_CHECK)
    add_executable(tcl_check ../tests/host/tcl_check.cpp)
    target_link_libraries(tcl_check PRIVATE raketa_scenarios)
endif()


enable_testing()

add_executable(protocol_check ../tests/protocol_check.c)
target_link_libraries(protocol_check PRIVATE raketa_protocol)
add_test(NAME protocol_check COMMAND protocol_check)

add_library(raketa_pressure_model STATIC ../firmware/model/pressure_model.c)
target_include_directories(raketa_pressure_model PUBLIC ../firmware/model)
target_compile_features(raketa_pressure_model PUBLIC c_std_11)

add_executable(pressure_model_check ../tests/pressure_model_check.c)
target_link_libraries(pressure_model_check PRIVATE raketa_pressure_model)
add_test(NAME pressure_model_check COMMAND pressure_model_check)

add_executable(command_handler_check
    ../tests/command_handler_check.c
    ../firmware/commands/command_handler.c
)
target_include_directories(command_handler_check PRIVATE ../firmware/commands)
target_link_libraries(command_handler_check PRIVATE raketa_pressure_model raketa_protocol)
add_test(NAME command_handler_check COMMAND command_handler_check)
add_test(NAME device_client_transport_check COMMAND device_client_transport_check)
