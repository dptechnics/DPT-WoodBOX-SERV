cmake_minimum_required(VERSION 2.6)

PROJECT(woodbox-server C)

INCLUDE (CheckFunctionExists)

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
ADD_DEFINITIONS(-Os -Wall -Werror -Wmissing-declarations --std=gnu99 -g3)

OPTION(TLS_SUPPORT "TLS support" ON)
OPTION(UBUS_SUPPORT "ubus support" OFF)

IF(APPLE)
  INCLUDE_DIRECTORIES(/opt/local/include)
  LINK_DIRECTORIES(/opt/local/lib)
ENDIF()

FIND_LIBRARY(LIBS crypt)
IF(LIBS STREQUAL "LIBS-NOTFOUND")
	SET(LIBS "")
ENDIF()

SET(SOURCES main.c listen.c client.c utils.c file.c auth.c cgi.c relay.c proc.c api.c)
IF(TLS_SUPPORT)
	SET(SOURCES ${SOURCES} tls.c)
	ADD_DEFINITIONS(-DHAVE_TLS)
ENDIF()

CHECK_FUNCTION_EXISTS(getspnam HAVE_SHADOW)
IF(HAVE_SHADOW)
    ADD_DEFINITIONS(-DHAVE_SHADOW)
ENDIF()

ADD_EXECUTABLE(woodbox-server ${SOURCES})
TARGET_LINK_LIBRARIES(woodbox-server dl ${LIBS})

INSTALL(TARGETS woodbox-server
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
)