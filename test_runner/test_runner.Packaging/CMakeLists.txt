#########################################
# set these variables for your project
#########################################

#used for java package identifiers in manifest, native activity java.in and source paths
set(DOMAIN_SFX com)
set(DOMAIN_NAME lagarith)
# used to configure strings.xml
set(HUMAN_READABLE_APP_NAME "Lagarith Test Runner")

#used to configure build.xml
#keystore settings only matter when RELEASE_ANT_TARGET is "release" instead of "debug"
set(RELEASE_ANT_TARGET "debug")
#set(KEYSTORE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/debug.keystore")
#set(KEYSTORE_PASSWORD DebugPassword)
#set(KEYSTORE_ALIAS "lagarith.com")
#version format is 32 bit hex
#high word=major_vers
#low word=minor_vers
#so 1.2 would be 0x00010002
set(GLES_VERSION "0x00030000") #see also LINK_LIBS in NativeActivity/CMakeLists.txt

set(SLN_FOLDER "Testing")

#if you have 3rd party prebuilt shared libs you need packaged and available for runtime loading
#create a directory libs/<ABI>/ and put them there.
#e.g. libs/arm64-v8a/

#all files under res/ will get included in the package

#########################################
# Everything below here derived from
# values above and inferred from
# files in this directory
#########################################

get_filename_component(SUBPROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

#set by NativeActivity cmakelists.txt
get_property(NATIVE_ACTIVITY_LIB_NAME GLOBAL PROPERTY NATIVE_ACTIVITY_LIB_NAME)

#accummulated in dependency cmake files and NativeActivity cmake file
get_property(PACKAGING_DEPS GLOBAL PROPERTY ANDROID_PKG_DEPS)

if(NOT NATIVE_ACTIVITY_LIB_NAME)
  message(FATAL_ERROR "NativeActivity target must be configured before Packaging target")
endif()

if(NOT PACKAGING_DEPS)
  message(FATAL_ERROR "NativeActivity and dependency targets must append target names to global property ANDROID_PKG_DEPS")
endif()

#generate file with list of tests to load (libraries)
#for pushing to device with test data
if(TRUE)
  set(ALL_TESTS_TEXT)
  foreach(TEST_NAME ${PACKAGING_DEPS})
    if(("${TEST_NAME}" MATCHES "Test") AND (NOT "${TEST_NAME}" MATCHES "NativeActivity"))
      set(ALL_TESTS_TEXT "${ALL_TESTS_TEXT}${TEST_NAME}\n")
    endif()
  endforeach(TEST_NAME)

  file(WRITE ${CMAKE_BINARY_DIR}/bin/all_tests.txt ${ALL_TESTS_TEXT})
  if(NOT EXISTS ${CMAKE_BINARY_DIR}/bin/run_tests.txt)
    file(WRITE ${CMAKE_BINARY_DIR}/bin/run_tests.txt ${ALL_TESTS_TEXT})
  endif()
  unset(ALL_TESTS_TEXT)
endif()

#*************** manifest setup ******************************
file(GLOB ACTIVITY_CLASS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*Activity.java.in")
if(ACTIVITY_CLASS)
  string(REPLACE ".java.in" "" ACTIVITY_CLASS "${ACTIVITY_CLASS}")
  set(ACTIVITY_CLASS "${DOMAIN_SFX}.${DOMAIN_NAME}.${ACTIVITY_CLASS}")
  set(HAS_JAVA_CODE "true")
else()
  set(ACTIVITY_CLASS "android.app.NativeActivity")
  set(HAS_JAVA_CODE "false")
endif()

set(MANIFEST_XML "AndroidManifest.xml")
set(PACKAGE_ID "${DOMAIN_SFX}.${DOMAIN_NAME}.${NATIVE_ACTIVITY_LIB_NAME}")
set(APP_ICON_ATTR)
file(GLOB ICON_IMAGE RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "res/drawable-*/*.png")
if(ICON_IMAGE)
  list(GET ICON_IMAGE 0 ICON_IMAGE)
  get_filename_component(ICON_IMAGE ${ICON_IMAGE} NAME)
  string(REPLACE ".png" "" ICON_IMAGE ${ICON_IMAGE})
  set(APP_ICON_ATTR "android:icon=\"@drawable/${ICON_IMAGE}\"")
else()
  message( WARNING " res/drawable-*/*.png not found. App icon will be default." )
endif()

configure_file( "${MANIFEST_XML}.in" 
                "${CMAKE_CURRENT_BINARY_DIR}/${MANIFEST_XML}" 
                @ONLY )
#***********************************************************

#************ ant build file setup *************************
set(BUILD_XML "build.xml")
#uses RELEASE_ANT_TARGET set at top of file
configure_file( "${BUILD_XML}.in" 
                "${CMAKE_CURRENT_BINARY_DIR}/${BUILD_XML}" 
                @ONLY )
#***********************************************************

#********** project properties setup ***********************
set(PROJECT_PROPS "project.properties")
configure_file( "${PROJECT_PROPS}.in" 
                "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_PROPS}" 
                @ONLY )
#***********************************************************

#********* strings xml setup *******************************
#strings are special resource
#uses HUMAN_READABLE_APP_NAME set at top of file
set(STRINGS_XML "res/values/strings.xml")
configure_file( "strings.xml.in" 
                "${CMAKE_CURRENT_BINARY_DIR}/${STRINGS_XML}" 
                @ONLY )
#***********************************************************


#***********************************************************
#
# configure java src and generate androidproj xml fragments
#
#***********************************************************

# sections of guid for generating guids that agree between internals of vcxproj and references
# these sections of guid are arbitrarily chosen and are not specifically magic
# the middle of the GUID gets incremented as new GUIDs are needed for each project
# the GUID is set into a cache variable to force assignment to a named target
# and then used to refer to that named target in the androidproj
# the guids must match at both ends of the reference and be unique per project 
# within a given set of vcxproj/androidproj files clumped into a sln file
set(GUID_PREFIX "AAAAAAAA-BBBB-CCCC")
set(GUID_MIDDLE 1111)
set(GUID_SUFFIX "ABCDEF012345")

#used to specify name of native activity lib in androidproj
set(ROOT_NAMESPACE "${NATIVE_ACTIVITY_LIB_NAME}")

file(GLOB ANDROIDPROJ_IN RELATIVE "${CURRENT_SOURCE_DIR}" "*.androidproj.in")
set(ANDROIDPROJ_OUT "${CMAKE_CURRENT_BINARY_DIR}/${SUBPROJECT_NAME}.androidproj")

#
# !!! DO NOT CHANGE THIS VALUE
# magic type guid that identifies an .androidproj properly in the generated solution file
#
#set(ANDROIDPROJ_TYPE_GUID "39E2626F-3545-4960-A6E8-258AD8476CE5")

#copy resource files to packaging project directory and accum list for .androidproj
file(GLOB RES_FILES LIST_DIRECTORIES false RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "res/*/*.*")
set(RES_CONTENT)
if(RES_FILES)
  set(RES_CONTENT "  <ItemGroup>")
  foreach(RES_FILE ${RES_FILES})
    configure_file("${RES_FILE}" "${CMAKE_CURRENT_BINARY_DIR}/${RES_FILE}" COPYONLY)
    set(RES_CONTENT "${RES_CONTENT}\n    <Content Include=\"${RES_FILE}\"/>")
  endforeach(RES_FILE)
  set(RES_CONTENT "${RES_CONTENT}\n  </ItemGroup>")
endif()

#configure java source to packaging project directory and accum list for .androidproj
file(GLOB JAVA_IN_FILES LIST_DIRECTORIES false RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.java.in")
set(JAVA_SOURCES)
if(JAVA_IN_FILES)
  foreach(JAVA_IN_FILE ${JAVA_IN_FILES})
    string(REPLACE ".in" "" JAVA_FILE "src/${DOMAIN_SFX}/${DOMAIN_NAME}/${JAVA_IN_FILE}")
    configure_file("${JAVA_IN_FILE}" "${CMAKE_CURRENT_BINARY_DIR}/${JAVA_FILE}" @ONLY)
    set(JAVA_SOURCES "${JAVA_SOURCES}\n    <Content Include=\"${JAVA_FILE}\">\n      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>\n    </Content>")
  endforeach(JAVA_IN_FILE)
endif()

#copy 3rd party libs to packaging project directory and accum list for .androidproj
file(GLOB LIB_FILES LIST_DIRECTORIES false RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "libs/*/*.*")
set(EXTRA_LIBS)
if(LIB_FILES)
  foreach(LIB_FILE ${LIB_FILES})
    configure_file("${LIB_FILE}" "${CMAKE_CURRENT_BINARY_DIR}/${LIB_FILE}" COPYONLY)
    set(EXTRA_LIBS "${EXTRA_LIBS}\n    <Content Include=\"${LIB_FILE}\">\n      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>\n    </Content>")
  endforeach(LIB_FILE)
endif()

#build list of references to native activity and shared lib projects
set(PROJECT_REFERENCES "  <ItemGroup>")
foreach(PACKAGING_DEP_TARGET ${PACKAGING_DEPS})
  get_target_property(REF_VCXPROJ ${PACKAGING_DEP_TARGET} BINARY_DIR)
  set(REF_VCXPROJ ${REF_VCXPROJ}/${PACKAGING_DEP_TARGET}.vcxproj)

  MATH(EXPR GUID_MIDDLE "${GUID_MIDDLE}+1")
  set(REF_GUID "${GUID_PREFIX}-${GUID_MIDDLE}-${GUID_SUFFIX}")

  #force the output guid for the target to be a particular value in the generated vcxproj and sln files
  #this value is also referenced below in generated XML for project reference from androidproj
  set(${PACKAGING_DEP_TARGET}_GUID_CMAKE ${REF_GUID} CACHE INTERNAL "Project GUID")

  set(PROJECT_REFERENCES "${PROJECT_REFERENCES}\n    <ProjectReference Include=\"${REF_VCXPROJ}\">\n      <Project>{${REF_GUID}}</Project>\n    </ProjectReference>")
endforeach(PACKAGING_DEP_TARGET)
set(PROJECT_REFERENCES "${PROJECT_REFERENCES}\n  </ItemGroup>")

#generate a GUID for the external project we're about to create via config_file
MATH(EXPR GUID_MIDDLE "${GUID_MIDDLE}+1")
set(ANDROIDPROJ_GUID "${GUID_PREFIX}-${GUID_MIDDLE}-${GUID_SUFFIX}")
set(SUBPROJECT_GUID_VAR ${SUBPROJECT_NAME}_GUID_CMAKE)

#put together all the pieces of the androidproj
configure_file( "${ANDROIDPROJ_IN}" 
                "${ANDROIDPROJ_OUT}" 
                @ONLY NEWLINE_STYLE WIN32 )

#include the generated androidproj in the solution, specifying the magic type GUID
include_external_msproject( ${SUBPROJECT_NAME}
                            "${ANDROIDPROJ_OUT}"
                            TYPE "${ANDROIDPROJ_TYPE_GUID}")

set_target_properties(${SUBPROJECT_NAME} PROPERTIES FOLDER "${SLN_FOLDER}/${NATIVE_ACTIVITY_LIB_NAME}")

