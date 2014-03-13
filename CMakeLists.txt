cmake_minimum_required(VERSION 2.7)
project(rediscpp)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/misc.cmake")
if(NOT DEFINED REDISCPP_SDIR)
	set(REDISCPP_SDIR "${ROOT_DIR}/src/")
endif(NOT DEFINED REDISCPP_SDIR)

use_package_shared(hiredis hiredis.h)

SET(REDISCPP_SOURCE
    "${REDISCPP_SDIR}/pool.cpp"
    "${REDISCPP_SDIR}/connection.cpp"
    "${REDISCPP_SDIR}/pool_wrapper.cpp"
    "${REDISCPP_SDIR}/connection_param.cpp"
    "${REDISCPP_SDIR}/exception.cpp"
)

add_library(rediscpp SHARED
    ${REDISCPP_SOURCE}
)

add_library(rediscpp-static STATIC
    ${REDISCPP_SOURCE}
)
set_target_properties(rediscpp-static PROPERTIES OUTPUT_NAME rediscpp)


target_link_libraries(rediscpp
    "${LIB_hiredis}"
)

add_executable(test
    "${REDISCPP_SDIR}/tests/tests.cpp"
    "${REDISCPP_SDIR}/tests/run_tests.cpp"
)

target_link_libraries(test rediscpp)

install(TARGETS rediscpp DESTINATION lib)
install(TARGETS rediscpp-static DESTINATION lib)
install(FILES
    "${REDISCPP_SDIR}/pool.hpp"
    "${REDISCPP_SDIR}/macro.hpp"
    "${REDISCPP_SDIR}/redis.hpp"
    "${REDISCPP_SDIR}/connection.hpp"
    "${REDISCPP_SDIR}/pool_wrapper.hpp"
    "${REDISCPP_SDIR}/connection_param.hpp"
    "${REDISCPP_SDIR}/exception.hpp"
    DESTINATION include/rediscpp)