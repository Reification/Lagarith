#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <cinttypes>
#include <memory>
#include <vector>
#include <algorithm>
#include <string>

#if defined(_WINDOWS)
#	if !defined(_WIN64)
#		error "Only x64 windows config supported."
#	endif
#	define LAGARITH_WINDOWS
#	define LAGARITH_X64
#elif defined(__ANDROID__)
#	if !defined(__aarch64__)
#		error "Only arm64 android config supported."
#	endif
#	define LAGARITH_ANDROID
#	define LAGARITH_ARM64
#elif defined(__OSX__)
#	if !defined(__x86_64__)
#		error "Only x64 OSX config supported."
#	endif
#	define LAGARITH_OSX
#	define LAGARITH_X64
#else
#	error "Unsupported OS"
#endif

#if defined(_WINDOWS)
#	define LAGARITH_MULTITHREAD_SUPPORT 1
#	define WIN32_LEAN_AND_MEAN
#	include <windows.h>
#	include <direct.h>
#	define PATH_SEP '\\'
#	undef min
#	undef max
#else
#	define LAGARITH_MULTITHREAD_SUPPORT 0
#	include <unistd.h>
#	define PATH_SEP '/'
#	define __assume(A)
#	define _ftelli64 ftello
#	define _fseeki64 fseeko
#	define memcpy_s(a, b, c, d) memcpy(a, c, (b < d) ? b : d)
#	define sprintf_s sprintf
inline int fopen_s(FILE** pfp, const char* path, const char* mode) {
	*pfp = fopen(path, mode);
	return *pfp ? 0 : -1;
}
#endif
