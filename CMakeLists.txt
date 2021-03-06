cmake_minimum_required(VERSION 3.11)

project(lagarith LANGUAGES C CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# All binary outputs use the same output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Only keep build products from the current configuration
foreach(BUILD_CONFIG ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER ${BUILD_CONFIG} BUILD_SUFFIX)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${BUILD_SUFFIX} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${BUILD_SUFFIX} ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${BUILD_SUFFIX} ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
endforeach(BUILD_CONFIG)

set(BUILD_TEST FALSE CACHE BOOL "Build simple manually run test")

set(BUILD_TEST_AS_LIB FALSE)

set(LIB_SRC_DIR "src")
set(API_HDR_DIR "include")

set(THIRD_PARTY_DIR "third_party")

if(WIN32)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -arch:AVX2")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
elseif(ANDROID)
  set(BUILD_TEST_AS_LIB TRUE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")
elseif(APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -arch:AVX2")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")
	message(FATAL_ERROR "OSX support not yet implemented.")
else()
  message(FATAL_ERROR "Only windows and android 64-bit configs supported.")
endif()

if(NOT ("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8"))
	message(FATAL_ERROR "Only windows and android 64-bit configuraitons supported")
endif()

set(INSTALL_TO_REIFY_OMNIBUS FALSE CACHE BOOL "Set install location for iterative testing with ReifyOminbus project")

#if we want to iterate on this library in ReifyOmnibus install it to the unpacked 3rd party location for that project
if(INSTALL_TO_REIFY_OMNIBUS AND (NOT "${CMAKE_INSTALL_PREFIX}" MATCHES "ReifyOmnibus"))
	if(WIN32)
		set(_PLAT "win64")
	elseif(ANDROID)
		set(_PLAT "android")
	endif()
  	set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/../ReifyOmnibus/build/${_PLAT}/ext" CACHE STRING "" FORCE)
	unset(_PLAT)
#if we have the default install prefix (which we will never use) set to a reasonable test location
elseif((NOT CMAKE_INSTALL_PREFIX) OR ("${CMAKE_INSTALL_PREFIX}" MATCHES "/Program Files"))
	set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install CACHE STRING "" FORCE)
endif()

file(GLOB LIB_SRC "${LIB_SRC_DIR}/*.cpp" "${LIB_SRC_DIR}/*.inl")
file(GLOB LIB_HDR "${LIB_SRC_DIR}/*.h" "${LIB_SRC_DIR}/*.hpp" "${THIRD_PARTY_DIR}/sse2neon/SSE2NEON.h")

file(GLOB API_HDR "${API_HDR_DIR}/*.h")

include_directories(  BEFORE 
                      ${LIB_SRC_DIR} 
                      ${API_HDR_DIR} 
                      ${THIRD_PARTY_DIR})

add_library(${PROJECT_NAME} STATIC ${LIB_SRC} ${LIB_HDR} ${API_HDR})

source_group("API" FILES ${API_HDR})
source_group("Android" FILES "${THIRD_PARTY_DIR}/sse2neon/SSE2NEON.h" "${LIB_SRC_DIR}/lag_sse2neon_ext.h")

if(BUILD_TEST)
	set(TEST_SRC_DIR "test")
	set(TEST_DATA_DIR "${TEST_SRC_DIR}/data")
	set(TEST_NAME "${PROJECT_NAME}_test")
	set(TEST_BIN_DIR "${CMAKE_BINARY_DIR}/bin/test_data")

	file(GLOB TEST_SRC	"${TEST_SRC_DIR}/*.c" "${TEST_SRC_DIR}/*.h" "${TEST_SRC_DIR}/*.cpp" "${TEST_SRC_DIR}/*.hpp"
											"${THIRD_PARTY_DIR}/stb/stb.h" "${THIRD_PARTY_DIR}/stb/stb_image.h" "${THIRD_PARTY_DIR}/stb/stb_image_write.h")

	if(BUILD_TEST_AS_LIB)
		add_library(${TEST_NAME} STATIC ${TEST_SRC})
		target_compile_definitions(${TEST_NAME} PRIVATE LAGARITH_TEST_IS_LIB)
	else()
		add_executable(${TEST_NAME} ${TEST_SRC})
	endif()

	target_link_libraries(${TEST_NAME} ${PROJECT_NAME})
	set_target_properties(${TEST_NAME} PROPERTIES FOLDER "Testing")

	file(GLOB TEST_DATA RELATIVE "${CMAKE_SOURCE_DIR}/${TEST_DATA_DIR}" "${TEST_DATA_DIR}/*.*")

	make_directory(${TEST_BIN_DIR})
	foreach(TEST_FILE ${TEST_DATA})
		#message(STATUS "copying ${TEST_FILE} to ${TEST_BIN_DIR}")
		configure_file("${TEST_DATA_DIR}/${TEST_FILE}" "${TEST_BIN_DIR}/${TEST_FILE}" COPYONLY)
	endforeach()
	
	add_subdirectory("test_runner")
endif()

install(FILES $<TARGET_FILE:${PROJECT_NAME}> DESTINATION lib)
install(FILES ${API_HDR} DESTINATION include/${PROJECT_NAME})