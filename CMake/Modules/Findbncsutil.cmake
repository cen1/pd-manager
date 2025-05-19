# - Try to find bncsutil
# Once done, this will define:
#   bncsutil_FOUND - System has bncsutil
#   bncsutil::bncsutil - Imported target

find_path(bncsutil_INCLUDE_DIR
        NAMES bncsutil/bncsutil.h
        PATHS ${CMAKE_SOURCE_DIR}/depends/bncsutil/include
)

find_library(bncsutil_LIBRARY
        NAMES bncsutil libbncsutil
        PATHS ${CMAKE_SOURCE_DIR}/depends/bncsutil
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(bncsutil
        REQUIRED_VARS bncsutil_INCLUDE_DIR bncsutil_LIBRARY
)

mark_as_advanced(bncsutil_INCLUDE_DIR bncsutil_LIBRARY)

if(bncsutil_FOUND AND NOT TARGET bncsutil::bncsutil)
    add_library(bncsutil::bncsutil UNKNOWN IMPORTED)
    set_target_properties(bncsutil::bncsutil PROPERTIES
            IMPORTED_LOCATION "${bncsutil_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${bncsutil_INCLUDE_DIR}"
    )
endif()

MESSAGE(STATUS "bncsutil include: " ${bncsutil_INCLUDE_DIR})
MESSAGE(STATUS "bncsutil lib: " ${bncsutil_LIBRARY})
