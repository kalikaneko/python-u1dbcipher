cmake_minimum_required (VERSION 2.6)
project (cu1db)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
include_directories(${CMAKE_SOURCE_DIR}/include)

execute_process(COMMAND python ${CMAKE_SOURCE_DIR}/setup.py --version
                OUTPUT_VARIABLE PROJECT_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)

set(ARCHIVE_NAME ${CMAKE_PROJECT_NAME}-${PROJECT_VERSION})
add_custom_target(dist
    COMMAND bzr export --root=${ARCHIVE_NAME}
        ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.bz2
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

find_package(Cython REQUIRED)

find_package(CURL REQUIRED)
if (CURL_FOUND)
  include_directories(${CURL_INCLUDE_DIRS})
endif (CURL_FOUND)

find_package(OAUTH REQUIRED)
if (OAUTH_FOUND)
  include_directories(${OAUTH_INCLUDE_DIRS})
endif (OAUTH_FOUND)

find_package(JSON REQUIRED)
if (JSON_FOUND)
  include_directories(${JSON_INCLUDE_DIRS})
endif (JSON_FOUND)

find_package(Sqlcipher REQUIRED)
if (Sqlcipher_FOUND)
  include_directories(${Sqlcipher_INCLUDE_DIRS})
endif (Sqlcipher_FOUND)

add_custom_target( ReplicatePythonSourceTree ALL ${CMAKE_COMMAND} -P
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/ReplicatePythonSourceTree.cmake
  ${CMAKE_CURRENT_BINARY_DIR}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

add_custom_target(build-inplace-for-tests
    COMMAND python setup.py build_ext -i -f -I ${CMAKE_SOURCE_DIR}/include -R ${CMAKE_BINARY_DIR}/src
    DEPENDS ReplicatePythonSourceTree cu1db
)

SET(CPACK_GENERATOR "TGZ")
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Noone")
INCLUDE(CPack)

add_custom_target(install-python ALL
    # Do NOT build this one in-place
    COMMAND python setup.py build_ext -f -I ${CMAKE_SOURCE_DIR}/include -R ${CMAKE_BINARY_DIR}/src
    COMMAND python setup.py install --prefix=${CMAKE_INSTALL_PREFIX} --root=${CMAKE_CURRENT_BINARY_DIR}/temp
    DEPENDS ReplicatePythonSourceTree cu1db
)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/temp/${CMAKE_INSTALL_PREFIX}/
    DESTINATION "."
    PATTERN "*.pyc" EXCLUDE
)

add_subdirectory(src)
