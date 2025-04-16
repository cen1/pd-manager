#cmake_policy(SET CMP0074 OLD)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
    cmake_policy(SET CMP0167 NEW)
endif()
cmake_minimum_required(VERSION 3.14)

project(pd-slave VERSION 1.0.0)

add_executable(pd-slave)

set(SOURCES
    src/bncsutilinterface.cpp
    src/bncsutilinterface.h
    src/bnet.cpp
    src/bnet.h
    src/bnetprotocol.cpp
    src/bnetprotocol.h
    src/commandpacket.cpp
    src/commandpacket.h
    src/config.cpp
    src/config.h
    src/configuration.h
    src/crc32.cpp
    src/crc32.h
    src/didyouknow.cpp
    src/didyouknow.h
    src/game_base.cpp
    src/game_base.h
    src/game_custom.cpp
    src/game_customdota.cpp
    src/game_customdota.h
    src/game_custom.h
    src/game_div1dota.cpp
    src/game_div1dota.h
    src/gameplayer.cpp
    src/gameplayer.h
    src/gameprotocol.cpp
    src/gameprotocol.h
    src/gameslot.cpp
    src/gameslot.h
    src/ghost.cpp
    src/ghostdb.cpp
    src/ghostdb.h
    src/ghost.h
    src/gpsprotocol.cpp
    src/gpsprotocol.h
    src/includes.h
    src/language.cpp
    src/language.h
    src/main.cpp
    src/map.cpp
    src/map.h
    src/masl_manager.cpp
    src/masl_manager.h
    src/masl_protocol_2.h
    src/ms_stdint.h
    src/next_combination.h
    src/packed.cpp
    src/packed.h
    src/profiler.h
    src/psr.cpp
    src/psr.h
    src/replay.cpp
    src/replay.h
    src/savegame.cpp
    src/savegame.h
    src/sha1.cpp
    src/sha1.h
    src/socket.cpp
    src/socket.h
    src/util.cpp
    src/util.h
    src/w3g_actions.txt
    src/w3g_format.txt
    src/w3version.cpp
    src/w3version.h
)

target_sources(pd-slave PRIVATE ${SOURCES})

OPTION(ENABLE_TESTS "Enable unit tests" ON)

target_compile_definitions(pd-slave PRIVATE -DBOOST_ALL_DYN_LINK -DBOOST_BIND_GLOBAL_PLACEHOLDERS -DGHOST_MYSQL )

set_target_properties(pd-slave PROPERTIES OUTPUT_NAME pd-slave)

set_target_properties(pd-slave PROPERTIES VERSION  ${PROJECT_VERSION})
set_target_properties(pd-slave PROPERTIES SOVERSION 1)
target_compile_features(pd-slave PRIVATE cxx_nullptr cxx_generic_lambdas)

if (WIN32)
	target_compile_options(pd-slave PRIVATE -D_CRT_SECURE_NO_WARNINGS -D_HAS_STD_BYTE=0)
	target_link_libraries(pd-slave PRIVATE wsock32 ws2_32)

	set_target_properties(pd-slave PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "s01.cfg")
	#set_target_properties(pd-slave PROPERTIES VS_STARTUP_PROJECT pd-slave)
	#set(VS_STARTUP_PROJECT pd-slave)
endif()

if (NOT WIN32)
	target_link_libraries(pd-slave PRIVATE -lpthread -lm -lstdc++ ${CMAKE_DL_LIBS})
endif()

if (ENABLE_LIFETIME)
	target_compile_options(pd-slave PRIVATE -Wlifetime -Wno-lifetime-pointer-arithmetic -Wno-lifetime-null)
endif()

find_package(STORMLIB REQUIRED)

target_include_directories(pd-slave PRIVATE ${BNCSUTIL_INCLUDE_DIRS} ${STORMLIB_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
target_link_libraries(pd-slave PRIVATE ${BNCSUTIL_LIBRARIES} ${STORMLIB_LIBRARIES} ${ZLIB_LIBRARIES} ${MYSQL_LIBRARIES} Boost::thread Boost::system Boost::filesystem Boost::chrono Boost::date_time Boost::regex)

install(TARGETS pd-slave RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

#Set inheritance vars for subprojects
get_target_property(PARENT_LINK_LIBRARIES pd-slave LINK_LIBRARIES)
get_target_property(PARENT_INCLUDE_DIRECTORIES pd-slave INCLUDE_DIRECTORIES)
get_target_property(PARENT_COMPILE_OPTIONS pd-slave COMPILE_OPTIONS)

#Expand $PATH with library paths to avoid missing dll on program startup
if (MSVC)
	list(JOIN Boost_LIBRARY_DIRS ";" BOOST_PATH)

	set(ZLIB_PATH "")
	FOREACH(LIB_NAME ${ZLIB_LIBRARIES})
		get_filename_component(LDIR ${LIB_NAME} DIRECTORY)
		if (NOT ZLIB_PATH MATCHES "${LDIR}")
			set(ZLIB_PATH "${ZLIB_PATH};${LDIR}")
		endif()
	ENDFOREACH()

	set(EXPAND_PATH "${BNCSUTIL_PATH}${STORMLIB_PATH}${MYSQL_PATH}${ZLIB_PATH};${BOOST_PATH}")
	message(STATUS "Expanding path with: ${EXPAND_PATH}")
	set_target_properties(pd-slave PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=%PATH%${EXPAND_PATH}")

	# Set startup project for VS
	set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT "pd-slave")
endif()

#Tests
if(ENABLE_TESTS)
    set(SOURCES_LIB ${SOURCES})
    list(REMOVE_ITEM SOURCE_LIB "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
    add_library(pd-slave-lib STATIC ${SOURCES_LIB})
    target_include_directories(pd-slave-lib PRIVATE ${PARENT_INCLUDE_DIRECTORIES})
    target_link_libraries(pd-slave-lib PRIVATE ${PARENT_LINK_LIBRARIES})

    if (DEFINED PARENT_COMPILE_OPTIONS AND NOT "${PARENT_COMPILE_OPTIONS}" STREQUAL "PARENT_COMPILE_OPTIONS-NOTFOUND")
        target_compile_options(pd-slave-lib PRIVATE ${PARENT_COMPILE_OPTIONS})
    endif()

    enable_testing()
    add_subdirectory(test)
endif()
