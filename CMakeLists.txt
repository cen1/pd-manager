cmake_minimum_required(VERSION 3.14)
project(pd-root LANGUAGES CXX)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake/Modules")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Download bncsutil headers and dll
if(WIN32)
    message(STATUS "Fetching bncsutil")
    include(FetchContent)

    set(BNCSUTIL_URL "https://github.com/BNETDocs/bncsutil/releases/download/v1.4.2/bncsutil_v1.4.2_amd64_dll.zip")
    set(BNCSUTIL_HASH "1546321325db6a45957b3c5b6f2d7a48bc2c221a57109903c550cb9e66db511b")
    set(BNCSUTIL_DIR "${CMAKE_SOURCE_DIR}/depends/bncsutil")

    FetchContent_Declare(
            bncsutil
            URL ${BNCSUTIL_URL}
            URL_HASH SHA256=${BNCSUTIL_HASH}
            DOWNLOAD_NO_EXTRACT true
    )

    FetchContent_MakeAvailable(bncsutil)

    file(ARCHIVE_EXTRACT
            INPUT ${bncsutil_SOURCE_DIR}/bncsutil_v1.4.2_amd64_dll.zip
            DESTINATION ${BNCSUTIL_DIR}
    )
endif()

find_package(BNCSUTIL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Boost COMPONENTS filesystem system thread chrono regex date_time REQUIRED)

# Add subdirectories
# Just a little hack for faster docker builds so we can omit whole source copy
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/manager")
    add_subdirectory(manager)
else()
    message(WARNING "Directory 'manager' does not exist. Skipping add_subdirectory.")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ghost")
    add_subdirectory(ghost)
else()
    message(WARNING "Directory 'ghost' does not exist. Skipping add_subdirectory.")
endif()

