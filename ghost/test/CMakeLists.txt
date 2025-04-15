cmake_minimum_required(VERSION 3.14)

set(gtest_force_shared_crt on)

include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.16.0
)
FetchContent_MakeAvailable(googletest)

add_executable(pds_test test.cpp PsrTestSample.h)

set_target_properties(pds_test PROPERTIES OUTPUT_NAME pds_test)

target_include_directories(pds_test PRIVATE  ${PARENT_INCLUDE_DIRECTORIES}  ../src)

target_link_libraries (pds_test PRIVATE pd-slave-lib gtest_main)

if (MSVC)
	set_target_properties(pds_test PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=%PATH%${EXPAND_PATH}")
endif()

add_test(NAME pds_test COMMAND pds_test)
