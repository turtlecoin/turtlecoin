AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR} source)
add_library(linenoise STATIC ${source})
set_target_properties(linenoise PROPERTIES IMPORTED_LOCATION ${PROJECT_BINARY_DIR}/linenoise/liblinenoise.a)
