function(create_build_info NAME)

	# Set build platform; to be written to BuildInfo.h
	set(GDTU_BUILD_OS "${CMAKE_SYSTEM_NAME}")

	if (CMAKE_COMPILER_IS_MINGW)
		set(GDTU_BUILD_COMPILER "mingw")
	elseif (CMAKE_COMPILER_IS_MSYS)
		set(GDTU_BUILD_COMPILER "msys")
	elseif (CMAKE_COMPILER_IS_GNUCXX)
		set(GDTU_BUILD_COMPILER "g++")
	elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
		set(GDTU_BUILD_COMPILER "msvc")
	elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
		set(GDTU_BUILD_COMPILER "clang")
	elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
		set(GDTU_BUILD_COMPILER "appleclang")
	else ()
		set(GDTU_BUILD_COMPILER "unknown")
	endif ()

	set(GDTU_BUILD_PLATFORM "${GDTU_BUILD_OS}.${GDTU_BUILD_COMPILER}")

	#cmake build type may be not speCified when using msvc
	if (CMAKE_BUILD_TYPE)
		set(_cmake_build_type ${CMAKE_BUILD_TYPE})
	else()
		set(_cmake_build_type "${CMAKE_CFG_INTDIR}")
	endif()

	# Generate header file containing useful build information
	add_custom_target(${NAME}_BuildInfo.h ALL
		WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
		COMMAND ${CMAKE_COMMAND} -DGDTU_SOURCE_DIR="${PROJECT_SOURCE_DIR}" -DGDTU_BUILDINFO_IN="${GDTU_CMAKE_DIR}/templates/BuildInfo.h.in" -DGDTU_DST_DIR="${PROJECT_BINARY_DIR}/include/${PROJECT_NAME}" -DGDTU_CMAKE_DIR="${GDTU_CMAKE_DIR}"
		-DGDTU_BUILD_TYPE="${_cmake_build_type}"
		-DGDTU_BUILD_OS="${GDTU_BUILD_OS}"
		-DGDTU_BUILD_COMPILER="${GDTU_BUILD_COMPILER}"
		-DGDTU_BUILD_PLATFORM="${GDTU_BUILD_PLATFORM}"
		-DPROJECT_VERSION="${PROJECT_VERSION}"
		-P "${GDTU_SCRIPTS_DIR}/buildinfo.cmake"
		)
	include_directories(BEFORE ${PROJECT_BINARY_DIR})
endfunction()
