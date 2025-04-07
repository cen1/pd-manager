# - Try to find Bncsutil
# Once done this will define
#  BNCSUTIL_FOUND - System has Bncsutil
#  BNCSUTIL_INCLUDE_DIR - The Bncsutil include directory
#  BNCSUTIL_LIBRARY - The library needed to use Bncsutil

find_path(BNCSUTIL_INCLUDE_DIR NAMES bncsutil/bncsutil.h PATHS ${CMAKE_SOURCE_DIR}/depends/bncsutil/include)
find_library(BNCSUTIL_LIBRARIES NAMES bncsutil libbncsutil PATHS ${CMAKE_SOURCE_DIR}/depends/bncsutil/lib )

MESSAGE(STATUS "BNCSUTIL lib: " ${BNCSUTIL_LIBRARIES})
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(BNCSUTIL DEFAULT_MSG BNCSUTIL_INCLUDE_DIR BNCSUTIL_LIBRARIES)
mark_as_advanced(BNCSUTIL_INCLUDE_DIR BNCSUTIL_LIBRARIES)

set(BNCSUTIL_INCLUDE_DIRS ${BNCSUTIL_INCLUDE_DIR} )

