if(ANDROID AND BUILD_TEST AND CMAKE_GENERATOR MATCHES "Visual Studio")
  #native activity must come first!
  add_subdirectory(test_runner.NativeActivity)
  add_subdirectory(test_runner.Packaging)

  find_program(ADB adb "$ENV{ANDROID_HOME}/platform-tools")

  if(EXISTS "${ADB}")
    set(UTILITY_COMMAND_FOLDER "Testing/Android Utilities")

    set(PACKAGE_NAME "com.lagarith.test_runner")

    #in theory we should query envar EXTERNAL_STORAGE on device, but this is universally supporte - by symlink on device if nothing else
    set(DEVICE_STORAGE_ROOT "/sdcard")
    set(DEVICE_APP_DATA_ROOT "${DEVICE_STORAGE_ROOT}/Android/data/${PACKAGE_NAME}")

    add_custom_target(DeployTestData
                      COMMAND ${ADB} push ${CMAKE_BINARY_DIR}/bin/. ${DEVICE_APP_DATA_ROOT}/
                      COMMAND ${ADB} shell "run-as ${PACKAGE_NAME}")
    set_target_properties(DeployTestData PROPERTIES FOLDER "${UTILITY_COMMAND_FOLDER}")

    make_directory(${CMAKE_BINARY_DIR}/bin)

    file(WRITE ${CMAKE_BINARY_DIR}/test_out_log.txt)
    file(WRITE ${CMAKE_BINARY_DIR}/test_err_log.txt)

    set(LOG_FILES ${CMAKE_BINARY_DIR}/test_out_log.txt ${CMAKE_BINARY_DIR}/test_err_log.txt)

    add_custom_target(PullTestLogs SOURCES ${LOG_FILES}
                      COMMAND ${ADB} pull ${DEVICE_APP_DATA_ROOT}/test_out_log.txt ${CMAKE_BINARY_DIR}/test_out_log.txt
                      COMMAND ${ADB} pull ${DEVICE_APP_DATA_ROOT}/test_err_log.txt ${CMAKE_BINARY_DIR}/test_err_log.txt)

    source_group("Log Files" FILES ${LOG_FILES})
    set_target_properties(PullTestLogs PROPERTIES FOLDER "${UTILITY_COMMAND_FOLDER}")
  else()
    message(WARNING " adb not installed. DeployTestData target will not be added.")
  endif()
else()
  message( STATUS "test_runner only valid while building for Android with Visual Studio - skipping." )
endif()
