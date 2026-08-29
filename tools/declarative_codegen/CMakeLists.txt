cmake_minimum_required(VERSION 3.20)
project(huxerui_declarative_codegen LANGUAGES CXX)

add_executable(huxerui_declarative_codegen
        main.cpp
        parser.cpp
        generator.cpp
)

target_compile_features(huxerui_declarative_codegen PRIVATE cxx_std_20)
target_include_directories(huxerui_declarative_codegen PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_options(huxerui_declarative_codegen PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/W4>"
        "$<$<CXX_COMPILER_ID:MSVC>:/permissive->"
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
        "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>"
        "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra>"
        "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wpedantic>"
)

set_target_properties(huxerui_declarative_codegen PROPERTIES
        CXX_EXTENSIONS OFF
)
