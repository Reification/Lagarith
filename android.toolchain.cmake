#include the toolchain file for generating visual studio projects using mobile c++ android workflow
#see https://github.com/Reification/CMake/releases if you want to generate for MSVS android c++ workflow
set(ANDROID_ABI arm64-v8a)
#set(ANDROID_CPP_FEATURES "exceptions")
set(ANDROID_NATIVE_API_LEVEL 26)
set(ANDROID_STL "c++_static")
set(ANDROID_TOOLCHAIN clang)

include("${CMAKE_ROOT}/msvs-android/android.toolchain.cmake")
