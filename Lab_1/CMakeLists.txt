cmake_minimum_required(VERSION 3.15)
project(MatrixMul_Lab1 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_executable(lab1_matrix_mul
    lab_1.cpp
)

target_compile_definitions(lab1_matrix_mul PRIVATE
    PROJECT_DIR="${CMAKE_SOURCE_DIR}"
)


if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()


if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" OR
   CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
   CMAKE_CXX_COMPILER_ID STREQUAL "GNU")

  target_compile_options(lab1_matrix_mul PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )


elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
  endif()

  target_compile_options(lab1_matrix_mul PRIVATE
      /W4
      /utf-8
  )

endif()