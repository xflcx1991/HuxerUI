if (HUXERUI_LIBRARY_GRAPH_ONLY OR CMAKE_CROSSCOMPILING)
    return()
endif ()

include(FetchContent)

set(BUILD_SHARED_LIBS OFF)
set(BUILD_STATIC_LIBS ON)
set(BUILD_TEST_APP OFF)
set(EFSW_INSTALL OFF)

FetchContent_Declare(efsw
    GIT_REPOSITORY https://github.com/SpartanJ/efsw.git
    GIT_TAG 1.7.2
    GIT_SHALLOW TRUE
    PATCH_COMMAND
        "${CMAKE_COMMAND}"
        -DSOURCE_DIR=<SOURCE_DIR>
        -P "${CMAKE_CURRENT_LIST_DIR}/efsw_cmake_minimum_patch.cmake"
)

FetchContent_MakeAvailable(efsw)

add_executable(huxerui_preview
        main.cpp
        evaluator.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../declarative_codegen/parser.cpp"
)

target_compile_features(huxerui_preview PRIVATE cxx_std_20)
target_include_directories(huxerui_preview PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../declarative_codegen"
)
target_link_libraries(huxerui_preview PRIVATE
        HuxerUI::huxerui
        efsw-static
)
target_compile_options(huxerui_preview PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/W4>"
        "$<$<CXX_COMPILER_ID:MSVC>:/permissive->"
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
        "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>"
        "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra>"
        "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wpedantic>"
)

set_target_properties(huxerui_preview PROPERTIES
        CXX_EXTENSIONS OFF
)
