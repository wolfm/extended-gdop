project(extended-gdop)

cmake_minimum_required(VERSION 3.4.3)
find_package(LLVM REQUIRED CONFIG)                        # This will find the shared LLVM build.
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")        # You don't need to change ${LLVM_CMAKE_DIR} since it is already defined.
include(AddLLVM)
add_definitions(${LLVM_DEFINITIONS})                      # You don't need to change ${LLVM_DEFINITIONS} since it is already defined.
include_directories(${LLVM_INCLUDE_DIRS} utils)                 # You don't need to change ${LLVM_INCLUDE_DIRS} since it is already defined.

# Utils shared library
add_subdirectory(utils)

# LLVM Passes
add_subdirectory(boguscf-pass)
add_subdirectory(dopseq-pass)