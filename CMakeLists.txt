cmake_minimum_required(VERSION 3.20)
project(autotile_imgui VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default options
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_DESKTOP "Build desktop application" ON)
option(ENABLE_ASAN "Enable address & undefined sanitizers" OFF)

include(cmake/Sanitizers.cmake)

function(add_project_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# Global include directories for third party
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

add_subdirectory(src)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_DESKTOP)
    add_subdirectory(desktop)
endif()
