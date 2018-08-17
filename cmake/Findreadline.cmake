find_path(READLINE_INCLUDE_DIR readline/readline.h)
if(READLINE_USE_STATIC)
  find_library(_READLINE_LIB libreadline.a)
  if (NOT _READLINE_LIB)
   message(STATUS "Not found libreadline.a")
  endif ()
  find_library(_HISTORY_LIB libhistory.a)
  if (NOT _HISTORY_LIB)
   message(STATUS "Not found libhistory.a")
  endif ()
else()
  find_library(_READLINE_LIB libreadline.so)
  if (NOT _READLINE_LIB)
   message(STATUS "Not found libreadline.so")
  endif ()
  find_library(_HISTORY_LIB libhistory.so)
  if (NOT _HISTORY_LIB)
   message(STATUS "Not found libhistory.so")
  endif ()
endif()

set(READLINE_LIBRARY ${_READLINE_LIB})
list(APPEND READLINE_LIBRARY ${_HISTORY_LIB})
mark_as_advanced(_READLINE_LIB _HISTORY_LIB)

set(CURSES_USE_STATIC ${READLINE_USE_STATIC})
find_package(curses REQUIRED)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(READLINE DEFAULT_MSG READLINE_LIBRARY READLINE_INCLUDE_DIR)
if (READLINE_FOUND)
 set(READLINE_LIBRARIES ${READLINE_LIBRARY})
 list(APPEND READLINE_LIBRARIES ${CURSES_LIBRARIES})
 set(READLINE_INCLUDE_DIRS ${READLINE_INCLUDE_DIR} ${CURSES_INCLUDE_DIR})
endif ()

# Apple uses the system readline library rather than GNU Readline, which doesn't
# support the stuff we need.
if (APPLE AND READLINE_INCLUDE_DIR STREQUAL "/usr/include")
    # Unset readline found so it doesn't get cached, and we check again, e.g.
    # if they install the correct readline
    unset(READLINE_FOUND CACHE)

    # Also unset the two path variables, so it doesn't instantly find them
    # without re-searching
    unset(READLINE_INCLUDE_DIR CACHE)
    unset(READLINE_LIBRARY CACHE)

    message(STATUS "Readline library found, but it is using the Apple version of readline rather than GNU Readline.\nTo fix this, run:\nbrew install readline; brew link --force readline; brew install ncurses; brew link --force ncurses\nThen, re-run cmake.\nIf this STILL fails, instead try:\nbrew uninstall --force --ignore-dependencies readline; brew install readline; brew link --force readline; brew install ncurses; brew link --force ncurses\nThen, re-run cmake.\nAlternatively, run:\ncmake .. -DENABLE_READLINE=FALSE\nTo disable readline support")
endif()