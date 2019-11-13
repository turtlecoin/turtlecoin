cmake_minimum_required(VERSION 2.6)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "-- Argon2: Found ccache package... Activating...")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

project(Argon2 C)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

set(ARGON2_VERSION 1.0)
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(CheckCSourceCompiles)
find_package(Threads REQUIRED)

add_library(argon2-interface INTERFACE)
target_include_directories(argon2-interface INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

add_library(argon2-internal INTERFACE)
target_include_directories(argon2-internal INTERFACE lib lib/blake2)
target_link_libraries(argon2-internal INTERFACE argon2-interface)

set(ARGON2_SRC
    lib/argon2.c
    lib/core.c
    lib/encoding.c
    lib/genkat.c
    lib/impl-select.c
    lib/thread.c
    lib/blake2/blake2.c
)

message("-- Argon2: Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message("-- Argon2: Build Type: ${ARCH}")

if(NOT "${ARCH}" STREQUAL "default")
    include(OptimizeForArchitecture)
    OptimizeForArchitecture()

    if (MSVC)
        # Needed by MSVC, but not added by OptimizeForArchitexture()
        add_definitions(-D__SSE__)
    endif()
endif()

foreach(flag ${Vc_ARCHITECTURE_FLAGS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
endforeach()

message(STATUS "-- Argon2: Build Flags: ${CMAKE_CXX_FLAGS}")

if(USE_AVX512F OR USE_AVX2 OR USE_SSE3 OR USE_SSE2 OR USE_XOP)
    list(APPEND ARGON2_SRC
        arch/x86_64/lib/argon2-sse2.c
        arch/x86_64/lib/argon2-sse3.c
        arch/x86_64/lib/argon2-ssse3.c
        arch/x86_64/lib/argon2-xop.c
        arch/x86_64/lib/argon2-avx2.c
        arch/x86_64/lib/argon2-avx512f.c
        arch/x86_64/lib/argon2-arch.c
    )
else()
    list(APPEND ARGON2_SRC
        arch/generic/lib/argon2-arch.c
    )
endif()

add_library(argon2 STATIC ${ARGON2_SRC})

target_compile_definitions(argon2
    PUBLIC "A2_VISCTL"
)

target_link_libraries(argon2
    PUBLIC argon2-interface ${CMAKE_THREAD_LIBS_INIT}
    PRIVATE argon2-internal
)

set_property(TARGET argon2 PROPERTY C_STANDARD 90)
set_property(TARGET argon2 PROPERTY VERSION ${Upstream_VERSION})
set_property(TARGET argon2 PROPERTY SOVERSION 1)
set_property(TARGET argon2 PROPERTY INTERFACE_ARGON2_MAJOR_VERSION 1)
set_property(TARGET argon2 APPEND PROPERTY
  COMPATIBLE_INTERFACE_STRING ARGON2_MAJOR_VERSION
)
