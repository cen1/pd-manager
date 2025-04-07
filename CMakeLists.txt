cmake_policy(SET CMP0048 NEW)
cmake_minimum_required(VERSION 3.14)

project(pd-manager VERSION 1.0.0)

add_executable(pd-manager)

set(SOURCES
        ghost/bncsutilinterface.cpp
        ghost/bncsutilinterface.h
        ghost/bnet.cpp
        ghost/bnet.h
        ghost/bnet_tour.cpp
        ghost/bnet_tour.h
        ghost/bnetprotocol.cpp
        ghost/bnetprotocol.h
        ghost/commandpacket.cpp
        ghost/commandpacket.h
        ghost/config.cpp
        ghost/config.h
        ghost/configuration.h
        ghost/custom_game.h
        ghost/ghost.cpp
        ghost/ghost.h
        ghost/ghostdb.cpp
        ghost/ghostdb.h
        ghost/language.cpp
        ghost/language.h
        ghost/masl_manager.cpp
        ghost/masl_manager.h
        ghost/masl_protocol_2.h
        ghost/masl_slavebot.cpp
        ghost/masl_slavebot.h
        ghost/ms_stdint.h
        ghost/next_combination.h
        ghost/psr.cpp
        ghost/psr.h
        ghost/socket.cpp
        ghost/socket.h
        ghost/typedefs.h
        ghost/util.cpp
        ghost/util.h
)

target_sources(pd-manager PRIVATE ${SOURCES})

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake/Modules")

OPTION(ENABLE_TESTS "Enable unit tests" ON)

OPTION(PD_DEBUG "Debug logging" OFF)
IF(PD_DEBUG)
    target_compile_definitions(pd-manager PRIVATE -DPD_DEBUG=1)
else()
    target_compile_definitions(pd-manager PRIVATE -DPD_DEBUG=0)
ENDIF(PD_DEBUG)

target_compile_definitions(pd-manager PRIVATE -DBOOST_ALL_DYN_LINK -DGHOST_MYSQL)

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

find_package(BNCSUTIL REQUIRED)
find_package(MYSQL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Boost COMPONENTS filesystem system thread chrono regex REQUIRED)

target_include_directories(pd-manager PRIVATE ${BNCSUTIL_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS} ${MYSQL_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
target_link_libraries(pd-manager PRIVATE ${BNCSUTIL_LIBRARIES} ${ZLIB_LIBRARIES} ${MYSQL_LIBRARIES} ${Boost_LIBRARIES})

install(TARGETS pd-manager RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

#Tests
if(ENABLE_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
