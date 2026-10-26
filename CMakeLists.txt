cmake_minimum_required(VERSION 3.22)
project(AiGateway VERSION 0.2 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
include(CTest)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()
add_compile_definitions(NOMINMAX)

# 优先使用系统的开发包；本机 Anaconda 已包含可用的 libcurl 开发文件。
if(WIN32)
    set(local_curl_prefix "$ENV{USERPROFILE}/anaconda3/Library")
    if(NOT DEFINED CURL_INCLUDE_DIR AND EXISTS "${local_curl_prefix}/include/curl/curl.h")
        set(CURL_INCLUDE_DIR "${local_curl_prefix}/include" CACHE PATH "curl headers")
    endif()
    if(NOT DEFINED CURL_LIBRARY AND EXISTS "${local_curl_prefix}/lib/libcurl.lib")
        set(CURL_LIBRARY "${local_curl_prefix}/lib/libcurl.lib" CACHE FILEPATH "curl import library")
    endif()
endif()

add_subdirectory(shared)
add_subdirectory(ai-gateway)
add_library(integrated_core
    modules/router/model_router.cpp
    modules/router/public_router.cpp
    modules/providers/public_provider.cpp
    gateway/service.cpp)
target_include_directories(integrated_core PUBLIC shared gateway PRIVATE
    ai-gateway/third_party ai-gateway/third_party/cpp-httplib)
target_link_libraries(integrated_core PUBLIC question_analyzer gateway_providers)
add_executable(gateway gateway/main.cpp)
target_include_directories(gateway PRIVATE ai-gateway/third_party ai-gateway/third_party/cpp-httplib)
target_link_libraries(gateway PRIVATE integrated_core gateway_mock_http)
if(WIN32)
    target_link_libraries(gateway PRIVATE ws2_32 shell32)
endif()

file(MAKE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config")
if(NOT EXISTS "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config/models.json")
    configure_file(config/models.example.json "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config/models.json" COPYONLY)
endif()
configure_file(config/models.example.json "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config/models.default.json" COPYONLY)
if(WIN32)
    get_filename_component(curl_library_dir "${CURL_LIBRARY}" DIRECTORY)
    find_file(CURL_RUNTIME_DLL NAMES libcurl.dll libcurl-4.dll
        PATHS "${local_curl_prefix}/bin" "${curl_library_dir}/../bin")
    if(CURL_RUNTIME_DLL)
        get_filename_component(curl_runtime_dir "${CURL_RUNTIME_DLL}" DIRECTORY)
        get_filename_component(compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
        add_custom_command(TARGET gateway POST_BUILD
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/deploy_runtime.py"
                --dll "${CURL_RUNTIME_DLL}" --search "${curl_runtime_dir}" --search "${compiler_bin}"
                --out "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" --objdump "${CMAKE_OBJDUMP}"
            VERBATIM)
        foreach(runtime libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
            if(EXISTS "${compiler_bin}/${runtime}")
                add_custom_command(TARGET gateway POST_BUILD COMMAND "${CMAKE_COMMAND}" -E
                    copy_if_different "${compiler_bin}/${runtime}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" VERBATIM)
            endif()
        endforeach()
    endif()
endif()

option(BUILD_CLIENT "Build the Qt desktop client" ON)
if(BUILD_CLIENT)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network)
    qt_standard_project_setup()
    add_library(client_widgets mainwindow.cpp mainwindow.h)
    target_include_directories(client_widgets PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(client_widgets PUBLIC Qt6::Core Qt6::Widgets Qt6::Network)
    qt_add_executable(MyClient WIN32 main.cpp)
    target_link_libraries(MyClient PRIVATE client_widgets)
    add_dependencies(MyClient gateway)
    if(WIN32)
        get_target_property(qt_qmake Qt6::qmake IMPORTED_LOCATION)
        get_filename_component(qt_bin "${qt_qmake}" DIRECTORY)
        add_custom_command(TARGET MyClient POST_BUILD
            COMMAND "${qt_bin}/windeployqt.exe" --release --no-translations --compiler-runtime
                    "$<TARGET_FILE:MyClient>"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${qt_bin}/../plugins/platforms/qoffscreen.dll"
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/platforms/qoffscreen.dll"
            VERBATIM)
    endif()
endif()

if(BUILD_TESTING)
    add_test(NAME integrated_http COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/test_gateway.py"
        --gateway "$<TARGET_FILE:gateway>"
        --config "${CMAKE_SOURCE_DIR}/config/models.example.json"
        --work "${CMAKE_BINARY_DIR}/test-work")
    set_tests_properties(integrated_http PROPERTIES TIMEOUT 60)
    if(BUILD_CLIENT)
        find_package(Qt6 REQUIRED COMPONENTS Test)
        add_executable(client_tests tests/client_integration.cpp)
        target_link_libraries(client_tests PRIVATE client_widgets Qt6::Test)
        add_dependencies(client_tests MyClient)
        if(WIN32)
            add_custom_command(TARGET client_tests POST_BUILD COMMAND "${CMAKE_COMMAND}" -E
                copy_if_different "$<TARGET_FILE:Qt6::Test>" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" VERBATIM)
        endif()
        add_test(NAME desktop_end_to_end COMMAND client_tests -o -,txt)
        set_tests_properties(desktop_end_to_end PROPERTIES
            TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endif()
