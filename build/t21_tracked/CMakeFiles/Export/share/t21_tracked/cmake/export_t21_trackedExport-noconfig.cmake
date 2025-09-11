#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "t21_tracked::t21_tracked" for configuration ""
set_property(TARGET t21_tracked::t21_tracked APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(t21_tracked::t21_tracked PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libt21_tracked.so"
  IMPORTED_SONAME_NOCONFIG "libt21_tracked.so"
  )

list(APPEND _IMPORT_CHECK_TARGETS t21_tracked::t21_tracked )
list(APPEND _IMPORT_CHECK_FILES_FOR_t21_tracked::t21_tracked "${_IMPORT_PREFIX}/lib/libt21_tracked.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
