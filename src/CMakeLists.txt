# Static core library: autotile
file(GLOB_RECURSE AUTOTILE_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

# Vendored miniz C sources
set(MINIZ_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/miniz/miniz.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/miniz/miniz_tdef.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/miniz/miniz_tinfl.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/miniz/miniz_zip.c"
)

add_library(autotile STATIC
    ${AUTOTILE_SOURCES}
    ${MINIZ_SOURCES}
)

target_include_directories(autotile PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
)

# Silence trigraph warnings specifically for pattern_data.cpp containing '??' sequences
if(NOT MSVC)
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/pattern/pattern_data.cpp" PROPERTIES COMPILE_FLAGS "-Wno-trigraphs")
endif()

# Silence warnings on vendored miniz C sources
if(MSVC)
    set_source_files_properties(${MINIZ_SOURCES} PROPERTIES COMPILE_FLAGS "/w")
else()
    set_source_files_properties(${MINIZ_SOURCES} PROPERTIES COMPILE_FLAGS "-w")
endif()

add_project_warnings(autotile)
add_sanitizer_flags(autotile)
