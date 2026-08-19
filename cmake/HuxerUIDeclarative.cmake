include_guard(GLOBAL)

function(huxerui_add_declarative target_name)
    if (NOT TARGET ${target_name})
        message(FATAL_ERROR
                "huxerui_add_declarative() target does not exist: ${target_name}"
        )
    endif ()

    set(options)
    set(one_value_args NAMESPACE)
    set(multi_value_args SOURCES)
    cmake_parse_arguments(HUXERUI_DECLARATIVE
            "${options}"
            "${one_value_args}"
            "${multi_value_args}"
            ${ARGN}
    )
    if (HUXERUI_DECLARATIVE_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
                "huxerui_add_declarative() received unknown arguments: "
                "${HUXERUI_DECLARATIVE_UNPARSED_ARGUMENTS}"
        )
    endif ()
    if (NOT HUXERUI_DECLARATIVE_SOURCES)
        message(FATAL_ERROR
                "huxerui_add_declarative() requires SOURCES"
        )
    endif ()

    if (HUXERUI_DECLARATIVE_NAMESPACE)
        set(HUXERUI_DECLARATIVE_NAMESPACE_VALUE "${HUXERUI_DECLARATIVE_NAMESPACE}")
    else ()
        set(HUXERUI_DECLARATIVE_NAMESPACE_VALUE "huxerui_generated")
    endif ()

    if (HUXERUI_DECLARATIVE_CODEGEN)
        set(HUXERUI_DECLARATIVE_COMMAND "${HUXERUI_DECLARATIVE_CODEGEN}")
    elseif (TARGET huxerui_declarative_codegen)
        if (CMAKE_CROSSCOMPILING)
            message(FATAL_ERROR
                    "huxerui_add_declarative() requires HUXERUI_DECLARATIVE_CODEGEN "
                    "when cross-compiling"
            )
        endif ()
        set(HUXERUI_DECLARATIVE_COMMAND "$<TARGET_FILE:huxerui_declarative_codegen>")
    else ()
        message(FATAL_ERROR
                "huxerui_add_declarative() requires HUXERUI_DECLARATIVE_CODEGEN "
                "or the huxerui_declarative_codegen target"
        )
    endif ()

    get_target_property(HUXERUI_DECLARATIVE_TARGET_TYPE ${target_name} TYPE)
    if (HUXERUI_DECLARATIVE_TARGET_TYPE STREQUAL "INTERFACE_LIBRARY"
            OR HUXERUI_DECLARATIVE_TARGET_TYPE STREQUAL "UTILITY")
        message(FATAL_ERROR
                "huxerui_add_declarative() requires a compilable target: ${target_name}"
        )
    endif ()

    set(HUXERUI_DECLARATIVE_OUTPUT_ROOT
            "${CMAKE_CURRENT_BINARY_DIR}/huxerui-declarative/${target_name}"
    )

    foreach (HUXERUI_DECLARATIVE_SOURCE IN LISTS HUXERUI_DECLARATIVE_SOURCES)
        get_filename_component(HUXERUI_DECLARATIVE_INPUT
                "${HUXERUI_DECLARATIVE_SOURCE}"
                ABSOLUTE
                BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
        )
        if (NOT EXISTS "${HUXERUI_DECLARATIVE_INPUT}")
            message(FATAL_ERROR
                    "huxerui_add_declarative() source does not exist: ${HUXERUI_DECLARATIVE_INPUT}"
            )
        endif ()
        get_filename_component(HUXERUI_DECLARATIVE_EXTENSION
                "${HUXERUI_DECLARATIVE_INPUT}"
                EXT
        )
        if (NOT HUXERUI_DECLARATIVE_EXTENSION STREQUAL ".ui")
            message(FATAL_ERROR
                    "huxerui_add_declarative() sources must use the .ui extension: "
                    "${HUXERUI_DECLARATIVE_INPUT}"
            )
        endif ()

        get_filename_component(HUXERUI_DECLARATIVE_STEM
                "${HUXERUI_DECLARATIVE_INPUT}"
                NAME_WE
        )
        string(SHA256 HUXERUI_DECLARATIVE_SOURCE_HASH "${HUXERUI_DECLARATIVE_INPUT}")
        string(SUBSTRING "${HUXERUI_DECLARATIVE_SOURCE_HASH}" 0 16 HUXERUI_DECLARATIVE_SOURCE_HASH)
        set(HUXERUI_DECLARATIVE_OUTPUT_DIRECTORY
                "${HUXERUI_DECLARATIVE_OUTPUT_ROOT}/${HUXERUI_DECLARATIVE_SOURCE_HASH}"
        )
        set(HUXERUI_DECLARATIVE_OUTPUT_HEADER
                "${HUXERUI_DECLARATIVE_OUTPUT_DIRECTORY}/${HUXERUI_DECLARATIVE_STEM}.generated.h"
        )
        set(HUXERUI_DECLARATIVE_OUTPUT_SOURCE
                "${HUXERUI_DECLARATIVE_OUTPUT_DIRECTORY}/${HUXERUI_DECLARATIVE_STEM}.generated.cpp"
        )
        add_custom_command(
                OUTPUT
                        "${HUXERUI_DECLARATIVE_OUTPUT_HEADER}"
                        "${HUXERUI_DECLARATIVE_OUTPUT_SOURCE}"
                COMMAND ${CMAKE_COMMAND} -E make_directory
                        "${HUXERUI_DECLARATIVE_OUTPUT_DIRECTORY}"
                COMMAND "${HUXERUI_DECLARATIVE_COMMAND}"
                        --input "${HUXERUI_DECLARATIVE_INPUT}"
                        --output-cpp "${HUXERUI_DECLARATIVE_OUTPUT_SOURCE}"
                        --output-header "${HUXERUI_DECLARATIVE_OUTPUT_HEADER}"
                        --namespace "${HUXERUI_DECLARATIVE_NAMESPACE_VALUE}"
                DEPENDS
                        "${HUXERUI_DECLARATIVE_INPUT}"
                        "${HUXERUI_DECLARATIVE_COMMAND}"
                COMMENT "Generating HuxerUI declarative source ${HUXERUI_DECLARATIVE_STEM}.ui"
                VERBATIM
        )
        set_source_files_properties(
                "${HUXERUI_DECLARATIVE_OUTPUT_HEADER}"
                "${HUXERUI_DECLARATIVE_OUTPUT_SOURCE}"
                PROPERTIES GENERATED TRUE
        )
        target_sources(${target_name} PRIVATE
                "${HUXERUI_DECLARATIVE_OUTPUT_HEADER}"
                "${HUXERUI_DECLARATIVE_OUTPUT_SOURCE}"
        )
        target_include_directories(${target_name} PRIVATE
                "${HUXERUI_DECLARATIVE_OUTPUT_DIRECTORY}"
        )
    endforeach ()
endfunction()
