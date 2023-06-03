cmake_minimum_required(VERSION 3.25)
project(riscv C)

set(CMAKE_C_STANDARD 11)

add_executable(riscv riscv_sim.c)
