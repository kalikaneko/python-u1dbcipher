cmake_minimum_required (VERSION 2.6)
project (u1db)

# Some default CFLAGS
SET (CMAKE_C_FLAGS "-O2 -g -Wall -Werror")

# Allow specifying the libdir to use for the C library
SET (LIB_SUFFIX "" CACHE STRING "Define suffix of library directory name.")
SET (LIB_INSTALL_DIR "lib${LIB_SUFFIX}")

# Allow the developer to select if Dynamic or Static libraries are built
OPTION (BUILD_SHARED_LIBS "Build Shared Libraries" OFF)

#Set the LIB_TYPE variable to STATIC
SET (LIB_TYPE STATIC)
IF (BUILD_SHARED_LIBS)
  # User wants to build Dynamic Libraries, so change the LIB_TYPE variable to CMake keyword 'SHARED'
  SET (LIB_TYPE SHARED)
ENDIF (BUILD_SHARED_LIBS)

#add_custom_command (
#  OUTPUT ${CMAKE_SOURCE_DIR}/src/u1db_schema.c
#  COMMAND python ${CMAKE_SOURCE_DIR}/sql_to_c.py ${CMAKE_SOURCE_DIR}/u1dbcipher/backends/dbschema.sql u1db__schema ${CMAKE_SOURCE_DIR}/src/u1db_schema.c
#  DEPENDS ${CMAKE_SOURCE_DIR}/u1db/backends/dbschema.sql ${CMAKE_SOURCE_DIR}/sql_to_c.py
#  )
# XXX nah, just adding it from the u1db package

add_library (u1db ${LIB_TYPE} mkstemp_compat.c u1db.c u1db_http_sync_target.c u1db_query.c  u1db_sync_target.c  u1db_uuid.c  u1db_vectorclock.c u1db_schema.c)

SET_TARGET_PROPERTIES(u1db
    PROPERTIES SOVERSION 1
    COMPILE_FLAGS -fPIC
)

INSTALL (
    TARGETS u1db
    ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
    LIBRARY DESTINATION ${LIB_INSTALL_DIR}
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/u1db DESTINATION include
    FILES_MATCHING PATTERN "u1db.h"
)
