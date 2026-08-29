if (HUXERUI_LIBRARY_GRAPH_ONLY OR CMAKE_CROSSCOMPILING)
    return()
endif ()

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
target_link_libraries(huxerui_preview PRIVATE HuxerUI::huxerui)
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
