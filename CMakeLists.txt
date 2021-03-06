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

SET(SOURCES main.c listen.c client.c utils.c file.c auth.c api.c gethandlers.c)
IF(TLS_SUPPORT)
	SET(SOURCES ${SOURCES} tls.c)
	ADD_DEFINITIONS(-DHAVE_TLS)
ENDIF()

CHECK_FUNCTION_EXISTS(getspnam HAVE_SHADOW)
IF(HAVE_SHADOW)
    ADD_DEFINITIONS(-DHAVE_SHADOW)
ENDIF()

ADD_EXECUTABLE(woodbox-server ${SOURCES})
FIND_LIBRARY(libjson NAMES json-c json)
TARGET_LINK_LIBRARIES(woodbox-server ubox dl ${libjson} ${LIBS})

SET(PLUGINS "")

IF(UBUS_SUPPORT)
	SET(PLUGINS ${PLUGINS} uhttpd_ubus)
	ADD_DEFINITIONS(-DHAVE_UBUS)
	ADD_LIBRARY(uhttpd_ubus MODULE ubus.c)
	FIND_LIBRARY(libjson NAMES json-c json)
	TARGET_LINK_LIBRARIES(uhttpd_ubus ubus ubox blobmsg_json ${libjson})
ENDIF()

IF(PLUGINS)
	SET_TARGET_PROPERTIES(${PLUGINS} PROPERTIES
		PREFIX ""
	)
ENDIF()

INSTALL(TARGETS woodbox-server ${PLUGINS}
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
)