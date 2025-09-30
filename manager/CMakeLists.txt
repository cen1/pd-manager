cmake_policy(SET CMP0048 NEW)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
    cmake_policy(SET CMP0167 NEW)
endif()
cmake_minimum_required(VERSION 3.14)

project(pd-manager VERSION 1.0.0)

add_executable(pd-manager)

set(SOURCES
    src/bncsutilinterface.cpp
    src/bncsutilinterface.h
    src/bnet.cpp
    src/bnet.h
    src/bnet_tour.cpp
    src/bnet_tour.h
    src/bnetprotocol.cpp
    src/bnetprotocol.h
    src/commandpacket.cpp
    src/commandpacket.h
    ../common/src/config.cpp
    ../common/src/config.h
    src/configuration.h
    src/custom_game.h
    src/ghost.cpp
    src/ghost.h
    src/ghostdb.cpp
    src/ghostdb.h
    src/language.cpp
    src/language.h
    src/masl_manager.cpp
    src/masl_manager.h
    src/masl_slavebot.cpp
    src/masl_slavebot.h
    src/next_combination.h
    src/psr.cpp
    src/psr.h
    src/socket.cpp
    src/socket.h
    src/typedefs.h
    src/util.cpp
    src/util.h
    ../common/src/masl_protocol_2.h
    ../common/src/ms_stdint.h
    ../common/src/regions.h
    ../common/src/sha1.h
    ../common/src/sha1.cpp
)

target_sources(pd-manager PRIVATE ${SOURCES})

OPTION(ENABLE_TESTS "Enable unit tests" ON)

OPTION(PD_DEBUG "Debug logging" OFF)
IF(PD_DEBUG)
    target_compile_definitions(pd-manager PRIVATE -DPD_DEBUG=1)
else()
    target_compile_definitions(pd-manager PRIVATE -DPD_DEBUG=0)
ENDIF(PD_DEBUG)

target_compile_definitions(pd-manager PRIVATE -DBOOST_ALL_DYN_LINK -DBOOST_BIND_GLOBAL_PLACEHOLDERS -DGHOST_MYSQL)

set_target_properties(pd-manager PROPERTIES OUTPUT_NAME pd-manager)

set_target_properties(pd-manager PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(pd-manager PROPERTIES SOVERSION 1)
target_compile_features(pd-manager PRIVATE cxx_nullptr)

if (WIN32)
    target_compile_options(pd-manager PRIVATE -D_CRT_SECURE_NO_WARNINGS)
	target_link_libraries(pd-manager PRIVATE wsock32 ws2_32)

	set_target_properties(pd-manager PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "s01.cfg")
	#set_target_properties(pd-manager PROPERTIES VS_STARTUP_PROJECT pd-manager)
	#set(VS_STARTUP_PROJECT pd-manager)
endif()

if (NOT WIN32)
	target_link_libraries(pd-manager PRIVATE -lpthread -lm -lstdc++ ${CMAKE_DL_LIBS})
endif()

if (ENABLE_LIFETIME)
	target_compile_options(pd-manager PRIVATE -Wlifetime -Wno-lifetime-pointer-arithmetic -Wno-lifetime-null)
endif()

find_package(MYSQL QUIET) # System package (may not define targets)
if(MYSQL_FOUND)
    message(STATUS "Found system MySQL package")
else()
    find_package(libmysqlclient QUIET) # Conan package

    if(TARGET libmysqlclient::libmysqlclient)
        set(MYSQL_LIBRARIES libmysqlclient::libmysqlclient)
        target_compile_definitions(pd-manager PRIVATE -DUSE_MYSQL_FLAT_INCLUDE=1)
        message(STATUS "Found MySQL from conan")
    else()
        find_package(unofficial-libmysql REQUIRED) # vcpkg package
        if(TARGET unofficial::libmysql::libmysql)
            set(MYSQL_LIBRARIES unofficial::libmysql::libmysql)
            message(STATUS "Found MySQL from vcpkg")
        else()
            message(FATAL_ERROR "Could not find MYSQL package from any package manager.")
        endif()
    endif()
endif()

target_include_directories(pd-manager PRIVATE ${ZLIB_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS} ../common/src)
target_link_libraries(pd-manager PRIVATE ${ZLIB_LIBRARIES} ${MYSQL_LIBRARIES} bncsutil::bncsutil Boost::thread Boost::system Boost::filesystem Boost::chrono Boost::date_time Boost::regex)

install(TARGETS pd-manager RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

if(WIN32 AND BUILD_SHARED_LIBS)
    # Copy conan dlls
    add_custom_command(
        TARGET pd-manager POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_BINARY_DIR}/bin" "$<TARGET_FILE_DIR:pd-manager>"
    )

    # This does not take care of transitive dependencies but we leave it anyway..
    add_custom_command(
        TARGET pd-manager POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:pd-manager> $<TARGET_FILE_DIR:pd-manager>
        COMMAND_EXPAND_LISTS
    )

    # Bncsutil workaround since we don't have cmake targets set up..
    if(BNCSUTIL_INSTALL_VIA_FETCH)
        add_custom_command(
            TARGET pd-manager POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/depends/bncsutil/bncsutil.dll" "$<TARGET_FILE_DIR:pd-manager>"
            COMMAND_EXPAND_LISTS
        )
    endif()
endif()

#Tests
if(ENABLE_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
