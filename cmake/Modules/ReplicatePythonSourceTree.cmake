# Note: when executed in the build dir, then CMAKE_CURRENT_SOURCE_DIR is the
# build dir.
file( COPY setup.py u1dbcipher  DESTINATION "${CMAKE_ARGV3}"
  FILES_MATCHING
  PATTERN "*.py"
  PATTERN "*.pyx")
file( COPY "MANIFEST.in" "README"
    "COPYING" "COPYING.LESSER" DESTINATION "${CMAKE_ARGV3}")
