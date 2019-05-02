CMAKE_MINIMUM_REQUIRED(VERSION 3.0.0)

PROJECT(lz4)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache package... Activating...")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

SET(HEADERS_LZ4
  lz4.h
  lz4hc.h
  lz4frame.h
)

SET(SOURCES_LZ4
  lz4.c
  lz4hc.c
  lz4frame.c
  xxhash.c
)

SET(LZ4_STATIC_LIB lz4)
add_library(${LZ4_STATIC_LIB} STATIC ${HEADERS_LZ4} ${SOURCES_LZ4})