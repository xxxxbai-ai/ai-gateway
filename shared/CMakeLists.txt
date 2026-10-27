cmake_minimum_required(VERSION 3.16)
project(gateway_contracts LANGUAGES CXX)

add_library(gateway_contracts INTERFACE)
target_compile_features(gateway_contracts INTERFACE cxx_std_17)
target_include_directories(gateway_contracts INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

include(CTest)
# 编译真正的 B 实现，证明公共类型可被实际调用方使用。
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../modules/analyzer"
                 "${CMAKE_CURRENT_BINARY_DIR}/analyzer")

add_executable(contract_example examples/contract_example.cpp)
target_link_libraries(contract_example PRIVATE gateway_contracts question_analyzer)
set(contract_targets contract_example)

if(BUILD_TESTING)
    add_executable(contracts_test tests/contracts_test.cpp)
    target_link_libraries(contracts_test PRIVATE gateway_contracts question_analyzer)
    list(APPEND contract_targets contracts_test)
    add_test(NAME public_contracts COMMAND contracts_test)
    add_test(NAME public_contract_example COMMAND contract_example)
endif()

foreach(target IN LISTS contract_targets)
    if(WIN32)
        target_link_libraries(${target} PRIVATE shell32)
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic
            -finput-charset=UTF-8 -fexec-charset=UTF-8)
    endif()
endforeach()
