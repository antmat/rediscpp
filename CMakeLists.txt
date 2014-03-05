cmake_minimum_required(VERSION 2.7)
project(rediscpp)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/misc.cmake")
if(NOT DEFINED REDISCPP_SDIR)
	set(REDISCPP_SDIR "${ROOT_DIR}/src/")
endif(NOT DEFINED REDISCPP_SDIR)

use_package_shared(hiredis hiredis.h)

add_library(rediscpp SHARED
    "${REDISCPP_SDIR}/pool.cpp"
    "${REDISCPP_SDIR}/connection.cpp"
    "${REDISCPP_SDIR}/wrapper.cpp"
    "${REDISCPP_SDIR}/connection_param.cpp"
    "${REDISCPP_SDIR}/exception.cpp"
)


target_link_libraries(rediscpp
    "${LIB_hiredis}"
)

add_executable(test
    "${REDISCPP_SDIR}/tests/tests.cpp"
    "${REDISCPP_SDIR}/tests/run_tests.cpp"
)

target_link_libraries(test rediscpp)