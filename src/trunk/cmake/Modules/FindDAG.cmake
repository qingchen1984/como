# - Find DAG
# Find the native DAG includes and library
#
#  DAG_INCLUDE_DIR - where to find dagapi.h, etc.
#  DAG_LIBRARIES   - List of libraries when using DAG.
#  DAG_FOUND       - True if DAG found.


IF (DAG_INCLUDE_DIR)
  # Already in cache, be silent
  SET(DAG_FIND_QUIETLY TRUE)
ENDIF (DAG_INCLUDE_DIR)

FIND_PATH(DAG_INCLUDE_DIR dagapi.h
  /usr/local/include
  /usr/include
)

SET(DAG_NAMES dag)
FIND_LIBRARY(DAG_LIBRARY
  NAMES ${DAG_NAMES}
  PATHS /usr/lib /usr/local/lib
)

IF (DAG_INCLUDE_DIR AND DAG_LIBRARY)
   SET(DAG_FOUND TRUE)
    SET( DAG_LIBRARIES ${DAG_LIBRARY} )
ELSE (DAG_INCLUDE_DIR AND DAG_LIBRARY)
   SET(DAG_FOUND FALSE)
   SET( DAG_LIBRARIES )
ENDIF (DAG_INCLUDE_DIR AND DAG_LIBRARY)

IF (DAG_FOUND)
   IF (NOT DAG_FIND_QUIETLY)
      MESSAGE(STATUS "Found DAG: ${DAG_LIBRARY}")
   ENDIF (NOT DAG_FIND_QUIETLY)
ELSE (DAG_FOUND)
   IF (DAG_FIND_REQUIRED)
      MESSAGE(STATUS "Looked for DAG library named ${DAGS_NAMES}.")
      MESSAGE(FATAL_ERROR "Could NOT find DAG library")
   ENDIF (DAG_FIND_REQUIRED)
ENDIF (DAG_FOUND)

MARK_AS_ADVANCED(
  DAG_LIBRARY
  DAG_INCLUDE_DIR
  )
