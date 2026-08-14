file(GLOB_RECURSE TEST_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_executable(autotile_tests ${TEST_SOURCES})

target_link_libraries(autotile_tests PRIVATE autotile)

target_include_directories(autotile_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
)

add_project_warnings(autotile_tests)
add_sanitizer_flags(autotile_tests)

add_test(NAME autotile_unit_tests COMMAND autotile_tests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME autotile_corpus_parity_quick
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../corpus/verify.py --exe $<TARGET_FILE:autotile_mixer> --quick
    )
    add_test(NAME autotile_corpus_parity_full
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../corpus/verify.py --exe $<TARGET_FILE:autotile_mixer>
    )
    set_tests_properties(autotile_corpus_parity_full PROPERTIES LABELS "slow" TIMEOUT 1800)
endif()
