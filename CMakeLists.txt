cmake_minimum_required(VERSION 4.1)
project(AEON_KRON_1 C)

set(CMAKE_C_STANDARD 11)

add_subdirectory(../isa isa)

add_executable(AEON_KRON_1 main.c
        main.h)

target_link_libraries(AEON_KRON_1 isa)

# .mem-Dateien neben die Binary kopieren
file(COPY ${CMAKE_SOURCE_DIR}/programs/
        DESTINATION ${CMAKE_BINARY_DIR})
