#support for older versions of cmake
IF(NOT DEFINED CMAKE_CURRENT_LIST_DIR)
	GET_FILENAME_COMPONENT(CMAKE_CURRENT_LIST_DIR ${CMAKE_CURRENT_LIST_FILE} PATH)
ENDIF (NOT DEFINED CMAKE_CURRENT_LIST_DIR)

#setting global project root
IF(NOT DEFINED ROOT_DIR)
	SET(ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../")
ENDIF(NOT DEFINED ROOT_DIR)

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${ROOT_DIR}/cmake/")

#setting bunch of compile flags
SET (FLAGS_DEFAULT "-fPIC -pipe")
SET (FLAGS_WARNING 	"-Wall -Werror -Wextra -pedantic -Winit-self -Wconversion")
SET (FLAGS_CXX_LANG "-std=c++11")
SET (FLAGS_WARNING_CXX "${FLAGS_WARNING} -Weffc++ -Wold-style-cast -Woverloaded-virtual -Wshadow -Wctor-dtor-privacy -Wnon-virtual-dtor")
SET (FLAGS_RELEASE "-g -O3")
SET (FLAGS_DEBUG "-ggdb -O3")
MESSAGE("Build type is ${CMAKE_BUILD_TYPE}")

#support for add_subdirectory with a lot of warnings
function(disable_warnings)
	SET (CMAKE_C_FLAGS_DEBUG "${FLAGS_DEFAULT} ${FLAGS_DEBUG}" PARENT_SCOPE)
	SET (CMAKE_C_FLAGS_RELEASE "${FLAGS_DEFAULT} ${FLAGS_RELEASE}" PARENT_SCOPE)
	SET (CMAKE_C_FLAGS_RELWITHDEBINFO "${FLAGS_DEFAULT} ${FLAGS_DEBUG} ${FLAGS_RELEASE}" PARENT_SCOPE)
	SET (CMAKE_CXX_FLAGS_DEBUG "${FLAGS_DEFAULT} ${FLAGS_DEBUG} ${FLAGS_CXX_LANG} -Wno-deprecated-declarations -Wno-deprecated" PARENT_SCOPE)
	SET (CMAKE_CXX_FLAGS_RELEASE "${FLAGS_DEFAULT} ${FLAGS_RELEASE} ${FLAGS_CXX_LANG} -Wno-deprecated-declarations -Wno-deprecated" PARENT_SCOPE)
	SET (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${FLAGS_DEFAULT} ${FLAGS_RELEASE} ${FLAGS_CXX_LANG} -Wno-deprecated-declarations -Wno-deprecated" PARENT_SCOPE)
endfunction(disable_warnings)

function(enable_warnings)
	SET (CMAKE_C_FLAGS_DEBUG "${FLAGS_DEFAULT} ${FLAGS_WARNING} ${FLAGS_DEBUG}" PARENT_SCOPE)
	SET (CMAKE_C_FLAGS_RELEASE "${FLAGS_DEFAULT} ${FLAGS_WARNING} ${FLAGS_RELEASE}" PARENT_SCOPE)
	SET (CMAKE_C_FLAGS_RELWITHDEBINFO "${FLAGS_DEFAULT} ${FLAGS_WARNING} ${FLAGS_DEBUG} ${FLAGS_RELEASE}" PARENT_SCOPE)

	SET (CMAKE_CXX_FLAGS_DEBUG "${FLAGS_DEFAULT} ${FLAGS_WARNING_CXX} ${FLAGS_DEBUG} ${FLAGS_CXX_LANG}" PARENT_SCOPE)
	SET (CMAKE_CXX_FLAGS_RELEASE "${FLAGS_DEFAULT} ${FLAGS_WARNING_CXX} ${FLAGS_RELEASE} ${FLAGS_CXX_LANG}" PARENT_SCOPE)
	SET (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${FLAGS_DEFAULT} ${FLAGS_WARNING_CXX} ${FLAGS_RELEASE} ${FLAGS_CXX_LANG}" PARENT_SCOPE)
endfunction(enable_warnings)

#enabling all warnings by default
enable_warnings()

#default build type
IF (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  SET (CMAKE_BUILD_TYPE RELEASE)
  SET (CMAKE_BUILD_TYPE RELEASE CACHE STRING "Build type" FORCE)
ENDIF (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)

#some os x specific shit
IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	ADD_DEFINITIONS(-D_GLIBCXX_USE_NANOSLEEP)
	SET(CMAKE_OSX_SYSROOT "")
	SET(CMAKE_OSX_DEPLOYMENT_TARGET "")
	SET(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-isystem")
	INCLUDE_DIRECTORIES(
		SYSTEM
		/opt/local/include
		/usr/local/include
	)
	LINK_DIRECTORIES(/opt/local/lib)
	LINK_DIRECTORIES(/opt/local/lib/mysql55/mysql/)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")


INCLUDE_DIRECTORIES("${ROOT_DIR}")
INCLUDE_DIRECTORIES("${ROOT_DIR}/contrib/mysql-connector")
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/")
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/src/")

SET (LIBDIR lib)
IF (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
  SET (LIBDIR lib64)
ENDIF (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")

# Make FIND_LIBRARY search for static libs first and make it search inside lib64/
# directory in addition to the usual lib/ one.

SET (CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_STATIC_LIBRARY_SUFFIX} ${CMAKE_SHARED_LIBRARY_SUFFIX})
SET (CMAKE_FIND_LIBRARY_PREFIXES ${CMAKE_STATIC_LIBRARY_PREFIX} ${CMAKE_SHARED_LIBRARY_PREFIX})
SET (FIND_LIBRARY_USE_LIB64_PATHS TRUE)
SET (LINK_SEARCH_END_STATIC TRUE)

# Include source tree root, include directory inside it and build tree root,
# which is for files, generated by cmake from templates (e.g. autogenerated
# C/C++ includes).

INCLUDE_DIRECTORIES (${PROJECT_BINARY_DIR})
INCLUDE_DIRECTORIES (${PROJECT_SOURCE_DIR})

###############################################################################
# USE_PROGRAM (bin)
# -----------------------------------------------------------------------------
# Find program [bin] using standard FIND_PROGRAM command and save its path into
# variable named BIN_[bin].

MACRO (USE_PROGRAM bin)
  FIND_PROGRAM (BIN_${bin} ${bin})
  IF (BIN_${bin})
	MESSAGE (STATUS "FOUND ${BIN_${bin}}")
  ELSE ()
	MESSAGE (STATUS "ERROR ${BIN_${bin}}")
  ENDIF ()
ENDMACRO (USE_PROGRAM)

# USE_INCLUDE (inc [FIND_PATH_ARGS ...])
# -----------------------------------------------------------------------------
# Find include [inc] using standard FIND_PATH command and save its dirname into
# variable named INC_[inc]. Also include its dirname into project.

MACRO (USE_INCLUDE inc)
  FIND_PATH (INC_${inc} ${inc} ${ARGN})
  IF (INC_${inc})
	MESSAGE (STATUS "FOUND ${INC_${inc}}/${inc}")  # SHOULD BE BOLD GREEN
	INCLUDE_DIRECTORIES (${INC_${inc}})
  ELSE ()
	MESSAGE (STATUS "ERROR ${INC_${inc}}/${inc}")  # SHOULD BE BOLD RED
  ENDIF ()
ENDMACRO (USE_INCLUDE)

# USE_LIBRARY (lib [FIND_LIBRARY_ARGS ...])
# -----------------------------------------------------------------------------
# Find library [lib] using standard FIND_LIBRARY command and save its path into
# variable named LIB_[lib].

MACRO (USE_LIBRARY lib)
  FIND_LIBRARY (LIB_${lib} ${lib} ${ARGN})
  IF (LIB_${lib})
	MESSAGE (STATUS "FOUND ${LIB_${lib}}")  # SHOULD BE BOLD GREEN
  ELSE ()
	MESSAGE (STATUS "ERROR ${LIB_${lib}}")  # SHOULD BE BOLD RED
  ENDIF ()
ENDMACRO (USE_LIBRARY)

# USE_PACKAGE (var lib inc [FIND_PATH_ARGS ...])
# -----------------------------------------------------------------------------
# Find package using USE_LIBRARY and USE_INCLUDE macros.

MACRO (USE_PACKAGE lib inc)
  USE_LIBRARY (${lib} ${ARGN})
  USE_INCLUDE (${inc} ${ARGN})
ENDMACRO (USE_PACKAGE)

MACRO (USE_PACKAGE_STATIC lib inc)
  SET (CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_STATIC_LIBRARY_SUFFIX})
  USE_PACKAGE (${lib} ${inc} ${ARGN})
  SET (CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_STATIC_LIBRARY_SUFFIX} ${CMAKE_SHARED_LIBRARY_SUFFIX})
ENDMACRO (USE_PACKAGE_STATIC)

MACRO (USE_PACKAGE_SHARED lib inc)
  SET (CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_SHARED_LIBRARY_SUFFIX})
  USE_PACKAGE (${lib} ${inc} ${ARGN})
  SET (CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_STATIC_LIBRARY_SUFFIX} ${CMAKE_SHARED_LIBRARY_SUFFIX})
ENDMACRO (USE_PACKAGE_SHARED)

MACRO (USE_SUBPATH var sub)
  FIND_PATH (${var}_PREFIX ${sub} ONLY_CMAKE_FIND_ROOT_PATH)
  IF (${var}_PREFIX)
	GET_FILENAME_COMPONENT (${var} "${${var}_PREFIX}/${sub}" PATH)
	MESSAGE (STATUS "FOUND ${var}=${${var}}")
  ELSE (${var}_PREFIX)
	MESSAGE (STATUS "ERROR ${var}")
  ENDIF (${var}_PREFIX)
ENDMACRO (USE_SUBPATH)

###############################################################################
# MAKE_LIBRARY (apath <SHARED|STATIC> [LIBRARIES_TO_LINK_WITH [...]])
# -----------------------------------------------------------------------------
# Make library of SHARED or STATIC type from source code inside the [apath]
# subfolder and install it and all header files from the subfolder.

MACRO (MAKE_LIBRARY apath atype)
  GET_FILENAME_COMPONENT (${apath}_NAME "${apath}" NAME)
  AUX_SOURCE_DIRECTORY (${apath} SRC_${${apath}_NAME})
  ADD_LIBRARY (${${apath}_NAME} ${atype} ${SRC_${${apath}_NAME}})
  IF (${ARGC} GREATER 2)
	TARGET_LINK_LIBRARIES (${${apath}_NAME} ${ARGN})
  ENDIF (${ARGC} GREATER 2)
  # TODO SET_TARGET_PROPERTIES (...)
  INSTALL (TARGETS ${${apath}_NAME} DESTINATION ${LIBDIR})
  INSTALL (DIRECTORY ${apath} DESTINATION include FILES_MATCHING PATTERN "*.h")
  INSTALL (DIRECTORY ${apath} DESTINATION include FILES_MATCHING PATTERN "*.hpp")
  INSTALL (DIRECTORY ${apath} DESTINATION include FILES_MATCHING PATTERN "*.tcc")
ENDMACRO (MAKE_LIBRARY)

# MAKE_SHARED (apath [LIBRARIES_TO_LINK_WITH [...]])
# -----------------------------------------------------------------------------
# Make SHARED library with MAKE_LIBRARY macro.

MACRO (MAKE_SHARED apath)
  MAKE_LIBRARY (${apath} SHARED ${ARGN})
ENDMACRO (MAKE_SHARED)

# MAKE_STATIC (apath [LIBRARIES_TO_LINK_WITH [...]])
# -----------------------------------------------------------------------------
# Make STATIC library with MAKE_LIBRARY macro.

MACRO (MAKE_STATIC apath)
  MAKE_LIBRARY (${apath} STATIC ${ARGN})
ENDMACRO (MAKE_STATIC)

# MAKE_PROGRAM (apath)
# -----------------------------------------------------------------------------
# Make program (executable) from source code inside the [apath] subfolder and
# install it.

MACRO (MAKE_PROGRAM apath)
  GET_FILENAME_COMPONENT (${apath}_NAME "${apath}" NAME)
  AUX_SOURCE_DIRECTORY (${apath} SRC_${${apath}_NAME})
  ADD_EXECUTABLE (${${apath}_NAME} ${SRC_${${apath}_NAME}})
  IF (${ARGC} GREATER 1)
	TARGET_LINK_LIBRARIES (${${apath}_NAME} ${ARGN})
  ENDIF (${ARGC} GREATER 1)
  INSTALL (TARGETS ${${apath}_NAME} DESTINATION bin)
ENDMACRO (MAKE_PROGRAM)

# MAKE_TEST (apath)
# -----------------------------------------------------------------------------
# Make test from source code inside the [apath] subfolder.

MACRO (MAKE_TEST apath)
  GET_FILENAME_COMPONENT (${apath}_NAME "${apath}" NAME)
  AUX_SOURCE_DIRECTORY (${apath} SRC_test_${${apath}_NAME})
  ADD_EXECUTABLE (test_${${apath}_NAME} ${SRC_test_${${apath}_NAME}})
  IF (${ARGC} GREATER 1)
	TARGET_LINK_LIBRARIES (test_${${apath}_NAME} ${ARGN})
  ENDIF (${ARGC} GREATER 1)
  ADD_TEST (test_${${apath}_NAME} test_${${apath}_NAME}})
ENDMACRO (MAKE_TEST)

# INSTALL_TEMPLATE (sub [INSTALL_ARGS [...]])
# -----------------------------------------------------------------------------
# Install template files (*.in) with one line of code, all arguments except the
# first one will be left untouched and proxied to INSTALL (FILES) call.

MACRO (INSTALL_TEMPLATE sub)
  STRING (REGEX REPLACE "\\.in$" "" ${sub}_NOIN ${sub})
  CONFIGURE_FILE (${sub} ${PROJECT_BINARY_DIR}/auto/${${sub}_NOIN})
  INSTALL (FILES ${PROJECT_BINARY_DIR}/auto/${${sub}_NOIN} ${ARGN})
ENDMACRO (INSTALL_TEMPLATE)

###############################################################################
# GET_LOCALTIME (var [format [tmzone]])
# -----------------------------------------------------------------------------
# Print system date and time regarding to specified [format] and [tmzone]. If
# either [format] or [tmzone] is omitted, the default settings for the current
# locale will take the place.
# TODO make variadic.

#MACRO (GET_LOCALTIME var format tmzone)
#  SET_IF_NOT_SET (o_format "${format}")
#  SET_IF_NOT_SET (o_format "%c")
#  SET_IF_NOT_SET (o_tmzone "${tmzone}")
#  SET_IF_NOT_NIL (o_tmzone "-d'now GMT${o_tmzone}'")
#  ADD_CUSTOM_COMMAND (OUTPUT var COMMAND "date +'${o_format}' ${o_tmzone}")
#ENDMACRO (GET_LOCALTIME)

###############################################################################
function(PROTOBUF_GENERATE_CPP_CUSTOM SRCS HDRS WDIR)
	get_filename_component(WDIR ${WDIR} ABSOLUTE)
	MESSAGE("${WDIR}")
	if(NOT ARGN)
		message(SEND_ERROR "Error: PROTOBUF_GENERATE_CPP() called without any proto files")
		return()
	endif()

	if(PROTOBUF_GENERATE_CPP_APPEND_PATH)
		# Create an include path for each file specified
		foreach(FIL ${ARGN})
			get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
			get_filename_component(ABS_PATH ${ABS_FIL} PATH)
			list(FIND _protobuf_include_path ${ABS_PATH} _contains_already)
			if(${_contains_already} EQUAL -1)
				list(APPEND _protobuf_include_path -I ${ABS_PATH})
			endif()
		endforeach()
	else()
		set( _protobuf_include_path -I "${WDIR}")
	endif()
	MESSAGE("${WDIR}")

	if(DEFINED PROTOBUF_IMPORT_DIRS)
		foreach(DIR ${PROTOBUF_IMPORT_DIRS})
			get_filename_component(ABS_PATH ${DIR} ABSOLUTE)
			list(FIND _protobuf_include_path ${ABS_PATH} _contains_already)
			if(${_contains_already} EQUAL -1)
				list(APPEND _protobuf_include_path -I ${ABS_PATH})
			endif()
		endforeach()
	endif()

	set(${SRCS})
	set(${HDRS})
	foreach(FIL ${ARGN})
		get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
		string(REPLACE ".proto" "" FIL_WE ${FIL})

		list(APPEND ${SRCS} "${FIL_WE}.pb.cc")
		list(APPEND ${HDRS} "${FIL_WE}.pb.h")

		add_custom_command(
			OUTPUT "${FIL_WE}.pb.cc"
			"${FIL_WE}.pb.h"
			COMMAND protoc
			ARGS --cpp_out  ${WDIR} ${_protobuf_include_path} ${ABS_FIL}
			DEPENDS ${ABS_FIL}
			COMMENT "Running C++ protocol buffer compiler on ${FIL}"
			VERBATIM )
	endforeach()
	set_source_files_properties(${${SRCS}} ${${HDRS}} PROPERTIES GENERATED TRUE)
	set(${SRCS} ${${SRCS}} PARENT_SCOPE)
	set(${HDRS} ${${HDRS}} PARENT_SCOPE)
endfunction()

#####################################################################################################
FUNCTION(CTPP_COMPILE TARGET IN OUT)
  STRING(REGEX REPLACE "/" "_" CTPP_TARGET "${IN}")
  MESSAGE("Creating CTPP target with name ${CTPP_TARGET}")
  ADD_CUSTOM_TARGET(
    ${CTPP_TARGET}
    COMMAND ctpp2c ${IN} ${OUT}
    COMMENT "Running ctpp compiler on ${IN}"
    VERBATIM)
  ADD_DEPENDENCIES("${TARGET}" "${CTPP_TARGET}")
ENDFUNCTION()

FUNCTION(ADD_SUBDIR_ONCE DIR)
    STRING(REGEX REPLACE "/" "_" DIR_HASH "SUBDIR_${IN}")
    if(not defined {$DIR_HASH})
        set({$DIR_HASH} 1)
        add_subdirectory({$DIR})
    endif(defined {$DIR_HASH})
ENDFUNCTION()