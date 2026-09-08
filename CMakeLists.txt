cmake_minimum_required(VERSION 4.1)
project(AEON_KRON_1 C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)

add_executable(AEON_KRON_1 main.c)

cmake_minimum_required(VERSION 3.20)
project(kron_sim C)

# .mem-Dateien neben die Binary kopieren
file(COPY ${CMAKE_SOURCE_DIR}/programs/
        DESTINATION ${CMAKE_BINARY_DIR})
