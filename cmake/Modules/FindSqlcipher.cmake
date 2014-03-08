# - Try to find Sqlcipher
# Once done, this will define
#
#  Sqlcipher_FOUND - system has Sqlcipher
#  Sqlcipher_INCLUDE_DIRS - the Sqlcipher include directories
#  Sqlcipher_LIBRARIES - link these to use Sqlcipher

find_package(PkgConfig)
pkg_check_modules(PC_SQLCIPHER sqlcipher)

FIND_PATH(Sqlcipher_INCLUDE_DIR sqlite3.h
  HINTS
  ${PC_SQLICIPHER_INCLUDE_DIRS}
  /usr/include
  /usr/local/include
  /opt/local/include
)

FIND_LIBRARY(Sqlcipher_LIBRARY
  NAMES ${Sqlcipher_NAMES} libsqlcipher.so libsqlcipher.dylib
  HINTS ${PC_SQLCIPHER_LIBRARY_DIRS}
  /usr/lib /usr/local/lib /opt/local/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Sqlcipher DEFAULT_MSG Sqlcipher_LIBRARY Sqlcipher_INCLUDE_DIR)

IF(Sqlcipher_FOUND)
  SET(Sqlcipher_LIBRARIES ${Sqlcipher_LIBRARY})
  SET(Sqlcipher_INCLUDE_DIRS ${Sqlcipher_INCLUDE_DIR})
ENDIF(Sqlcipher_FOUND)
