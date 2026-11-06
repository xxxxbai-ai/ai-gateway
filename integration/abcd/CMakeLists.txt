cmake_minimum_required(VERSION 3.20)
project(GatewayABCDIntegration LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
include(CTest)
get_filename_component(team_root "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

# 各成员的实际模块；不链接 A 或 BC 的测试替身。
add_subdirectory("${team_root}/shared" "${CMAKE_BINARY_DIR}/shared")
add_subdirectory("${team_root}/modules/router" "${CMAKE_BINARY_DIR}/router")
add_subdirectory("${team_root}/ai-gateway" "${CMAKE_BINARY_DIR}/providers")
set(GATEWAY_MODULE_LIBRARIES question_analyzer model_router gateway_public_provider
    CACHE STRING "Actual B/C/D module targets")
add_subdirectory("${team_root}/gateway" "${CMAKE_BINARY_DIR}/gateway")

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME abcd_http_integration COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/http_tests.py"
        --gateway "$<TARGET_FILE:gateway>"
        --provider "$<TARGET_FILE:mock_provider>"
        --config "${team_root}/config/models.example.json"
        --work "${CMAKE_BINARY_DIR}/test-work/abcd")
    set_tests_properties(abcd_http_integration PROPERTIES TIMEOUT 45)
endif()
