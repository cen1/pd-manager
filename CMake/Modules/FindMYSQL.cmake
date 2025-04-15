 # Try to find Mysql
 # Once done this will define
 #  MYSQL_FOUND - System has Mysql
 #  MYSQL_INCLUDE_DIR - The Mysql include directory
 #  MYSQL_LIBRARIES - The library needed to use Mysql

 find_path(MYSQL_INCLUDE_DIR NAMES mysql/mysql.h PATHS ${CMAKE_SOURCE_DIR}/mysql/include PATH_SUFFIXES mysql)
 find_library(MYSQL_LIBRARIES NAMES mysql libmysql libmysqlclient mysqlclient PATHS ${CMAKE_SOURCE_DIR}/mysql/lib /usr/lib/x86_64-linux-gnu /usr/lib64/mysql)

 include(FindPackageHandleStandardArgs)
 FIND_PACKAGE_HANDLE_STANDARD_ARGS(MYSQL DEFAULT_MSG MYSQL_INCLUDE_DIR MYSQL_LIBRARIES)
 mark_as_advanced(MYSQL_INCLUDE_DIR MYSQL_LIBRARIES)

 set(MYSQL_INCLUDE_DIRS ${MYSQL_INCLUDE_DIR} )

 set(MYSQL_PATH "")
 FOREACH(LIB_NAME ${MYSQL_LIBRARIES})
 get_filename_component(LDIR ${LIB_NAME} DIRECTORY)
 set(MYSQL_PATH "${MYSQL_PATH};${LDIR}")
 ENDFOREACH()

 MESSAGE(STATUS "MYSQL expanded path: " ${MYSQL_PATH})
 MESSAGE(STATUS "MYSQL lib: " ${MYSQL_LIBRARIES})
