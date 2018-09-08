# Please ensure your changes or patch meets minimum requirements.
#   The minimum requirements are 2.8.6. It roughly equates to
#   Ubuntu 14.05 LTS or Solaris 11.3. Please do not check in something
#   for 3.5.0 or higher because it will break LTS operating systems
#   and a number of developer boards used for testing. To test your
#   changes, please set up a Ubuntu 14.05 LTS system.

if(NOT DEFINED cryptocpp_DISPLAY_CMAKE_SUPPORT_WARNING)
  set(cryptocpp_DISPLAY_CMAKE_SUPPORT_WARNING 1)
endif()
if(cryptocpp_DISPLAY_CMAKE_SUPPORT_WARNING)
  message( STATUS
"*************************************************************************\n"
"The Crypto++ library does not officially support CMake. CMake support is a\n"
"community effort, and the library works with the folks using CMake to help\n"
"improve it. If you find an issue then please fix it or report it at\n"
"https://github.com/noloader/cryptopp-cmake.\n"
"-- *************************************************************************"
)
endif()

cmake_minimum_required(VERSION 2.8.6)
if (${CMAKE_VERSION} VERSION_LESS "3.0.0")
  project(cryptopp)
  set(cryptopp_VERSION_MAJOR 7)
  set(cryptopp_VERSION_MINOR 0)
  set(cryptopp_VERSION_PATCH 0)
else ()
  cmake_policy(SET CMP0048 NEW)
  project(cryptopp VERSION 7.0.0)
  if (NOT ${CMAKE_VERSION} VERSION_LESS "3.1.0")
    cmake_policy(SET CMP0054 NEW)
  endif ()
endif ()

# Need to set SRC_DIR manually after removing the Python library code.
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Make RelWithDebInfo the default (it does e.g. add '-O2 -g -DNDEBUG' for GNU)
#   If not in multi-configuration environments, no explicit build type or CXX
#   flags are set by the user and if we are the root CMakeLists.txt file.
if (NOT CMAKE_CONFIGURATION_TYPES AND
    NOT CMAKE_NO_BUILD_TYPE AND
    NOT CMAKE_BUILD_TYPE AND
    NOT CMAKE_CXX_FLAGS AND
    CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif ()

include(GNUInstallDirs)
include(CheckCXXCompilerFlag)

#============================================================================
# Settable options
#============================================================================

option(BUILD_STATIC "Build static library" ON)
option(BUILD_SHARED "Build shared library" OFF)
option(BUILD_TESTING "Build library tests" OFF)
option(BUILD_DOCUMENTATION "Use Doxygen to create the HTML based API documentation" OFF)
option(USE_INTERMEDIATE_OBJECTS_TARGET "Use a common intermediate objects target for the static and shared library targets" ON)

# These are IA-32 options. TODO: Add ARM A-32, Aarch64 and Power8 options.
option(DISABLE_ASM "Disable ASM" OFF)
option(DISABLE_SSSE3 "Disable SSSE3" ON)
option(DISABLE_SSE4 "Disable SSE4" ON)
option(DISABLE_AESNI "Disable AES-NI" OFF)
option(DISABLE_SHA "Disable SHA" OFF)
option(CRYPTOPP_NATIVE_ARCH "Enable native architecture" OFF)
set(CRYPTOPP_DATA_DIR "" CACHE PATH "Crypto++ test data directory")

#============================================================================
# Compiler options
#============================================================================

set(CRYPTOPP_COMPILE_DEFINITIONS)
set(CRYPTOPP_COMPILE_OPTIONS)

# Stop hiding the damn output...
# set(CMAKE_VERBOSE_MAKEFILE on)

# Always 1 ahead in Master. Also see http://groups.google.com/forum/#!topic/cryptopp-users/SFhqLDTQPG4
set(LIB_VER ${cryptopp_VERSION_MAJOR}${cryptopp_VERSION_MINOR}${cryptopp_VERSION_PATCH})

# Don't use RPATH's. The resulting binary could fail a security audit.
set(CMAKE_MACOSX_RPATH 0)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
  list(APPEND CRYPTOPP_COMPILE_OPTIONS -wd68 -wd186 -wd279 -wd327 -wd161 -wd3180)
endif ()

# Also see http://github.com/weidai11/cryptopp/issues/395
if (DISABLE_ASM)
  list(APPEND CRYPTOPP_COMPILE_DEFINITIONS CRYPTOPP_DISABLE_ASM)
endif ()
if (DISABLE_SSSE3)
  list(APPEND CRYPTOPP_COMPILE_DEFINITIONS CRYPTOPP_DISABLE_SSSE3)
endif ()
if (DISABLE_SSE4)
  list(APPEND CRYPTOPP_COMPILE_DEFINITIONS CRYPTOPP_DISABLE_SSSE4)
endif ()
if (DISABLE_AESNI)
  list(APPEND CRYPTOPP_COMPILE_DEFINITIONS CRYPTOPP_DISABLE_AESNI)
endif ()
if (DISABLE_SHA)
  list(APPEND CRYPTOPP_COMPILE_DEFINITIONS CRYPTOPP_DISABLE_SHA)
endif ()
if (NOT CRYPTOPP_DATA_DIR STREQUAL "")
  list(APPEND CRYPTOPP_COMPILE_DEFINITIONS "CRYPTOPP_DATA_DIR=${CRYPTOPP_DATA_DIR}")
endif ()

# CRYPTOPP_NATIVE_ARCH is set below once we know what it means to be "native"

###############################################################################

# Try to find a Posix compatible grep and sed. Solaris, Digital Unix,
#   Tru64, HP-UX and a few others need tweaking

if (${CMAKE_SYSTEM_NAME} STREQUAL "SunOS")
  set(GREP_CMD /usr/xpg4/bin/grep)
  set(SED_CMD /usr/xpg4/bin/sed)
else()
  set(GREP_CMD grep)
  set(SED_CMD sed)
endif ()

###############################################################################

function(CheckCompilerOption opt var)

  if (MSVC)

    # CMake does not provide a generic shell/terminal mechanism
    #  and Microsoft environments don't know what 'sh' is.
    set(${var} 0 PARENT_SCOPE)

  elseif (CMAKE_CXX_COMPILER_ID STREQUAL "SunPro")

    message(STATUS "Performing Test ${var}")
    execute_process(
      COMMAND sh -c "${CMAKE_CXX_COMPILER} ${CMAKE_CXX_FLAGS} ${opt} -E -xdumpmacros /dev/null 2>&1"
      COMMAND ${GREP_CMD} -i -c -E "illegal value ignored"
      RESULT_VARIABLE COMMAND_RESULT
      OUTPUT_VARIABLE COMMAND_OUTPUT
      OUTPUT_STRIP_TRAILING_WHITESPACE)

    # No dereference below. Thanks for the warning, CMake (not!).
    if (COMMAND_RESULT AND NOT COMMAND_OUTPUT)
      set(${var} 1 PARENT_SCOPE)
      message(STATUS "Performing Test ${var} - Success")
    else ()
      set(${var} 0 PARENT_SCOPE)
      message(STATUS "Performing Test ${var} - Failed")
    endif ()

  else ()

    CHECK_CXX_COMPILER_FLAG(${opt} ${var})

  endif ()

endfunction(CheckCompilerOption)

###############################################################################

function(DumpMachine output pattern)

  if (MSVC)

    # CMake does not provide a generic shell/terminal mechanism
    #  and Microsoft environments don't know what 'sh' is.
    set(${output} 0 PARENT_SCOPE)

  else ()

    execute_process(
      COMMAND sh -c "${CMAKE_CXX_COMPILER} -dumpmachine 2>&1"
      COMMAND ${GREP_CMD} -i -c -E "${pattern}"
      OUTPUT_VARIABLE ${output}
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(${output} "${${output}}" PARENT_SCOPE)

  endif()

endfunction(DumpMachine)

# Thansk to Anonimal for MinGW; see http://github.com/weidai11/cryptopp/issues/466
DumpMachine(CRYPTOPP_AMD64 "amd64|x86_64")
DumpMachine(CRYPTOPP_I386 "i.86")
DumpMachine(CRYPTOPP_MINGW32 "\\<mingw32\\>")
DumpMachine(CRYPTOPP_MINGW64 "w64-mingw32|mingw64")
DumpMachine(CRYPTOPP_X32 "x32")
DumpMachine(CRYPTOPP_AARCH32 "Aarch32")
DumpMachine(CRYPTOPP_AARCH64 "Aarch64")
DumpMachine(CRYPTOPP_ARMHF "armhf|arm7l|eabihf")
DumpMachine(CRYPTOPP_ARM "\\<arm\\>")

###############################################################################

# Test SunCC for a string like 'CC: Sun C++ 5.13 SunOS_i386'
if (NOT CRYPTOPP_SOLARIS)
  execute_process(COMMAND sh -c "${CMAKE_CXX_COMPILER} -V 2>&1"
    COMMAND ${GREP_CMD} -i -c "SunOS"
    OUTPUT_VARIABLE CRYPTOPP_SOLARIS
    OUTPUT_STRIP_TRAILING_WHITESPACE)
endif ()

# Test GCC for a string like 'i386-pc-solaris2.11'
if (NOT CRYPTOPP_SOLARIS)
  execute_process(COMMAND sh -c "${CMAKE_CXX_COMPILER} -dumpmachine 2>&1"
    COMMAND ${GREP_CMD} -i -c "Solaris"
    OUTPUT_VARIABLE CRYPTOPP_SOLARIS
    OUTPUT_STRIP_TRAILING_WHITESPACE)
endif ()

# DumpMachine SunCC style
if (CMAKE_CXX_COMPILER_ID STREQUAL "SunPro")

  # SunCC is 32-bit, but it builds both 32 and 64 bit. Use
  execute_process(COMMAND sh -c "${CMAKE_CXX_COMPILER} -V 2>&1"
    COMMAND ${GREP_CMD} -i -c "Sparc"
    OUTPUT_VARIABLE CRYPTOPP_SPARC
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  execute_process(COMMAND sh -c "${CMAKE_CXX_COMPILER} -V 2>&1"
    COMMAND ${GREP_CMD} -i -c -E "i386|i86"
    OUTPUT_VARIABLE CRYPTOPP_I386
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  execute_process(COMMAND isainfo -k
    COMMAND ${GREP_CMD} -i -c "i386"
    OUTPUT_VARIABLE KERNEL_I386
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  execute_process(COMMAND isainfo -k
    COMMAND ${GREP_CMD} -i -c "amd64"
    OUTPUT_VARIABLE KERNEL_AMD64
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  execute_process(COMMAND isainfo -k
    COMMAND ${GREP_CMD} -i -c "Sparc"
    OUTPUT_VARIABLE KERNEL_SPARC
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  execute_process(COMMAND isainfo -k
    COMMAND ${GREP_CMD} -i -c -E "UltraSarc|Sparc64|SparcV9"
    OUTPUT_VARIABLE KERNEL_SPARC64
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  # Build for 32-bit on 64-bit kernels unless Native Arch is
  #  specified. Note that this is a policy provided by Sun.
  if (CRYPTOPP_NATIVE_ARCH AND KERNEL_I386)
    unset(CRYPTOPP_AMD64)
    set(CRYPTOPP_I386 1)
  elseif (CRYPTOPP_NATIVE_ARCH AND KERNEL_AMD64)
    unset(CRYPTOPP_I386)
    set(CRYPTOPP_AMD64 1)
  elseif (CRYPTOPP_NATIVE_ARCH AND KERNEL_SPARC)
    unset(KERNEL_SPARC64)
    set(KERNEL_SPARC 1)
  elseif (CRYPTOPP_NATIVE_ARCH AND KERNEL_SPARC64)
    unset(KERNEL_SPARC)
    set(KERNEL_SPARC64 1)
  endif ()
endif ()

###############################################################################

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")

  execute_process(COMMAND sh -c "${CMAKE_CXX_COMPILER} --version 2>&1"
    COMMAND ${GREP_CMD} -i -c "macports"
    OUTPUT_VARIABLE MACPORTS
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  if (MACPORTS EQUAL 0)
    # Get GAS version, add defs + set as appropriate
    set(GAS_CMD sh -c "${CMAKE_CXX_COMPILER} -xc -c /dev/null -Wa,-v -o/dev/null 2>&1")

    execute_process(COMMAND ${GAS_CMD}
      OUTPUT_VARIABLE GAS_STRING
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(FIND "${GAS_STRING}" "GNU assembler" GAS_OUTPUT)

    if (NOT GAS_OUTPUT EQUAL -1)
      #.intel_syntax wasn't supported until GNU assembler 2.10

      # TODO(unassigned): string() REGEX was not cooperating at time of writing. Re-implement as needed.
      execute_process(COMMAND echo ${GAS_STRING}
        COMMAND ${GREP_CMD} -i -c -E "GNU.[Aa]ssembler.*(2\\.[1-9][0-9]|[3-9])"
        OUTPUT_VARIABLE GAS210_OR_LATER)
      if (GAS210_OR_LATER EQUAL 0)
        list(APPEND CRYPTOPP_COMPILE_DEFINITIONS CRYPTOPP_DISABLE_ASM)
        set(DISABLE_ASM 1)
      endif ()

      execute_process(COMMAND echo ${GAS_STRING}
        COMMAND ${GREP_CMD} -i -c -E "GNU.[Aa]ssembler.*(2\\.1[7-9]|2\\.[2-9]|[3-9])"
        OUTPUT_VARIABLE GAS217_OR_LATER)
      if (GAS217_OR_LATER EQUAL 0)
        list(APPEND CRYPTOPP_COMPILE_DEFINITIONS CRYPTOPP_DISABLE_SSSE3)
        set(DISABLE_SSSE3 1)
      endif ()

      # OpenBSD and CentOS 5 needed this one due to ARIA and BLAKE2
      execute_process(COMMAND echo ${GAS_STRING}
        COMMAND ${GREP_CMD} -i -c -E "GNU.[Aa]ssembler.*(2\\.1[8-9]|2\\.[2-9]|[3-9])"
        OUTPUT_VARIABLE GAS218_OR_LATER)
      if (GAS218_OR_LATER EQUAL 0)
        list(APPEND CRYPTOPP_COMPILE_DEFINITIONS CRYPTOPP_DISABLE_SSSE4)
        set(DISABLE_SSE4 1)
      endif ()

      execute_process(COMMAND echo ${GAS_STRING}
        COMMAND ${GREP_CMD} -i -c -E "GNU.[Aa]ssembler.*(2\\.19|2\\.[2-9]|[3-9])"
        OUTPUT_VARIABLE GAS219_OR_LATER)
      if (GAS219_OR_LATER EQUAL 0)
        list(APPEND CRYPTOPP_COMPILE_DEFINITIONS CRYPTOPP_DISABLE_AESNI)
        set(DISABLE_AESNI 1)
      endif ()

      # Ubuntu 10 and Ubuntu 12 needed this one
      execute_process(COMMAND echo ${GAS_STRING}
        COMMAND ${GREP_CMD} -i -c -E "GNU.[Aa]ssembler.*(2\\.2[3-9]|2\\.[3-9]|[3-9])"
        OUTPUT_VARIABLE GAS223_OR_LATER)
      if (GAS223_OR_LATER EQUAL 0)
        list(APPEND CRYPTOPP_COMPILE_DEFINITIONS CRYPTOPP_DISABLE_SHA)
        set(DISABLE_SHA 1)
      endif ()
    endif ()
  endif ()
endif ()

# TODO: what about ICC and LLVM on Windows?
if (MSVC)
  if (CMAKE_SYSTEM_VERSION MATCHES "10\\.0.*")
    list(APPEND CRYPTOPP_COMPILE_DEFINITIONS "_WIN32_WINNT=0x0A00")
  endif ()
  list(APPEND CRYPTOPP_COMPILE_OPTIONS "/FI\"winapifamily.h\"")
endif ()

# Enable PIC for all target machines except 32-bit i386 due to register pressures.
if (NOT CRYPTOPP_I386)
  SET(CMAKE_POSITION_INDEPENDENT_CODE 1)
endif ()

# Solaris specific
if (CRYPTOPP_SOLARIS)

  # SunCC needs -template=no%extdef
  if (CMAKE_CXX_COMPILER_ID STREQUAL "SunPro")
    list(APPEND CRYPTOPP_COMPILE_OPTIONS "-template=no%extdef")
  endif ()

  # SunCC needs -xregs=no%appl on Sparc (not x86) for libraries (not test program)
  # TODO: wire this up properly
  #if (CMAKE_CXX_COMPILER_ID STREQUAL "SunPro")
  #  list(APPEND CRYPTOPP_COMPILE_OPTIONS "-xregs=no%appl")
  #endif ()

  # GCC needs to enable use of '/' for division in the assembler
  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND CRYPTOPP_COMPILE_OPTIONS "-Wa,--divide")
  endif ()

endif ()

#============================================================================
# Sources & headers
#============================================================================

# Library headers
file(GLOB cryptopp_HEADERS ${SRC_DIR}/*.h)
# Remove headers used to build test suite
list(REMOVE_ITEM cryptopp_HEADERS
    ${SRC_DIR}/bench.h
    ${SRC_DIR}/validate.h
    )

# Test sources. You can use the GNUmakefile to generate the list: `make sources`.
set(cryptopp_SOURCES_TEST
    ${SRC_DIR}/test.cpp
    ${SRC_DIR}/bench1.cpp
    ${SRC_DIR}/bench2.cpp
    ${SRC_DIR}/bench3.cpp
    ${SRC_DIR}/validat0.cpp
    ${SRC_DIR}/validat1.cpp
    ${SRC_DIR}/validat2.cpp
    ${SRC_DIR}/validat3.cpp
    ${SRC_DIR}/validat4.cpp
    ${SRC_DIR}/validat5.cpp
    ${SRC_DIR}/validat6.cpp
    ${SRC_DIR}/validat7.cpp
    ${SRC_DIR}/validat8.cpp
    ${SRC_DIR}/validat9.cpp
    ${SRC_DIR}/validat10.cpp
    ${SRC_DIR}/regtest1.cpp
    ${SRC_DIR}/regtest2.cpp
    ${SRC_DIR}/regtest3.cpp
    ${SRC_DIR}/regtest4.cpp
    ${SRC_DIR}/datatest.cpp
    ${SRC_DIR}/dlltest.cpp
    ${SRC_DIR}/fipsalgt.cpp
    ${SRC_DIR}/fipstest.cpp
    )

# Template sources
set(cryptopp_SOURCES_TEMPLATE
  ${SRC_DIR}/algebra.cpp
  ${SRC_DIR}/eccrypto.cpp
  ${SRC_DIR}/eprecomp.cpp
  ${SRC_DIR}/polynomi.cpp
  ${SRC_DIR}/strciphr.cpp
  )

# Library sources. You can use the GNUmakefile to generate the list: `make sources`.
# Makefile sorted them at http://github.com/weidai11/cryptopp/pull/426.
file(GLOB cryptopp_SOURCES ${SRC_DIR}/*.cpp)
list(SORT cryptopp_SOURCES)
list(REMOVE_ITEM cryptopp_SOURCES
    ${SRC_DIR}/cryptlib.cpp
    ${SRC_DIR}/cpu.cpp
    ${SRC_DIR}/integer.cpp
    ${SRC_DIR}/pch.cpp
    ${SRC_DIR}/simple.cpp
    ${cryptopp_SOURCES_TEMPLATE}
    ${cryptopp_SOURCES_TEST}
    )
set(cryptopp_SOURCES
    ${SRC_DIR}/cryptlib.cpp
    ${SRC_DIR}/cpu.cpp
    ${SRC_DIR}/integer.cpp
    ${cryptopp_SOURCES}
    )

set(cryptopp_SOURCES_ASM)

if (MSVC AND NOT DISABLE_ASM)
  if (${CMAKE_GENERATOR} MATCHES ".*ARM")
    message(STATUS "Disabling ASM because ARM is specified as target platform.")
  else ()
    enable_language(ASM_MASM)
    list(APPEND cryptopp_SOURCES_ASM
      ${SRC_DIR}/rdrand.asm
      )
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
      list(APPEND cryptopp_SOURCES_ASM
        ${SRC_DIR}/x64dll.asm
        ${SRC_DIR}/x64masm.asm
        )
      set_source_files_properties(${cryptopp_SOURCES_ASM} PROPERTIES COMPILE_DEFINITIONS "_M_X64")
    else ()
      set_source_files_properties(${cryptopp_SOURCES_ASM} PROPERTIES COMPILE_DEFINITIONS "_M_X86" COMPILE_FLAGS "/safeseh")
    endif ()
    set_source_files_properties(${cryptopp_SOURCES_ASM} PROPERTIES LANGUAGE ASM_MASM)
  endif ()
endif ()

#============================================================================
# Architecture flags
#============================================================================

# TODO: Android, AIX, IBM xlC, iOS and a few other profiles are missing.

# New as of Pull Request 461, http://github.com/weidai11/cryptopp/pull/461.
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Intel")

  if (CRYPTOPP_AMD64 OR CRYPTOPP_I386 OR CRYPTOPP_X32)

    CheckCompilerOption("-msse2" CRYPTOPP_IA32_SSE2)
    CheckCompilerOption("-mssse3" CRYPTOPP_IA32_SSSE3)
    CheckCompilerOption("-msse4.1" CRYPTOPP_IA32_SSE41)
    CheckCompilerOption("-msse4.2" CRYPTOPP_IA32_SSE42)
    CheckCompilerOption("-mssse3 -mpclmul" CRYPTOPP_IA32_CLMUL)
    CheckCompilerOption("-msse4.1 -maes" CRYPTOPP_IA32_AES)
    CheckCompilerOption("-msse4.2 -msha" CRYPTOPP_IA32_SHA)
    CheckCompilerOption("-march=native" CRYPTOPP_IA32_NATIVE)

    # Deferred native arch; added to all CXXFLAGS
    if (CRYPTOPP_NATIVE_ARCH AND CRYPTOPP_IA32_NATIVE)
      list(APPEND CRYPTOPP_COMPILE_OPTIONS "-march=native")
    elseif (CRYPTOPP_NATIVE_ARCH)
      message(WARNING "CRYPTOPP_NATIVE_ARCH enabled, but failed to detect native architecture")
    endif ()

    if (CRYPTOPP_IA32_SSE2 AND NOT DISABLE_ASM)
      set_source_files_properties(${SRC_DIR}/sse-simd.cpp PROPERTIES COMPILE_FLAGS "-msse2")
    endif ()
    if (CRYPTOPP_IA32_SSSE3 AND NOT DISABLE_SSSE3)
      set_source_files_properties(${SRC_DIR}/aria-simd.cpp PROPERTIES COMPILE_FLAGS "-mssse3")
      set_source_files_properties(${SRC_DIR}/cham-simd.cpp PROPERTIES COMPILE_FLAGS "-mssse3")
      set_source_files_properties(${SRC_DIR}/lea-simd.cpp PROPERTIES COMPILE_FLAGS "-mssse3")
      set_source_files_properties(${SRC_DIR}/simeck-simd.cpp PROPERTIES COMPILE_FLAGS "-mssse3")
      set_source_files_properties(${SRC_DIR}/simon128-simd.cpp PROPERTIES COMPILE_FLAGS "-mssse3")
      set_source_files_properties(${SRC_DIR}/speck128-simd.cpp PROPERTIES COMPILE_FLAGS "-mssse3")
      if (CRYPTOPP_IA32_SSE41 AND NOT DISABLE_SSE4)
        set_source_files_properties(${SRC_DIR}/blake2-simd.cpp PROPERTIES COMPILE_FLAGS "-msse4.1")
        set_source_files_properties(${SRC_DIR}/simon64-simd.cpp PROPERTIES COMPILE_FLAGS "-msse4.1")
        set_source_files_properties(${SRC_DIR}/speck64-simd.cpp PROPERTIES COMPILE_FLAGS "-msse4.1")
      endif ()
      if (CRYPTOPP_IA32_SSE42 AND NOT DISABLE_SSE4)
        set_source_files_properties(${SRC_DIR}/crc-simd.cpp PROPERTIES COMPILE_FLAGS "-msse4.2")
        if (CRYPTOPP_IA32_CLMUL AND NOT DISABLE_AES)
          set_source_files_properties(${SRC_DIR}/gcm-simd.cpp PROPERTIES COMPILE_FLAGS "-mssse3 -mpclmul")
        endif ()
        if (CRYPTOPP_IA32_AES AND NOT DISABLE_AES)
          set_source_files_properties(${SRC_DIR}/rijndael-simd.cpp PROPERTIES COMPILE_FLAGS "-msse4.1 -maes")
          set_source_files_properties(${SRC_DIR}/sm4-simd.cpp PROPERTIES COMPILE_FLAGS "-mssse3 -maes")
        endif ()
        if (CRYPTOPP_IA32_SHA AND NOT DISABLE_SHA)
          set_source_files_properties(${SRC_DIR}/sha-simd.cpp PROPERTIES COMPILE_FLAGS "-msse4.2 -msha")
          set_source_files_properties(${SRC_DIR}/shacal2-simd.cpp PROPERTIES COMPILE_FLAGS "-msse4.2 -msha")
        endif ()
      endif ()
    endif ()

  elseif (CRYPTOPP_AARCH32 OR CRYPTOPP_AARCH64)

    CheckCompilerOption("-march=armv8-a" CRYPTOPP_ARMV8A_BASE)
    CheckCompilerOption("-march=armv8-a+crc" CRYPTOPP_ARMV8A_CRC)
    CheckCompilerOption("-march=armv8-a+crypto" CRYPTOPP_ARMV8A_CRYPTO)
    CheckCompilerOption("-march=armv8-a" CRYPTOPP_ARMV8A_NATIVE)

    # Deferred native arch; added to all CXXFLAGS
    if (CRYPTOPP_NATIVE_ARCH AND CRYPTOPP_ARMV8A_NATIVE)
      list(APPEND CRYPTOPP_COMPILE_OPTIONS "-march=armv8-a")
    elseif (CRYPTOPP_NATIVE_ARCH)
      message(WARNING "CRYPTOPP_NATIVE_ARCH enabled, but failed to detect native architecture")
    endif ()

    if (CRYPTOPP_ARMV8A_BASE)
      set_source_files_properties(${SRC_DIR}/aria-simd.cpp PROPERTIES COMPILE_FLAGS "-march=armv8-a")
      set_source_files_properties(${SRC_DIR}/blake2-simd.cpp PROPERTIES COMPILE_FLAGS "-march=armv8-a")
      set_source_files_properties(${SRC_DIR}/cham-simd.cpp PROPERTIES COMPILE_FLAGS "-march=armv8-a")
      set_source_files_properties(${SRC_DIR}/lea-simd.cpp PROPERTIES COMPILE_FLAGS "-march=armv8-a")
      set_source_files_properties(${SRC_DIR}/neon-simd.cpp PROPERTIES COMPILE_FLAGS "-march=armv8-a")
      set_source_files_properties(${SRC_DIR}/simeck-simd.cpp PROPERTIES COMPILE_FLAGS "-march=armv8-a")
      set_source_files_properties(${SRC_DIR}/simon64-simd.cpp PROPERTIES COMPILE_FLAGS "-march=armv8-a")
      set_source_files_properties(${SRC_DIR}/simon128-simd.cpp PROPERTIES COMPILE_FLAGS "-march=armv8-a")
      set_source_files_properties(${SRC_DIR}/speck64-simd.cpp PROPERTIES COMPILE_FLAGS "-march=armv8-a")
      set_source_files_properties(${SRC_DIR}/speck128-simd.cpp PROPERTIES COMPILE_FLAGS "-march=armv8-a")
    endif ()
    if (CRYPTOPP_ARMV8A_CRC)
      set_source_files_properties(${SRC_DIR}/crc-simd.cpp PROPERTIES COMPILE_FLAGS "-march=armv8-a+crc")
    endif ()
    if (CRYPTOPP_ARMV8A_CRYPTO)
      set_source_files_properties(${SRC_DIR}/gcm-simd.cpp PROPERTIES COMPILE_FLAGS "-march=armv8-a+crypto")
      set_source_files_properties(${SRC_DIR}/rijndael-simd.cpp PROPERTIES COMPILE_FLAGS "-march=armv8-a+crypto")
      set_source_files_properties(${SRC_DIR}/sha-simd.cpp PROPERTIES COMPILE_FLAGS "-march=armv8-a+crypto")
      set_source_files_properties(${SRC_DIR}/shacal2-simd.cpp PROPERTIES COMPILE_FLAGS "-march=armv8-a+crypto")
    endif ()

  elseif (CRYPTOPP_ARM OR CRYPTOPP_ARMHF)
    CheckCompilerOption("-march=armv7-a -mfpu=neon" CRYPTOPP_ARMV7A_NEON)
    CheckCompilerOption("-march=armv7-a -mfloat-abi=hard -mfpu=neon" CRYPTOPP_ARMV7A_HARD)
    CheckCompilerOption("-march=armv7-a -mfloat-abi=softfp -mfpu=neon" CRYPTOPP_ARMV7A_SOFTFP)

    if (CRYPTOPP_ARMV7A_HARD)
      set(CRYPTOPP_ARMV7A_FLAGS "-march=armv7-a -mfloat-abi=hard -mfpu=neon")
    elseif (CRYPTOPP_ARMV7A_SOFTFP)
      set(CRYPTOPP_ARMV7A_FLAGS "-march=armv7-a -mfloat-abi=softfp -mfpu=neon")
    endif()

    # Deferred native arch; added to all CXXFLAGS
    if (CRYPTOPP_NATIVE_ARCH AND (CRYPTOPP_ARMV7A_HARD OR CRYPTOPP_ARMV7A_SOFTFP))
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CRYPTOPP_ARMV7A_FLAGS}")
    elseif (CRYPTOPP_NATIVE_ARCH)
      message(WARNING "CRYPTOPP_NATIVE_ARCH enabled, but failed to detect native architecture")
    endif ()

    if (CRYPTOPP_ARMV7A_HARD OR CRYPTOPP_ARMV7A_SOFTFP)
      # Add ASM files for ARM
      if (NOT MSVC)
        list(APPEND cryptopp_SOURCES ${SRC_DIR}/aes-armv4.S)
        set_source_files_properties(${SRC_DIR}/aes-armv4.S PROPERTIES LANGUAGE C)
      endif ()

      # Need to set floating point ABI to something, like "hard" of "softfp". Most Linux use hard floats.
      set_source_files_properties(${SRC_DIR}/aes-armv4.S PROPERTIES COMPILE_FLAGS "${CRYPTOPP_ARMV7A_FLAGS}")
      set_source_files_properties(${SRC_DIR}/aria-simd.cpp PROPERTIES COMPILE_FLAGS "${CRYPTOPP_ARMV7A_FLAGS}")
      set_source_files_properties(${SRC_DIR}/blake2-simd.cpp PROPERTIES COMPILE_FLAGS "${CRYPTOPP_ARMV7A_FLAGS}")
      set_source_files_properties(${SRC_DIR}/cham-simd.cpp PROPERTIES COMPILE_FLAGS "${CRYPTOPP_ARMV7A_FLAGS}")
      set_source_files_properties(${SRC_DIR}/crc-simd.cpp PROPERTIES COMPILE_FLAGS "${CRYPTOPP_ARMV7A_FLAGS}")
      set_source_files_properties(${SRC_DIR}/lea-simd.cpp PROPERTIES COMPILE_FLAGS "${CRYPTOPP_ARMV7A_FLAGS}")
      set_source_files_properties(${SRC_DIR}/gcm-simd.cpp PROPERTIES COMPILE_FLAGS "${CRYPTOPP_ARMV7A_FLAGS}")
      set_source_files_properties(${SRC_DIR}/rijndael-simd.cpp PROPERTIES COMPILE_FLAGS "${CRYPTOPP_ARMV7A_FLAGS}")
      set_source_files_properties(${SRC_DIR}/neon-simd.cpp PROPERTIES COMPILE_FLAGS "${CRYPTOPP_ARMV7A_FLAGS}")
      set_source_files_properties(${SRC_DIR}/sha-simd.cpp PROPERTIES COMPILE_FLAGS "${CRYPTOPP_ARMV7A_FLAGS}")
      set_source_files_properties(${SRC_DIR}/simeck-simd.cpp PROPERTIES COMPILE_FLAGS "${CRYPTOPP_ARMV7A_FLAGS}")
      set_source_files_properties(${SRC_DIR}/simon64-simd.cpp PROPERTIES COMPILE_FLAGS "${CRYPTOPP_ARMV7A_FLAGS}")
      set_source_files_properties(${SRC_DIR}/simon128-simd.cpp PROPERTIES COMPILE_FLAGS "${CRYPTOPP_ARMV7A_FLAGS}")
      set_source_files_properties(${SRC_DIR}/speck64-simd.cpp PROPERTIES COMPILE_FLAGS "${CRYPTOPP_ARMV7A_FLAGS}")
      set_source_files_properties(${SRC_DIR}/speck128-simd.cpp PROPERTIES COMPILE_FLAGS "${CRYPTOPP_ARMV7A_FLAGS}")
      set_source_files_properties(${SRC_DIR}/sm4-simd.cpp PROPERTIES COMPILE_FLAGS "${CRYPTOPP_ARMV7A_FLAGS}")
    endif ()
  endif ()
endif ()

# New as of Pull Request 461, http://github.com/weidai11/cryptopp/pull/461.
if (CMAKE_CXX_COMPILER_ID STREQUAL "SunPro")

  if (CRYPTOPP_AMD64 OR CRYPTOPP_I386 OR CRYPTOPP_X32)

    CheckCompilerOption("-xarch=sse2" CRYPTOPP_IA32_SSE2)
    CheckCompilerOption("-xarch=ssse3" CRYPTOPP_IA32_SSSE3)
    CheckCompilerOption("-xarch=sse4_1" CRYPTOPP_IA32_SSE41)
    CheckCompilerOption("-xarch=sse4_2" CRYPTOPP_IA32_SSE42)
    CheckCompilerOption("-xarch=aes" CRYPTOPP_IA32_CLMUL)
    CheckCompilerOption("-xarch=aes" CRYPTOPP_IA32_AES)
    CheckCompilerOption("-xarch=sha" CRYPTOPP_IA32_SHA)
    CheckCompilerOption("-native" CRYPTOPP_IA32_NATIVE)

    # Deferred native arch; added to all CXXFLAGS
    if (CRYPTOPP_NATIVE_ARCH AND CRYPTOPP_IA32_NATIVE)
      list(APPEND CRYPTOPP_COMPILE_OPTIONS "-native")
    elseif (CRYPTOPP_NATIVE_ARCH)
      message(WARNING "CRYPTOPP_NATIVE_ARCH enabled, but failed to detect native architecture")
    endif ()

    # TODO: each of these -xarch=XXX options must be added to LDFLAGS
    #  if the option is used during a compile.
    if (CRYPTOPP_IA32_SSE2 AND NOT DISABLE_ASM)
      set_source_files_properties(${SRC_DIR}/sse-simd.cpp PROPERTIES COMPILE_FLAGS "-xarch=sse2")
    endif ()
    if (CRYPTOPP_IA32_SSSE3 AND NOT DISABLE_SSSE3)
      set_source_files_properties(${SRC_DIR}/aria-simd.cpp PROPERTIES COMPILE_FLAGS "-xarch=ssse3")
      set_source_files_properties(${SRC_DIR}/cham-simd.cpp PROPERTIES COMPILE_FLAGS "-xarch=ssse3")
      set_source_files_properties(${SRC_DIR}/lea-simd.cpp PROPERTIES COMPILE_FLAGS "-xarch=ssse3")
      set_source_files_properties(${SRC_DIR}/simeck-simd.cpp PROPERTIES COMPILE_FLAGS "-xarch=ssse3")
      set_source_files_properties(${SRC_DIR}/simon128-simd.cpp PROPERTIES COMPILE_FLAGS "-xarch=ssse3")
      set_source_files_properties(${SRC_DIR}/speck128-simd.cpp PROPERTIES COMPILE_FLAGS "-xarch=ssse3")
      if (CRYPTOPP_IA32_SSE41 AND NOT DISABLE_SSE4)
        set_source_files_properties(${SRC_DIR}/blake2-simd.cpp PROPERTIES COMPILE_FLAGS "-xarch=sse4_1")
        set_source_files_properties(${SRC_DIR}/simon64-simd.cpp PROPERTIES COMPILE_FLAGS "-xarch=sse4_1")
        set_source_files_properties(${SRC_DIR}/speck64-simd.cpp PROPERTIES COMPILE_FLAGS "-xarch=sse4_1")
      endif ()
      if (CRYPTOPP_IA32_SSE42 AND NOT DISABLE_SSE4)
        set_source_files_properties(${SRC_DIR}/crc-simd.cpp PROPERTIES COMPILE_FLAGS "-xarch=sse4_2")
        if (CRYPTOPP_IA32_CLMUL AND NOT DISABLE_AES)
          set_source_files_properties(${SRC_DIR}/gcm-simd.cpp PROPERTIES COMPILE_FLAGS "-xarch=aes")
        endif ()
        if (CRYPTOPP_IA32_AES AND NOT DISABLE_AES)
          set_source_files_properties(${SRC_DIR}/rijndael-simd.cpp PROPERTIES COMPILE_FLAGS "-xarch=aes")
          set_source_files_properties(${SRC_DIR}/sm4-simd.cpp PROPERTIES COMPILE_FLAGS "-xarch=aes")
        endif ()
        if (CRYPTOPP_IA32_SHA AND NOT DISABLE_SHA)
          set_source_files_properties(${SRC_DIR}/sha-simd.cpp PROPERTIES COMPILE_FLAGS "-xarch=sha")
          set_source_files_properties(${SRC_DIR}/shacal2-simd.cpp PROPERTIES COMPILE_FLAGS "-xarch=sha")
        endif ()
      endif ()
    endif ()

  elseif (CRYPTOPP_SPARC OR CRYPTOPP_SPARC64)

    # Deferred native arch; added to all CXXFLAGS
    if (CRYPTOPP_NATIVE_ARCH AND CRYPTOPP_SPARC_NATIVE)
      list(APPEND CRYPTOPP_COMPILE_OPTIONS "-native")
    elseif (CRYPTOPP_NATIVE_ARCH)
      message(WARNING "CRYPTOPP_NATIVE_ARCH enabled, but failed to detect native architecture")
    endif ()

  endif ()
endif ()

#============================================================================
# Compile targets
#============================================================================

# Work around the archaic versions of cmake that do not support
# target_compile_xxxx commands
# !!! DO NOT try to use the old way for newwer version - it does not work !!!
function(cryptopp_target_compile_properties target)
  if (NOT ${CMAKE_VERSION} VERSION_LESS "2.8.11")
    target_compile_definitions(${target} PUBLIC ${CRYPTOPP_COMPILE_DEFINITIONS})
  else()
    string (REPLACE ";" " " PROP_STR "${CRYPTOPP_COMPILE_DEFINITIONS}")
    set_target_properties(${target} PROPERTIES COMPILE_DEFINITIONS "${CRYPTOPP_COMPILE_DEFINITIONS}")
  endif()
  if (NOT ${CMAKE_VERSION} VERSION_LESS "2.8.12")
    target_compile_options(${target} PUBLIC ${CRYPTOPP_COMPILE_OPTIONS})
  else()
    string (REPLACE ";" " " PROP_STR "${CRYPTOPP_COMPILE_OPTIONS}")
    set_target_properties(${target} PROPERTIES COMPILE_FLAGS "${PROP_STR}")
  endif()
endfunction()

set(cryptopp_LIBRARY_SOURCES ${cryptopp_SOURCES_ASM})
if (USE_INTERMEDIATE_OBJECTS_TARGET AND NOT ${CMAKE_VERSION} VERSION_LESS "2.8.8")
  add_library(cryptopp-object OBJECT ${cryptopp_SOURCES})
  cryptopp_target_compile_properties(cryptopp-object)

  list(APPEND cryptopp_LIBRARY_SOURCES
    $<TARGET_OBJECTS:cryptopp-object>
    )
else ()
  list(APPEND cryptopp_LIBRARY_SOURCES
    ${cryptopp_SOURCES}
    )
endif ()

if (BUILD_STATIC)
  add_library(cryptopp-static STATIC ${cryptopp_LIBRARY_SOURCES})
  cryptopp_target_compile_properties(cryptopp-static)
  if (NOT ${CMAKE_VERSION} VERSION_LESS "2.8.11")
    target_include_directories(cryptopp-static PUBLIC $<BUILD_INTERFACE:${SRC_DIR}> $<INSTALL_INTERFACE:include>)
  else ()
    set_target_properties(cryptopp-static PROPERTIES INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${SRC_DIR}> $<INSTALL_INTERFACE:include>")
  endif ()
endif ()

if (BUILD_SHARED)
  add_library(cryptopp-shared SHARED ${cryptopp_LIBRARY_SOURCES})
  cryptopp_target_compile_properties(cryptopp-shared)
  if (NOT ${CMAKE_VERSION} VERSION_LESS "2.8.11")
    target_include_directories(cryptopp-shared PUBLIC $<BUILD_INTERFACE:${SRC_DIR}> $<INSTALL_INTERFACE:include>)
  else ()
    set_target_properties(cryptopp-shared PROPERTIES INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${SRC_DIR}> $<INSTALL_INTERFACE:include>")
  endif ()
endif ()

# Set filenames for targets to be "cryptopp"
if (NOT MSVC)
  set(COMPAT_VERSION ${cryptopp_VERSION_MAJOR}.${cryptopp_VERSION_MINOR})

  if (BUILD_STATIC)
    set_target_properties(cryptopp-static
        PROPERTIES
        OUTPUT_NAME cryptopp)
  endif ()
  if (BUILD_SHARED)
    set_target_properties(cryptopp-shared
        PROPERTIES
        SOVERSION ${COMPAT_VERSION}
        OUTPUT_NAME cryptopp)
  endif ()
endif ()

# Add alternate ways to invoke the build for the shared library that are 
# similar to how the crypto++ 'make' tool works.
# see https://github.com/noloader/cryptopp-cmake/issues/32
if (BUILD_STATIC)
  add_custom_target(static DEPENDS cryptopp-static)
endif ()
if (BUILD_SHARED)
  add_custom_target(shared DEPENDS cryptopp-shared)
  add_custom_target(dynamic DEPENDS cryptopp-shared)
endif ()

#============================================================================
# Third-party libraries
#============================================================================

if (WIN32)
  if (BUILD_STATIC)
    target_link_libraries(cryptopp-static ws2_32)
  endif ()
  if (BUILD_SHARED)
    target_link_libraries(cryptopp-shared ws2_32)
  endif ()
endif ()

# This may need to be expanded to "Solaris"
if (CRYPTOPP_SOLARIS)
  if (BUILD_STATIC)
    target_link_libraries(cryptopp-static nsl socket)
  endif ()
  if (BUILD_SHARED)
    target_link_libraries(cryptopp-shared nsl socket)
  endif ()
endif ()

find_package(Threads)
if (BUILD_STATIC)
  target_link_libraries(cryptopp-static ${CMAKE_THREAD_LIBS_INIT})
endif ()
if (BUILD_SHARED)
  target_link_libraries(cryptopp-shared ${CMAKE_THREAD_LIBS_INIT})
endif ()

#============================================================================
# Tests
#============================================================================

enable_testing()
if (BUILD_TESTING)
  add_executable(cryptest ${cryptopp_SOURCES_TEST} ${cryptopp_SOURCES_TEMPLATE})
  target_link_libraries(cryptest cryptopp-static)

  # Setting "cryptest" binary name to "cryptest.exe"
  if (NOT (WIN32 OR CYGWIN))
    set_target_properties(cryptest PROPERTIES OUTPUT_NAME cryptest.exe)
  endif ()
  if (NOT TARGET cryptest.exe)
    add_custom_target(cryptest.exe)
    add_dependencies(cryptest.exe cryptest)
  endif ()

  file(COPY ${SRC_DIR}/TestData DESTINATION ${PROJECT_BINARY_DIR})
  file(COPY ${SRC_DIR}/TestVectors DESTINATION ${PROJECT_BINARY_DIR})

  add_test(NAME build_cryptest COMMAND "${CMAKE_COMMAND}" --build ${CMAKE_BINARY_DIR} --target cryptest)
  add_test(NAME cryptest COMMAND $<TARGET_FILE:cryptest> v)
  set_tests_properties(cryptest PROPERTIES DEPENDS build_cryptest)
endif ()

#============================================================================
# Doxygen documentation
#============================================================================

if (BUILD_DOCUMENTATION)
  find_package(Doxygen REQUIRED)

  set(in_source_DOCS_DIR "${SRC_DIR}/html-docs")
  set(out_source_DOCS_DIR "${PROJECT_BINARY_DIR}/html-docs")

  add_custom_target(docs ALL
      COMMAND ${DOXYGEN_EXECUTABLE} Doxyfile -d CRYPTOPP_DOXYGEN_PROCESSING
      WORKING_DIRECTORY ${SRC_DIR}
      SOURCES ${SRC_DIR}/Doxyfile
      )

  if (NOT ${in_source_DOCS_DIR} STREQUAL ${out_source_DOCS_DIR})
    add_custom_command(
        TARGET docs POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${in_source_DOCS_DIR}" "${out_source_DOCS_DIR}"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${in_source_DOCS_DIR}"
    )
  endif ()
endif ()

#============================================================================
# Install
#============================================================================

set(export_name "cryptopp-targets")

# Runtime package
if (BUILD_SHARED)
  install(
      TARGETS cryptopp-shared
      EXPORT ${export_name}
      DESTINATION ${CMAKE_INSTALL_LIBDIR}
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
endif ()

# Development package
if (BUILD_STATIC)
  install(TARGETS cryptopp-static EXPORT ${export_name} DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif ()
install(FILES ${cryptopp_HEADERS} DESTINATION include/cryptopp)

# CMake Package
if (NOT CMAKE_VERSION VERSION_LESS 2.8.8)
  include(CMakePackageConfigHelpers)
  write_basic_package_version_file("${PROJECT_BINARY_DIR}/cryptopp-config-version.cmake" VERSION ${cryptopp_VERSION_MAJOR}.${cryptopp_VERSION_MINOR}.${cryptopp_VERSION_PATCH} COMPATIBILITY SameMajorVersion)
  install(FILES cryptopp-config.cmake ${PROJECT_BINARY_DIR}/cryptopp-config-version.cmake DESTINATION "lib/cmake/cryptopp")
  install(EXPORT ${export_name} DESTINATION "lib/cmake/cryptopp")
endif ()

# Tests
if (BUILD_TESTING)
  install(TARGETS cryptest DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(DIRECTORY ${SRC_DIR}/TestData DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cryptopp)
  install(DIRECTORY ${SRC_DIR}/TestVectors DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cryptopp)
endif ()

# Documentation
if (BUILD_DOCUMENTATION)
  install(DIRECTORY "${out_source_DOCS_DIR}" DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif ()

# Print a configuration summary. We want CXX and CXXFLAGS, but they are not includd in ALL.
if (CRYPTOPP_I386)
  message(STATUS "Platform: i386/i686")
elseif (CRYPTOPP_AMD64)
  message(STATUS "Platform: x86_64")
elseif (CRYPTOPP_X32)
  message(STATUS "Platform: x86_64-x32")
elseif (CRYPTOPP_ARMHF)
  message(STATUS "Platform: armhf")
elseif (CRYPTOPP_ARM)
  message(STATUS "Platform: arm")
elseif (CRYPTOPP_AARCH32)
  message(STATUS "Platform: Aarch32")
elseif (CRYPTOPP_AARCH64)
  message(STATUS "Platform: Aarch64")
elseif (CRYPTOPP_SPARC)
  message(STATUS "Platform: Sparc")
elseif (CRYPTOPP_SPARC64)
  message(STATUS "Platform: Sparc64")
elseif (CRYPTOPP_POWERPC)
  message(STATUS "Platform: PowerPC")
elseif (CRYPTOPP_POWERPC64)
  message(STATUS "Platform: PowerPC64")
elseif (CRYPTOPP_MINGW32)
  message(STATUS "Platform: MinGW-32")
elseif (CRYPTOPP_MINGW32)
  message(STATUS "Platform: MinGW-64")
endif ()
if (CRYPTOPP_ARMV7A_NEON)
  message(STATUS "NEON: TRUE")
endif ()
if (CRYPTOPP_NATIVE_ARCH)
  message(STATUS "Native arch: TRUE")
else ()
  message(STATUS "Native arch: FALSE")
endif ()
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "Flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "Compiler options: ${CRYPTOPP_COMPILE_OPTIONS}")
message(STATUS "Compiler definitions: ${CRYPTOPP_COMPILE_DEFINITIONS}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
