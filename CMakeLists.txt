cmake_minimum_required(VERSION 2.7)
project(rediscpp)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/misc.cmake")
if(NOT DEFINED REDISCPP_SDIR)
	set(REDISCPP_SDIR "${ROOT_DIR}/src/")
endif(NOT DEFINED REDISCPP_SDIR)

use_package_shared(hiredis hiredis.h)
use_package_shared(cppunit hiredis.h)

SET(REDISCPP_SOURCE
    "${REDISCPP_SDIR}/log.cpp"
    "${REDISCPP_SDIR}/pool.cpp"
    "${REDISCPP_SDIR}/named_pool.cpp"
    "${REDISCPP_SDIR}/connection.cpp"
    "${REDISCPP_SDIR}/sharded_connection.cpp"
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

#cppunit is broken in brew in mac os x
if( NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
add_executable(test
    "${REDISCPP_SDIR}/tests/connection_test_abstract.cpp"
    "${REDISCPP_SDIR}/tests/connection_test_plain.cpp"
    "${REDISCPP_SDIR}/tests/run_tests.cpp"
)
set_target_properties(test PROPERTIES COMPILE_FLAGS "-Wno-effc++")

target_link_libraries(test cppunit rediscpp )
endif()

install(TARGETS rediscpp DESTINATION lib)
install(TARGETS rediscpp-static DESTINATION lib)
install(FILES
    "${REDISCPP_SDIR}/log.hpp"
    "${REDISCPP_SDIR}/pool.hpp"
    "${REDISCPP_SDIR}/named_pool.hpp"
    "${REDISCPP_SDIR}/debug.hpp"
    "${REDISCPP_SDIR}/macro.hpp"
    "${REDISCPP_SDIR}/holders.hpp"
    "${REDISCPP_SDIR}/redis.hpp"
    "${REDISCPP_SDIR}/connection.hpp"
    "${REDISCPP_SDIR}/sharded_connection.hpp"
    "${REDISCPP_SDIR}/pool_wrapper.hpp"
    "${REDISCPP_SDIR}/connection_param.hpp"
    "${REDISCPP_SDIR}/exception.hpp"
    DESTINATION include/rediscpp)