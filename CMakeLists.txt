# We need 3.8 for set(CMAKE_CXX_STANDARD 17)
cmake_minimum_required(VERSION 3.8)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  message(STATUS "Found ccache package... Activating...")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

project(TurtleCoin)

# Required for finding Threads on ARM
enable_language(C)
enable_language(CXX)

# Assert our compiler is good
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC 7.0 or higher
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7)
        message(FATAL_ERROR "GCC/G++ 7.0 or greater is required to compile. Please check the README.md for detailed compilation instructions.")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Clang 6.0 or higher
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 6)
        message(FATAL_ERROR "Clang 6.0 or greater is required to compile. Please check the README.md for detailed compilation instructions.")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # Visual Studio 15 2017 or higher
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19.10)
        message(FATAL_ERROR "MSVC 19.10 or greater is required to compile (Latest Visual Studio 15 2017 should suffice). Please check the README.md for detailed compilation instructions.")
    endif()
else()
    message(WARNING "You are using an unsupported compiler. The compilation is very likely to fail. God speed.")
endif()

find_package(Threads)

set(VERSION "0.1")

## This section describes our general CMake setup options
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CONFIGURATION_TYPES Debug RelWithDebInfo Release CACHE TYPE INTERNAL)
set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: Debug, Release, RelWithDebInfo")
set(CMAKE_SKIP_INSTALL_RULES OFF FORCE)
set(CMAKE_SKIP_PACKAGE_ALL_DEPENDENCY ON FORCE)
set(CMAKE_SUPPRESS_REGENERATION ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Enable c++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

## This section is specifically for RocksDB build options that we've disabled for maximum portability
set(ENABLE_AVX OFF CACHE STRING "Enable RocksDB AVX/AVX2? Defaults to OFF")
set(ENABLE_LEAF_FRAME OFF CACHE STRING "Enable RocksDB OMIT_LEAF_FRAME_POINTER detection? Defaults to OFF")
set(ENABLE_SSE42 OFF CACHE STRING "Enable RocksDB SSE4.2 support detection? Defaults to OFF")
set(ENABLE_THREAD_LOCAL OFF CACHE STRING "Enable RocksDB THREAD_LOCAL support detection? Defaults to OFF")
set(ENABLE_SYNC_FILE_RANGE_WRITE OFF CACHE STRING "Enable RocksDB SYNC_FILE_RANGE_WRITE support detection? Defaults to OFF")
set(ENABLE_PTHREAD_MUTEX_ADAPTIVE_NP OFF CACHE STRING "Enable RocksDB PTHREAD_MUTEX_ADAPTIVE_NP support detection? Defaults to OFF")
set(ENABLE_MALLOC_USABLE_SIZE OFF CACHE STRING "Enable RocksDB MALLOC_USABLE_SIZE support detection? Defaults to OFF")
set(ENABLE_SCHED_GETCPU OFF CACHE STRING "Enable RocksDB SCHED_GETCPU support detection? Defaults to OFF")
## This section is for settings found in the slow-hash routine(s) that may benefit some systems (mostly ARM)
set(FORCE_USE_HEAP ON CACHE BOOL "Force the use of heap memory allocation")
set(NO_AES OFF CACHE BOOL "Turn off Hardware AES instructions?")
set(NO_OPTIMIZED_MULTIPLY_ON_ARM OFF CACHE BOOL "Turn off Optimized Multiplication on ARM?")

if(FORCE_USE_HEAP)
  add_definitions(-DFORCE_USE_HEAP)
  message(STATUS "FORCE_USE_HEAP: ENABLED")
else()
  message(STATUS "FORCE_USE_HEAP: DISABLED")
endif()

if(NO_AES)
  add_definitions(-DNO_AES)
  message(STATUS "HW AES: DISABLED")
else()
  message(STATUS "HW AES: ENABLED")
endif()

if(NO_OPTIMIZED_MULTIPLY_ON_ARM)
  add_definitions(-DNO_OPTIMIZED_MULTIPLY_ON_ARM)
  message(STATUS "OPTIMIZED_ARM_MULTIPLICATION: DISABLED")
else()
  message(STATUS "OPTIMIZED_ARM_MULTIPLICATION: ENABLED")
endif()

# We need to set the label and import it into CMake if it exists
set(LABEL "")
if(DEFINED ENV{LABEL})
  set(LABEL $ENV{LABEL})
  message(STATUS "Found LABEL: ${LABEL}")
endif()

# We do not support 32-bit builds. When and if we do, this can be removed.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Targeting a 32-bit architecture is not supported.")
endif()

## We only build static binaries -- this is left here for our dependencies
set(STATIC ON CACHE BOOL FORCE "Link libraries statically? Forced to ON")

## This section helps us tag our builds with the git commit information
set(COMMIT_ID_IN_VERSION ON CACHE BOOL "Include commit ID in version")

# So we can use std::string with rapidjson
add_definitions(-DRAPIDJSON_HAS_STDSTRING)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/version")

if (NOT COMMIT_ID_IN_VERSION)
  set(VERSION "${VERSION}-unknown")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/config/version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/version/version.h")
  add_custom_target(version ALL)
elseif(DEFINED COMMIT)
  string(REPLACE "." "\\." VERSION_RE "${VERSION}")
  if(NOT REFS MATCHES "(\\(|, )tag: v${VERSION_RE}(\\)|, )")
    set(VERSION "${VERSION}-g${COMMIT}")
  endif()
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/config/version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/version/version.h")
  add_custom_target(version ALL)
else()
  find_package(Git QUIET)
  if(Git_FOUND OR GIT_FOUND)
    message(STATUS "Found Git: ${GIT_EXECUTABLE}")
    add_custom_target(version ALL "${CMAKE_COMMAND}" "-D" "VERSION=${VERSION}" "-D" "GIT=${GIT_EXECUTABLE}" "-D" "TO=${CMAKE_CURRENT_BINARY_DIR}/version/version.h" "-P" "${CMAKE_CURRENT_SOURCE_DIR}/src/version.cmake" WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  else()
    message(STATUS "WARNING: Git was not found!")
    set(VERSION "${VERSION}-unknown")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/config/version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/version/version.h")
    add_custom_target(version ALL)
  endif()
endif()
include_directories(include "${CMAKE_CURRENT_BINARY_DIR}/version" src external)

## Platform specific code base information is applied here
if(MSVC)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/Platform/Windows)
  add_definitions("/bigobj /MP /W3 /GS- /D_CRT_SECURE_NO_WARNINGS /wd4996 /wd4345 /D_WIN32_WINNT=0x0600 /DWIN32_LEAN_AND_MEAN /DGTEST_HAS_TR1_TUPLE=0 /D_VARIADIC_MAX=8 /D__SSE4_1__")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:10485760")
  foreach(VAR CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELEASE)
    string(REPLACE "/MD" "/MT" ${VAR} "${${VAR}}")
  endforeach()
  include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/msc)
elseif(APPLE)
  include_directories(SYSTEM /usr/include/malloc)
  enable_language(ASM)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/Platform/OSX)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/Platform/Posix)

else()
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/Platform/Linux)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/Platform/Posix)
endif()

if(NOT MSVC)
  if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    # This option has no effect in glibc version less than 2.20.
    # Since glibc 2.20 _BSD_SOURCE is deprecated, this macro is recomended instead
    add_definitions("-D_DEFAULT_SOURCE -D_GNU_SOURCE")
  endif()

  ## This is here to support building for multiple architecture types... but we all know how well that usually goes...
  set(ARCH default CACHE STRING "CPU to build for: -march value or default")
  if("${ARCH}" STREQUAL "default")
    set(ARCH_FLAG "")
  else()
    set(ARCH_FLAG "-march=${ARCH}")
  endif()

  ## These options generate all those nice warnings we see while building
  set(WARNINGS "-Wall -Wextra -Wpointer-arith -Wvla -Wwrite-strings  -Wno-error=extra -Wno-error=unused-function -Wno-error=sign-compare -Wno-error=strict-aliasing -Wno-error=type-limits -Wno-unused-parameter -Wno-error=unused-variable -Wno-error=undef -Wno-error=uninitialized -Wno-error=unused-result")
  if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set(WARNINGS "${WARNINGS} -Wno-error=mismatched-tags -Wno-error=null-conversion -Wno-overloaded-shift-op-parentheses -Wno-error=shift-count-overflow -Wno-error=tautological-constant-out-of-range-compare -Wno-error=unused-private-field -Wno-error=unneeded-internal-declaration -Wno-error=unused-function -Wno-error=missing-braces -Wno-error=unused-command-line-argument")
  else()
    set(WARNINGS "${WARNINGS} -Wlogical-op -Wno-error=maybe-uninitialized -Wno-error=clobbered -Wno-error=unused-but-set-variable")
  endif()

  if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(WARNINGS "${WARNINGS} -Wno-error=odr")
  endif()
  set(C_WARNINGS "-Waggregate-return -Wnested-externs -Wold-style-definition -Wstrict-prototypes")
  set(CXX_WARNINGS "-Wno-reorder -Wno-missing-field-initializers")

  if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64" OR "${LABEL}" STREQUAL "aarch64")
    set(MAES_FLAG "")
    set(CRYPTOPP_AARCH64 ON CACHE BOOL FORCE "Tell CryptoPP that we are building for aarch64")
  elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64" AND NOT "${LABEL}" STREQUAL "aarch64")
    set(MAES_FLAG "-maes")
  endif()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 ${STATICASSERTC_FLAG} ${WARNINGS} ${C_WARNINGS} ${ARCH_FLAG} ${MAES_FLAG}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 ${STATICASSERTCPP_FLAG} ${WARNINGS} ${CXX_WARNINGS} ${ARCH_FLAG} ${MAES_FLAG}")

  if(APPLE)
      if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
          # Need to build against libc++ instead of libstc++ on apple
          set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -nostdinc++ -I/usr/local/opt/llvm/include/c++/v1 -nodefaultlibs -lc++ -lc++abi -lm -lc -lgcc -lgcc_eh")

          # Need these flags so gcc OSX works on the cryptopp ASM - https://groups.google.com/forum/#!topic/cryptopp-users/po8yO-vYaac
          set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCRYPTOPP_CLANG_INTEGRATED_ASSEMBLER=1")

      elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
          # Need to link against the llvm libc++ library, default is too old for std::filesystem
          set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -I/usr/local/opt/llvm/include/c++/v1")
      endif()
  endif()

  if(NOT APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
  endif()

  ## Setting up DEBUG flags
  if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(DEBUG_FLAGS "-g3 -Og -gdwarf-4 -fvar-tracking -fvar-tracking-assignments -fno-inline -fno-omit-frame-pointer")
  else()
    set(DEBUG_FLAGS "-g3 -O0 -fno-omit-frame-pointer")
  endif()

  ## Setting up RELEASE flags
  set(RELEASE_FLAGS "-Ofast -DNDEBUG -Wno-unused-variable")

  if(NOT APPLE)
    # There is a clang bug that does not allow to compile code that uses AES-NI intrinsics if -flto is enabled
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_BUILD_TYPE STREQUAL "Release")
      # On linux, to build in lto mode, check that ld.gold linker is used: 'update-alternatives --install /usr/bin/ld ld /usr/bin/ld.gold HIGHEST_PRIORITY'
      set(CMAKE_AR gcc-ar)
      set(CMAKE_RANLIB gcc-ranlib)
    endif()
  endif()

  ## Set up the normal CMake flags as we've built them
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${DEBUG_FLAGS}")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${DEBUG_FLAGS}")
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${RELEASE_FLAGS}")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${RELEASE_FLAGS}")

  ## Statically link our binaries
  if(NOT APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
  endif()
endif()

## Go get us some static BOOST libraries
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)
find_package(Boost REQUIRED COMPONENTS system filesystem thread date_time chrono regex serialization program_options)
message(STATUS "Boost Found: ${Boost_INCLUDE_DIRS}")
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
if(APPLE)
  set(Boost_LIBRARIES "${Boost_LIBRARIES}")
elseif(NOT MSVC)
  set(Boost_LIBRARIES "${Boost_LIBRARIES};rt")
endif()

## Let's see if we have any OpenSSL static libraries available
set(OPENSSL_USE_STATIC_LIBS ON)
## On MSVC, we need to link the RT/MT libraries
if(MSVC)
  set(OPENSSL_MSVC_STATIC_RT ON)
endif()

# We have to look for Homebrew OpenSSL a bit differently
# Borrowed from https://github.com/tarantool/tarantool/commit/6eab201af1843f53a833c8928dc58fceffa08147
if(APPLE)
  find_program(HOMEBREW_EXECUTABLE brew)
  execute_process(COMMAND ${HOMEBREW_EXECUTABLE} --prefix openssl
                  OUTPUT_VARIABLE HOMEBREW_OPENSSL
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
  if (DEFINED HOMEBREW_OPENSSL)
    if (NOT DEFINED OPENSSL_ROOT_DIR)
        message(STATUS "Setting OpenSSL root to ${HOMEBREW_OPENSSL}")
        set(OPENSSL_ROOT_DIR "${HOMEBREW_OPENSSL}")
    endif()
  endif()
endif()

find_package(OpenSSL)

if(OPENSSL_FOUND)
  ## On non MSVC build systems, we need to link ldl with the static OpenSSL library
  if (NOT MSVC)
    set(OPENSSL_LIBRARIES "${OPENSSL_LIBRARIES};dl")
  endif()

  include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})
  add_definitions(-DCPPHTTPLIB_OPENSSL_SUPPORT)
  message(STATUS "OpenSSL Found: ${OPENSSL_INCLUDE_DIR}")
  message(STATUS "OpenSSL Libraries: ${OPENSSL_LIBRARIES}")
else()
  message(STATUS "OpenSSL Found: No... Skipping...")
endif()

add_subdirectory(external)
add_subdirectory(src)

## We need to setup the RocksDB build environment to match our system
if(NOT MSVC)
    execute_process(
        COMMAND cmake ${CMAKE_CURRENT_SOURCE_DIR}/external/rocksdb -DWITH_LZ4=ON -DWITH_GFLAGS=0 -DCMAKE_BUILD_TYPE=MinSizeRel -DWITH_TESTS=OFF -DWITH_TOOLS=OFF -DPORTABLE=ON -B${PROJECT_BINARY_DIR}/rocksdb
    )
  set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${PROJECT_BINARY_DIR}/rocksdb/librocksdb.a")
endif()
