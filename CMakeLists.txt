cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(TPDPHook)

if(NOT MSVC)
    message(FATAL_ERROR "Must be compiled with Visual Studio.")
endif()

# target must be 32-bit
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Target must be 32-bit, re-run CMake and select a Win32 target.")
endif()

add_compile_options("/MP") # parallel builds

add_compile_options("/arch:SSE2") # mandatory to avoid x87 fuckery

add_compile_options("$<$<CONFIG:Release>:/MT>") # static link CRT for release builds

# LibTPDP
option(STATIC_RUNTIME "Static link the C runtime" ON)
option(BUILD_CSHARP "Build csharp GUI front-ends" OFF)
option(BUILD_BINEDIT "Build binedit" OFF)
option(BUILD_DIFFGEN "Build diffgen" OFF)
add_subdirectory("deps/TPDP-Dev-Tools")

# lazy
file(GLOB_RECURSE SRC_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
file(GLOB_RECURSE SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(TPDPHook SHARED ${SRC_HEADERS} ${SRC_FILES} "${CMAKE_CURRENT_SOURCE_DIR}/src/exports.def")
set_target_properties(TPDPHook PROPERTIES OUTPUT_NAME "dxgi")
target_link_libraries(TPDPHook PRIVATE libtpdp "Ws2_32.lib")
target_compile_options(TPDPHook PRIVATE "/W4") # warning level
target_compile_definitions(TPDPHook PRIVATE "HOOK_BUILD")
