cmake_minimum_required(VERSION 3.11)

project(lagarith LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CONFIGURATION_TYPES "Debug" "Release" CACHE STRING "" FORCE)

#if we have the default install prefix (which we will never use) set to a reasonable test location
if("${CMAKE_INSTALL_PREFIX}" MATCHES "/Program Files")
  set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install CACHE STRING "" FORCE)
endif()

#set(BUILD_TESTS FALSE CACHE BOOL "Build simple manually run tests")
#set(BUILD_TESTS_AS_LIBS FALSE)

set(LIB_SRC_DIR "src")
set(API_HDR_DIR "include")

set(LINK_LIBS)

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
file(GLOB LIB_HDR "${LIB_SRC_DIR}/*.h")

file(GLOB API_HDR "${API_HDR_DIR}/*.h")

include_directories(  BEFORE 
                      ${LIB_SRC_DIR} 
                      ${API_HDR_DIR} )

add_library(${PROJECT_NAME} SHARED ${LIB_SRC} ${LIB_HDR} ${API_HDR})
#set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY COMPILE_FLAGS "-DBUILDING_MHD_LIB")

source_group("API" FILES ${API_HDR})

#if(BUILD_TESTS)
#  set(TESTS hellobrowser simplepost largepost)
#  foreach(TEST ${TESTS})
#    if(BUILD_TESTS_AS_LIBS)
#      add_library("${TEST}" SHARED doc/examples/${TEST}.c)
#    else()
#      add_executable("${TEST}" doc/examples/${TEST}.c)
#    endif()
#    set_target_properties("${TEST}" PROPERTIES FOLDER "Tests")
#    target_link_libraries("${TEST}" ${PROJECT_NAME} ${LINK_LIBS})
#  endforeach()
#endif()

install(FILES $<TARGET_FILE:${PROJECT_NAME}> DESTINATION lib)
install(FILES ${API_HDR} DESTINATION include/${PROJECT_NAME})