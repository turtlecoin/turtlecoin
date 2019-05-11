# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_SOURCE_DIR}/external/googletest/
                 ${CMAKE_SOURCE_DIR}/googletest/build
                 EXCLUDE_FROM_ALL)

# The gtest/gtest_main targets carry header search path
# dependencies automatically when using CMake 2.8.11 or
# later. Otherwise we have to add them here ourselves.
if (CMAKE_VERSION VERSION_LESS 2.8.11)
  include_directories("${gtest_SOURCE_DIR}/include")
endif()


add_executable(
  test
  ${CMAKE_SOURCE_DIR}/src/test/test.cpp
  ${CMAKE_SOURCE_DIR}/src/Common/StringTools.test.cpp
)

target_link_libraries(
  test
  gtest_main
  Common
)

add_test(NAME test COMMAND test)
