cmake_minimum_required(VERSION 2.7)
if(NOT DEFINED REDISCPP_SDIR)
	GET_FILENAME_COMPONENT(CMAKE_CURRENT_LIST_DIR ${CMAKE_CURRENT_LIST_FILE} PATH)
	set(REDISCPP_SDIR "${CMAKE_CURRENT_LIST_DIR}/rediscpp/")
	include_directories("${REDISCPP_SDIR}/../")
	message("RCPP_SDIR: ${REDISCPP_SDIR}")
	use_library_static(cppunit)

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

	add_library(rediscpp-static STATIC
		${REDISCPP_SOURCE}
	)

	#cppunit is broken in brew in mac os x
	if( NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	add_executable(test
		"${REDISCPP_SDIR}/tests/connection_test_abstract.cpp"
		"${REDISCPP_SDIR}/tests/connection_test_plain.cpp"
		"${REDISCPP_SDIR}/tests/run_tests.cpp"
	)
	set_target_properties(test PROPERTIES COMPILE_FLAGS "-Wno-effc++")

	target_link_libraries(test cppunit rediscpp-static hiredis)
	endif()
endif(NOT DEFINED REDISCPP_SDIR)
