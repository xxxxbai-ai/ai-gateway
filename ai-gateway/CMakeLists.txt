cmake_minimum_required(VERSION 3.20)
project(ai_gateway LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# D 模块：libcurl 使用本机开发包；两个头文件依赖固定版本随源码保存。
# 构建：cmake -S . -B build-d -G Ninja && cmake --build build-d
# 验证：ctest --test-dir build-d --output-on-failure
# 运行：build-d/mock_provider，然后在另一终端运行 build-d/provider_cli --stream
# 网关联调：build-d/gateway_demo --http --stream
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
add_library(gateway_vendor INTERFACE)
target_include_directories(gateway_vendor SYSTEM INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cpp-httplib)

add_library(gateway_providers
    src/providers/provider_adapter.cpp
    src/providers/mock_provider.cpp
    src/providers/sse_decoder.cpp
    src/providers/credential_resolver.cpp
    src/providers/compatible_chat_adapter.cpp)
target_include_directories(gateway_providers PUBLIC src)
target_link_libraries(gateway_providers PUBLIC CURL::libcurl PRIVATE gateway_vendor)

add_library(gateway_core
    src/analyzer/question_analyzer.cpp
    src/router/router.cpp
    src/storage/model_repository.cpp
    src/server/gateway_service.cpp
)
target_include_directories(gateway_core PUBLIC src)
target_link_libraries(gateway_core PUBLIC gateway_providers PRIVATE gateway_vendor)
add_executable(gateway_demo src/main.cpp)
target_link_libraries(gateway_demo PRIVATE gateway_core)

add_library(gateway_mock_http tools/mock_provider/mock_server.cpp)
target_include_directories(gateway_mock_http PUBLIC tools/mock_provider)
target_link_libraries(gateway_mock_http PRIVATE gateway_vendor Threads::Threads)
if(WIN32)
    target_link_libraries(gateway_mock_http PRIVATE ws2_32)
endif()
add_executable(mock_provider tools/mock_provider/main.cpp)
target_link_libraries(mock_provider PRIVATE gateway_mock_http)
add_executable(provider_cli tools/provider_cli/main.cpp)
target_link_libraries(provider_cli PRIVATE gateway_providers gateway_vendor Threads::Threads)
if(WIN32)
    target_link_libraries(provider_cli PRIVATE shell32)
    target_link_libraries(gateway_demo PRIVATE shell32)
endif()

include(CTest)
set(gateway_targets gateway_core gateway_providers gateway_demo gateway_mock_http mock_provider provider_cli)
if(BUILD_TESTING)
    add_executable(provider_tests tests/providers/provider_tests.cpp)
    target_link_libraries(provider_tests PRIVATE gateway_core gateway_mock_http gateway_vendor Threads::Threads)
    list(APPEND gateway_targets provider_tests)
    foreach(group sse calls stream faults cancel gateway_route gateway_fallback)
        add_test(NAME provider_${group} COMMAND provider_tests ${group})
        set_tests_properties(provider_${group} PROPERTIES TIMEOUT 20)
    endforeach()
    add_test(NAME gateway_existing_demo COMMAND gateway_demo)
endif()

foreach(target IN LISTS gateway_targets)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
