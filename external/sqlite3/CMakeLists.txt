cmake_minimum_required (VERSION 2.6)

project(sqlite3)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  message(STATUS "Found ccache package... Activating...")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

add_definitions("-DSQLITE_OMIT_LOAD_EXTENSION")

if(NOT MSVC)
  add_definitions("-fno-fast-math")
endif()

add_library(sqlite3 STATIC sqlite3.c)
