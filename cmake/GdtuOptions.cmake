macro(configure_project)
	set(NAME ${PROJECT_NAME})

	# features
	gdtu_default_option(PROFILING OFF)

	# components
	gdtu_default_option(TESTS ON)
	gdtu_default_option(TOOLS ON)

	# Define a matching property name of each of the "features".
	foreach(FEATURE ${ARGN})
		set(SUPPORT_${FEATURE} TRUE)
	endforeach()

	include(GdtuBuildInfo)
	create_build_info(${NAME})
	print_config(${NAME})
endmacro()

macro(print_config NAME)
	message("")
	message("------------------------------------------------------------------------")
	message("-- Configuring ${NAME}")
	message("------------------------------------------------------------------------")
	message("--                  CMake Version                            ${CMAKE_VERSION}")
	message("-- CMAKE_BUILD_TYPE Build type                               ${CMAKE_BUILD_TYPE}")
	message("-- TARGET_PLATFORM  Target platform                          ${CMAKE_SYSTEM_NAME}")
	message("--------------------------------------------------------------- features")
	message("-- PROFILING        Profiling support                        ${PROFILING}")
	message("------------------------------------------------------------- components")
if (SUPPORT_TESTS)
	message("-- TESTS            Build tests                              ${TESTS}")
endif()
if (SUPPORT_TOOLS)
	message("-- TOOLS            Build tools                              ${TOOLS}")
endif()
	message("------------------------------------------------------------------------")
	message("")
endmacro()
