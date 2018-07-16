cmake_minimum_required(VERSION 3.11)

project(lagarith LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CONFIGURATION_TYPES "Debug" "Release" CACHE STRING "" FORCE)

#if we have the default install prefix (which we will never use) set to a reasonable test location
if("${CMAKE_INSTALL_PREFIX}" MATCHES "/Program Files")
  set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install CACHE STRING "" FORCE)
endif()

set(BUILD_TEST TRUE CACHE BOOL "Build simple manually run test")
set(BUILD_TEST_AS_LIB FALSE)

set(LIB_SRC_DIR "src")
set(API_HDR_DIR "include")

if(WIN32)
  add_definitions(-D -D_CRT_SECURE_NO_WARNINGS)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -arch:AVX2")
elseif(ANDROID)
  #set(API_PLAT_HDR_DIR "android")
  #set(LINK_LIBS "libsocket.a")
  #set(BUILD_TESTS_AS_LIBS TRUE)
elseif(APPLE)
  #set(API_PLAT_HDR_DIR "darwin")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -arch:AVX2")
else()
  message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} NYI")
endif()

file(GLOB LIB_SRC "${LIB_SRC_DIR}/*.cpp" "${LIB_SRC_DIR}/*.inl")
file(GLOB LIB_HDR "${LIB_SRC_DIR}/*.h" "${LIB_SRC_DIR}/*.hpp")

file(GLOB API_HDR "${API_HDR_DIR}/*.h")

include_directories(  BEFORE 
                      ${LIB_SRC_DIR} 
                      ${API_HDR_DIR} )

add_library(${PROJECT_NAME} STATIC ${LIB_SRC} ${LIB_HDR} ${API_HDR})

source_group("API" FILES ${API_HDR})

if(BUILD_TEST)
	set(TEST_SRC_DIR "test")
	set(TEST_DATA_DIR "${TEST_SRC_DIR}/data")
	set(TEST_NAME "${PROJECT_NAME}_test")
	set(TEST_BIN_DIR "${CMAKE_BINARY_DIR}/test_data")

	file(GLOB TEST_SRC "${TEST_SRC_DIR}/*.cpp" "${TEST_SRC_DIR}/*.h" "${TEST_SRC_DIR}/*.hpp")

	add_executable(${TEST_NAME} ${TEST_SRC})
	target_link_libraries(${TEST_NAME} ${PROJECT_NAME})
	set_target_properties(${TEST_NAME} PROPERTIES FOLDER "Tests")

	file(GLOB TEST_DATA RELATIVE "${CMAKE_SOURCE_DIR}/${TEST_DATA_DIR}" "${TEST_DATA_DIR}/*.*")

	make_directory(${TEST_BIN_DIR})
	foreach(TEST_FILE ${TEST_DATA})
		message(STATUS "copying ${TEST_FILE} to ${TEST_BIN_DIR}")
		configure_file("${TEST_DATA_DIR}/${TEST_FILE}" "${TEST_BIN_DIR}/${TEST_FILE}" COPYONLY)
	endforeach()
endif()

install(FILES $<TARGET_FILE:${PROJECT_NAME}> DESTINATION lib)
install(FILES ${API_HDR} DESTINATION include/${PROJECT_NAME})