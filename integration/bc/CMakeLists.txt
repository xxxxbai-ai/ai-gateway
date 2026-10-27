cmake_minimum_required(VERSION 3.20)
project(GatewayBCIntegration LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
include(CTest)
set(team_root "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# 实际 B、C、公共类型和 A；D 的测试替身只链接到明确命名的测试服务器。
add_subdirectory("${team_root}/shared" "${CMAKE_BINARY_DIR}/shared")
add_subdirectory("${team_root}/modules/router" "${CMAKE_BINARY_DIR}/router")
if(BUILD_TESTING)
    add_library(bc_provider_test_double STATIC provider_test_double.cpp)
    target_include_directories(bc_provider_test_double PRIVATE "${team_root}")
    set(GATEWAY_BC_INTEGRATION_TESTS ON)
endif()
add_subdirectory("${team_root}/gateway" "${CMAKE_BINARY_DIR}/gateway")

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME bc_http_integration COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/http_tests.py"
        --gateway "$<TARGET_FILE:gateway_bc_test_host>"
        --analyzer "$<TARGET_FILE:analyzer_evaluation>"
        --fixture "${team_root}/modules/analyzer/fixtures/v03_frozen_30.tsv"
        --config "${team_root}/config/models.example.json"
        --work "${CMAKE_BINARY_DIR}/test-work/bc")
    set_tests_properties(bc_http_integration PROPERTIES TIMEOUT 45)
endif()
