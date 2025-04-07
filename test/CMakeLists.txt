
cmake_minimum_required(VERSION 3.14)

include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.16.0
)
FetchContent_MakeAvailable(googletest)

add_executable(pdm_test test.cpp)

set_target_properties(pdm_test PROPERTIES OUTPUT_NAME pdm_test)

target_include_directories(pdm_test PRIVATE ../ghost)

target_link_libraries (pdm_test PRIVATE gtest_main)

add_test(NAME pdm_test COMMAND pdm_test)
