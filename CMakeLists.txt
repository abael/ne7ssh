cmake_minimum_required(VERSION 3.1)
project(net7ssh CXX)

find_file(HAVE_BOTAN "botan/botan.h" PATHS "${BOTAN_DIR}/include" "${CMAKE_SOURCE_DIR}/botan/include")
if (NOT HAVE_BOTAN)
    MESSAGE(FATAL_ERROR "Could not find Botan includes ${BOTAN_DIR}")
endif()
string(FIND ${HAVE_BOTAN} "botan/botan.h" BOTAN_STR)
string(SUBSTRING ${HAVE_BOTAN} 0 ${BOTAN_STR} HAVE_BOTAN)
find_library(HAVE_BOTAN_LIB NAMES botan PATHS "${BOTAN_DIR}/lib" "${BOTAN_DIR}" "${CMAKE_SOURCE_DIR}/botan/lib" "${CMAKE_SOURCE_DIR}/botan/")
if (NOT HAVE_BOTAN_LIB)
    MESSAGE(FATAL_ERROR "Could not find Botan library")
endif()

if(MSVC)
    add_definitions(-DWIN32_LEAN_AND_MEAN -D_CRT_SECURE_NO_WARNINGS)
    # Force to always compile with W4
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4512")
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX OR CMAKE_COMPILER_IS_CLANGCC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()

set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install")


add_subdirectory ( src )
add_subdirectory ( examples )

########### install files ###############

install(FILES LICENSE.QPL AUTHORS INSTALL COPYING CHANGELOG DESTINATION share/doc/ne7ssh)
install(DIRECTORY doc DESTINATION share/doc/ne7ssh)
install(DIRECTORY doc/html DESTINATION share/doc/ne7ssh)
