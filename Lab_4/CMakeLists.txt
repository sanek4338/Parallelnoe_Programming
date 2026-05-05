cmake_minimum_required(VERSION 3.18)

project(MatrixMul_Lab4_CUDA LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

add_executable(lab4_matrix_mul_cuda
    matrix_mul_cuda.cu
)

set_target_properties(lab4_matrix_mul_cuda PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_ARCHITECTURES 75
)