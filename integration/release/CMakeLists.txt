cmake_minimum_required(VERSION 3.21)
project(AIGatewayDelivery VERSION 1.0.0 LANGUAGES CXX)
include(CTest)
option(BUILD_CLIENT "Build the delivered Qt client" ON)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../abcd" "${CMAKE_BINARY_DIR}/abcd")
if(BUILD_CLIENT)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../client" "${CMAKE_BINARY_DIR}/client")
endif()
if(BUILD_TESTING AND WIN32)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME a_startup_persistence COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/startup_tests.py" --gateway "$<TARGET_FILE:gateway>"
        --script "${CMAKE_CURRENT_SOURCE_DIR}/../abcd/start-multi-model-gateway.ps1"
        --config "${CMAKE_CURRENT_SOURCE_DIR}/../../config/models.providers.example.json"
        --work "${CMAKE_BINARY_DIR}/test-work/startup")
    set_tests_properties(a_startup_persistence PROPERTIES TIMEOUT 60)
endif()
