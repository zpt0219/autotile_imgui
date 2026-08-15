include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)

# ImGui library target
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(imgui_lib STATIC ${IMGUI_SOURCES})
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${glfw_SOURCE_DIR}/include
)
target_link_libraries(imgui_lib PUBLIC glfw)
if(NOT MSVC)
    target_compile_options(imgui_lib PRIVATE -w)
else()
    target_compile_options(imgui_lib PRIVATE /w)
endif()

file(GLOB_RECURSE DESKTOP_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

set(TINYFD_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/tinyfd/tinyfiledialogs.c"
)

add_executable(autotile_mixer
    ${DESKTOP_SOURCES}
    ${TINYFD_SOURCES}
)

# Silence warnings on vendored tinyfd C source
if(MSVC)
    set_source_files_properties(${TINYFD_SOURCES} PROPERTIES COMPILE_FLAGS "/w")
else()
    set_source_files_properties(${TINYFD_SOURCES} PROPERTIES COMPILE_FLAGS "-w")
endif()

target_include_directories(autotile_mixer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/tinyfd
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${glfw_SOURCE_DIR}/include
)

target_link_libraries(autotile_mixer PRIVATE
    autotile
    imgui_lib
    glfw
)

add_project_warnings(autotile_mixer)

if(WIN32)
    target_link_libraries(autotile_mixer PRIVATE opengl32)
endif()

if(ENABLE_ASAN)
    enable_sanitizers(autotile_mixer)
endif()

# Copy assets folder to build directory next to executable
add_custom_command(TARGET autotile_mixer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:autotile_mixer>/assets
)
