cmake_minimum_required(VERSION 3.10)
project(integrals_ulfm)

set(CMAKE_CXX_STANDARD 20)

# Add MPI Package to Project
find_package(MPI REQUIRED)

add_executable(integrals_ulfm main.cpp functions/exp.cpp functions/heaviside_step.cpp functions/arcsin.cpp messaging.cpp messaging.h)
# Add libraries for code completion and compiling
target_link_libraries(integrals_ulfm PUBLIC MPI::MPI_CXX)