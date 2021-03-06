#
# This module finds if pkg-config is installed and determines where the
# executable is. This code sets the following variables:
#
#  PKGCONFIG_EXECUTABLE = the full path to pkg-config.
#  PKGCONFIG_FOUND      = true if pkg-config found.
#
# $Id$

IF (PKGCONFIG_EXECUTABLE)
  # Already in cache, be silent
  SET(PKGCONFIG_FIND_QUIETLY TRUE)
ENDIF (PKGCONFIG_EXECUTABLE)

FIND_PROGRAM(PKGCONFIG_EXECUTABLE NAMES pkg-config)

IF (PKGCONFIG_EXECUTABLE)
    SET(PKGCONFIG_FOUND TRUE)
ELSE (PKGCONFIG_EXECUTABLE)
    SET(PKGCONFIG_FOUND FALSE)
ENDIF (PKGCONFIG_EXECUTABLE)

IF (PKGCONFIG_FOUND)
   IF (NOT PKGCONFIG_FIND_QUIETLY)
      MESSAGE(STATUS "Found PKG-CONFIG: ${PKGCONFIG_EXECUTABLE}")
   ENDIF (NOT PKGCONFIG_FIND_QUIETLY)
ELSE (PKGCONFIG_FOUND)
    IF (NOT PKGCONFIG_FIND_QUIETLY)
        MESSAGE(STATUS "Looked for pkg-config executable.")
        IF (PKGCONFIG_FIND_REQUIRED)
            MESSAGE(FATAL_ERROR "Could NOT find pkg-config executable")
        ENDIF(PKGCONFIG_FIND_REQUIRED)
    ENDIF (NOT PKGCONFIG_FIND_QUIETLY)
ENDIF (PKGCONFIG_FOUND)

MARK_AS_ADVANCED(PKGCONFIG_EXECUTABLE)

