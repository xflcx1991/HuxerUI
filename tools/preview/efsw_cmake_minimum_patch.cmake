set(efsw_cmake_file "${SOURCE_DIR}/CMakeLists.txt")

file(READ "${efsw_cmake_file}" efsw_cmake_text)

if (NOT efsw_cmake_text MATCHES "cmake_minimum_required\\(VERSION 3\\.27 FATAL_ERROR\\)")
    return()
endif ()

string(REPLACE
    "cmake_minimum_required(VERSION 3.27 FATAL_ERROR)"
    "cmake_minimum_required(VERSION 3.20 FATAL_ERROR)"
    efsw_cmake_text
    "${efsw_cmake_text}"
)

file(WRITE "${efsw_cmake_file}" "${efsw_cmake_text}")
