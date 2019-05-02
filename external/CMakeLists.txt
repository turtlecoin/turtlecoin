set(UPNPC_BUILD_STATIC ON CACHE BOOL "Build static library")
set(UPNPC_BUILD_SHARED OFF CACHE BOOL "Build shared library")
set(UPNPC_BUILD_TESTS OFF CACHE BOOL "Build test executables")
set(WITH_MD_LIBRARY OFF CACHE BOOL "Code Generation for static library")

add_subdirectory(miniupnpc)
add_subdirectory(cryptopp)
add_subdirectory(sqlite3)
add_subdirectory(lz4)

if(MSVC)
    add_subdirectory(rocksDB EXCLUDE_FROM_ALL)
    set_property(TARGET upnpc-static cryptopp-static rocksdb sqlite3 lz4 PROPERTY FOLDER "external")
elseif(NOT MSVC)
    set_property(TARGET upnpc-static cryptopp-static sqlite3 lz4 PROPERTY FOLDER "external")
    add_custom_target(
       rocksdb
       COMMAND $(MAKE) rocksdb
       WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/rocksdb
    )
    add_library(rocksdblib STATIC IMPORTED GLOBAL)
    set_target_properties(rocksdblib PROPERTIES IMPORTED_LOCATION ${PROJECT_BINARY_DIR}/rocksdb/librocksdb.a)
    add_dependencies(rocksdblib rocksdb)
endif()

if(MSVC)
  set_property(TARGET upnpc-static APPEND_STRING PROPERTY COMPILE_FLAGS " -wd4244 -wd4267")
elseif(NOT MSVC)
  set_property(TARGET upnpc-static APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-undef -Wno-unused-result -Wno-unused-value")
endif()

