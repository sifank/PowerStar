# define the project name
project(indi-powerstar C CXX)
cmake_minimum_required(VERSION 2.8)

include(GNUInstallDirs)
include(FindPkgConfig)

# add our cmake_modules folder
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")

# find our required packages
find_package(INDI REQUIRED)
find_package(Nova REQUIRED)
find_package(ZLIB REQUIRED)
find_package(GSL REQUIRED)

# these will be used to set the version number in config.h and our driver's xml file
set(PS_VERSION_MAJOR 0)
set(PS_VERSION_MINOR 8)

# do the replacement in the config.h
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
)

# do the replacement in the driver's xml file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/indi_powerstar.xml.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/indi_powerstar.xml
)

# set our include directories to look for header files
include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${NOVA_INCLUDE_DIR})
include_directories( ${EV_INCLUDE_DIR})
include_directories( /usr/lib/x86_64-linux-gnu)

#FIND_PACKAGE(CURL)
#IF(CURL_FOUND)
 # INCLUDE_DIRECTORIES(${CURL_INCLUDE_DIR})
  #SET(requiredlibs ${requiredlibs} ${CURL_LIBRARIES} )
#ELSE(CURL_FOUND)
 # MESSAGE(FATAL_ERROR "Could not find the CURL library and development files.")
#ENDIF(CURL_FOUND)

include(CMakeCommon)

# tell cmake to build our executable
add_executable(
    indi_powerstar
    hid.c
    PScontrol.cpp
    indi_PowerStar.cpp
)

pkg_check_modules(libusb-1.0 REQUIRED IMPORTED_TARGET libusb-1.0)

# and link it to these libraries
target_link_libraries(
    indi_powerstar
    libpthread.so.0
    PkgConfig::libusb-1.0
    ${INDI_LIBRARIES}
    ${NOVA_LIBRARIES}
    ${GSL_LIBRARIES}
)

# tell cmake where to install our executable
install(TARGETS indi_powerstar RUNTIME DESTINATION /usr/bin)

# and where to put the driver's xml file.
install(
    FILES
    ${CMAKE_CURRENT_BINARY_DIR}/indi_powerstar.xml
    DESTINATION ${INDI_DATA_DIR}
)


