cmake_minimum_required(VERSION 3.16)
project(cvm_plus_plus VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Compiler flags ──────────────────────────
if (MSVC)
    add_compile_options(/W4 /O2)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O2)
endif()

# ── Source files ────────────────────────────
set(CVM_SOURCES
    src/lexer.cpp
    src/parser.cpp
    src/compiler.cpp
    src/vm.cpp
)

# ── Library (for testing) ───────────────────
add_library(cvm_lib STATIC ${CVM_SOURCES})
target_include_directories(cvm_lib PUBLIC include)

# ── Main executable ─────────────────────────
add_executable(cvm src/main.cpp)
target_link_libraries(cvm PRIVATE cvm_lib)
target_include_directories(cvm PRIVATE include)

# ── Test runner ─────────────────────────────
add_executable(cvm_test tests/test_runner.cpp)
target_link_libraries(cvm_test PRIVATE cvm_lib)
target_include_directories(cvm_test PRIVATE include)

# ── Install ─────────────────────────────────
install(TARGETS cvm DESTINATION bin)

# ── Output binary info ──────────────────────
message(STATUS "CVM++ build system configured:")
message(STATUS "  C++ Standard : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build type   : ${CMAKE_BUILD_TYPE}")
