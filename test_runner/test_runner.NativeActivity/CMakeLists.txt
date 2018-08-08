#see also GLES_VERSION in Packaging/CMakeLists.txt
set(LINK_LIBS EGL GLESv1_CM lagarith_test)
set(SLN_FOLDER "Testing/")
set(INCLUDE_DIRS "${PROJECT_SOURCE_DIR}")

#
# nothing below here will need editing in most cases
#

set(CMAKE_CXX_STANDARD "14")

get_filename_component(SUBPROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
string(REPLACE ".NativeActivity" "" LIB_NAME ${SUBPROJECT_NAME})

file(GLOB LIB_SRC "*.c" "*.cpp")
file(GLOB LIB_HDR "*.h" "*.hpp")

#androidproj can only contain files that are actually getting packaged or that drive the packaging process.
#the config sources and cmake file must live elsewhere, so the native activity holds them.
file(GLOB PKG_FILES  "../*.Packaging/*.in"
                     "../*.Packaging/*.txt"
                     "../*.Packaging/res/*/*.*")

add_library(${SUBPROJECT_NAME} SHARED ${LIB_SRC} ${LIB_HDR} ${PKG_FILES})

set_property(TARGET ${SUBPROJECT_NAME} PROPERTY OUTPUT_NAME "${LIB_NAME}")
set_target_properties(${SUBPROJECT_NAME} PROPERTIES FOLDER ${SLN_FOLDER}${LIB_NAME})

#put the packaging project generation inputs into a subfolder for clarity
source_group("Packaging Inputs" FILES ${PKG_FILES})

# used by PackagingProject to configure AndroidManifest.xml
# specify native activity library name and
# append target name to list of packaging dependencies
set_property(GLOBAL PROPERTY NATIVE_ACTIVITY_LIB_NAME "${LIB_NAME}")
set_property(GLOBAL APPEND PROPERTY ANDROID_PKG_DEPS ${SUBPROJECT_NAME})

target_include_directories(
 ${SUBPROJECT_NAME}
 PRIVATE
 ${CMAKE_CURRENT_SOURCE_DIR}
 ${INCLUDE_DIRS}
)

target_link_libraries(
  ${SUBPROJECT_NAME}
  PRIVATE
  ${LINK_LIBS}
)
