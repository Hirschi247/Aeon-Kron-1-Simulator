cmake_minimum_required(VERSION 3.16)
project(KRON-1-assembler C)

set(CMAKE_C_STANDARD 11)

add_subdirectory(../KRON-1-isa isa)

add_executable(KRON-1-assembler main.c
        main.h
        simcore/simcore.c
        simcore/simcore.h)

target_link_libraries(KRON-1-assembler isa)

# .mem-Dateien neben die Binary kopieren
file(COPY ${CMAKE_SOURCE_DIR}/programs/
        DESTINATION ${CMAKE_BINARY_DIR})
