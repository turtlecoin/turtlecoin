# Search for the path containing library's headers
find_path(Readline_ROOT_DIR
    NAMES include/readline/readline.h
)

# Search for include directory
find_path(Readline_INCLUDE_DIR
    NAMES readline/readline.h
    HINTS ${Readline_ROOT_DIR}/include
)

# Search for library
find_library(Readline_LIBRARY
    NAMES readline
    HINTS ${Readline_ROOT_DIR}/lib
)

# Conditionally set READLINE_FOUND value
if(Readline_INCLUDE_DIR AND Readline_LIBRARY)
    set(READLINE_FOUND TRUE)
else()
    FIND_LIBRARY(Readline_LIBRARY NAMES readline)

    include(FindPackageHandleStandardArgs)

    FIND_PACKAGE_HANDLE_STANDARD_ARGS(Readline DEFAULT_MSG Readline_INCLUDE_DIR Readline_LIBRARY )

    MARK_AS_ADVANCED(Readline_INCLUDE_DIR Readline_LIBRARY)
endif()

# Apple uses the system readline library rather than GNU Readline, which doesn't
# support the stuff we need.
if (APPLE AND Readline_INCLUDE_DIR STREQUAL "/usr/include")
    # Unset readline found so it doesn't get cached, and we check again, e.g.
    # if they install the correct readline
    unset(READLINE_FOUND CACHE)

    # Also unset the two path variables, so it doesn't instantly find them
    # without re-searching
    unset(Readline_INCLUDE_DIR CACHE)
    unset(Readline_LIBRARY CACHE)

    message(FATAL_ERROR "Readline library found, but it is using the Apple version of readline rather than GNU Readline.\nTo fix this, run:\nbrew install readline; brew link --force readline\nThen, re-run cmake.\nIf this STILL fails, instead try:\nbrew uninstall --force --ignore-dependencies readline; brew install readline; brew link --force readline\nThen, re-run cmake.\nAlternatively, run:\ncmake .. -DFORCE_READLINE=FALSE\nTo disable readline support")

endif()


# Hide these variables in cmake GUIs
mark_as_advanced(
    Readline_ROOT_DIR
    Readline_INCLUDE_DIR
    Readline_LIBRARY
)
