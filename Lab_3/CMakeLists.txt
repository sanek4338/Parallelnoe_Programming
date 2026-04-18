cmake_minimum_required(VERSION 3.15)
project(MatrixMul_Lab2_MPI CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(MPI REQUIRED)

add_executable(lab3_matrix_mul_mpi
    lab_3.cpp
)

target_compile_definitions(lab3_matrix_mul_mpi PRIVATE
    PROJECT_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(lab3_matrix_mul_mpi PRIVATE MPI::MPI_CXX)

if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" OR
   CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
   CMAKE_CXX_COMPILER_ID STREQUAL "GNU")

    target_compile_options(lab3_matrix_mul_mpi PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    endif()

    target_compile_options(lab3_matrix_mul_mpi PRIVATE
        /W4
        /utf-8
    )

endif()

set_target_properties(lab3_matrix_mul_mpi PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

foreach(DATA_FILE matrixA.txt matrixB.txt)
    if(EXISTS "${CMAKE_SOURCE_DIR}/${DATA_FILE}")
        add_custom_command(TARGET lab3_matrix_mul_mpi POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/${DATA_FILE}"
                "${CMAKE_CURRENT_BINARY_DIR}/${DATA_FILE}"
        )
    endif()
endforeach()

message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "MPI found: ${MPI_CXX_FOUND}")