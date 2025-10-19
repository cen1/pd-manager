cmake_minimum_required(VERSION 3.14)
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
    cmake_policy(SET CMP0167 NEW)
endif ()
project(pd-root LANGUAGES CXX VERSION 1.0.1)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake/Modules")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_SHARED_LIBS ON)

# Download bncsutil headers and dll
if (WIN32)
    message(STATUS "Fetching bncsutil")
    include(FetchContent)

    set(BNCSUTIL_URL "https://github.com/BNETDocs/bncsutil/releases/download/v1.4.3/bncsutil_v1.4.3_amd64_dll.zip")
    set(BNCSUTIL_HASH "fcc64c1b89485938c0120ec22a866b1d36e928b2f3a5d9325bc36d8cd0b805ef")
    set(BNCSUTIL_DIR "${CMAKE_SOURCE_DIR}/depends/bncsutil")

    FetchContent_Declare(
            bncsutil
            URL ${BNCSUTIL_URL}
            URL_HASH SHA256=${BNCSUTIL_HASH}
            DOWNLOAD_NO_EXTRACT true
    )

    FetchContent_MakeAvailable(bncsutil)

    file(ARCHIVE_EXTRACT
            INPUT ${bncsutil_SOURCE_DIR}/bncsutil_v1.4.3_amd64_dll.zip
            DESTINATION ${BNCSUTIL_DIR}
    )

    set(BNCSUTIL_INSTALL_VIA_FETCH ON)
endif ()

find_package(bncsutil REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Boost "1.83" COMPONENTS filesystem system thread chrono regex date_time REQUIRED)

# Enable testing
enable_testing()

# Add subdirectories
# Just a little hack for faster docker builds so we can omit whole source copy
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/manager")
    add_subdirectory(manager)
else ()
    message(WARNING "Directory 'manager' does not exist. Skipping add_subdirectory.")
endif ()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ghost")
    add_subdirectory(ghost)
else ()
    message(WARNING "Directory 'ghost' does not exist. Skipping add_subdirectory.")
endif ()

# CPack configuration
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_VENDOR "eurobattle.net")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local")

# DEB configuration
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/cen1/pd-manager")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "imbacen@gmail.com")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE ${ARCH})
SET(CPACK_PACKAGE_VERSION_MAJOR "1")
SET(CPACK_PACKAGE_VERSION_MINOR "0")
SET(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")

# Enable component-based packaging
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL pdslave pdmanager)

# Component-specific settings for pd-slave
set(CPACK_COMPONENT_PDSLAVE_DESCRIPTION "pd-slave - Warcraft III game lobby host")
set(CPACK_DEBIAN_PDSLAVE_PACKAGE_NAME "pd-slave")
set(CPACK_DEBIAN_PDSLAVE_FILE_NAME "pd-slave-${PROJECT_VERSION}.deb")
set(CPACK_DEBIAN_PDSLAVE_PACKAGE_DEPENDS "libmariadb-dev-compat, libboost-filesystem1.83-dev, libboost-system1.83-dev, libboost-chrono1.83-dev, libboost-thread1.83-dev, libboost-date-time1.83-dev, libboost-regex1.83-dev, bncsutil, stormlib")

# Component-specific settings for pd-manager
set(CPACK_COMPONENT_PDMANAGER_DESCRIPTION "pd-manager - Warcraft III hosting manager bot")
set(CPACK_DEBIAN_PDMANAGER_PACKAGE_NAME "pd-manager")
set(CPACK_DEBIAN_PDMANAGER_FILE_NAME "pd-manager-${PROJECT_VERSION}.deb")
set(CPACK_DEBIAN_PDMANAGER_PACKAGE_DEPENDS "libmariadb-dev-compat, libboost-filesystem1.83-dev, libboost-system1.83-dev, libboost-chrono1.83-dev, libboost-thread1.83-dev, libboost-date-time1.83-dev, libboost-regex1.83-dev, bncsutil")

include(CPack)
