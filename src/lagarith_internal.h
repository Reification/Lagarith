#pragma once

#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <cassert>
#include <cinttypes>
#include <memory>

#if defined(_WINDOWS)
#	if !defined(_WIN64)
#		error "Only x64 windows config supported."
#	endif
#elif defined(__ANDROID__)
#	if !defined(__aarch64__)
#		error "Only x64 android config supported."
#	endif
#elif defined(__OSX__)
#	if !defined(__x86_64__)
#		error "Only x64 OSX config supported."
#	endif
#else
#	error "Unsupported OS"
#endif

#if defined(_WINDOWS)
#	define LAGARITH_MULTITHREAD_SUPPORT 1
#	define WIN32_LEAN_AND_MEAN
#	include <windows.h>
#else
#	define LAGARITH_MULTITHREAD_SUPPORT 0
#endif

inline void* lag_aligned_malloc(void* ptr, uint32_t size, uint32_t align, const char* str) {
	(void)str;
	if (ptr) {
		try {
			_aligned_free(ptr);
		} catch (...) {
		}
	}
	return _aligned_malloc(size, align);
}

template <typename T> inline void lag_aligned_free(T*& ptr, const char* str) {
	(void)str;
	try {
		_aligned_free((void*)ptr);
	} catch (...) {
	}
	ptr = nullptr;
}

// y must be 2^n
inline uint32_t align_round(uint32_t x, uint32_t y) {
	return (((x) + (y - 1)) & (~(y - 1)));
}

// possible frame flags
#define ARITH_RGB24 4 // Standard RGB24 keyframe frame
#define BYTEFRAME 5   // solid greyscale color frame
#define PIXELFRAME 6  // solid non-greyscale color frame

// possible colorspaces
#define RGB24 24
#define RGB32 32

#include "compact.h"

namespace Lagarith {
#if LAGARITH_MULTITHREAD_SUPPORT
struct ThreadData {
	volatile const uint8_t* source     = nullptr;
	volatile uint8_t*       dest       = nullptr;
	volatile uint8_t*       buffer     = nullptr;
	volatile uint32_t       width      = 0;
	volatile uint32_t       height     = 0;
	volatile uint32_t       format     = 0;
	volatile uint32_t       length     = 0; // uncompressed data length
	volatile uint32_t       size       = 0; // compressed data length
	volatile HANDLE         thread     = NULL;
	volatile HANDLE         StartEvent = NULL;
	volatile HANDLE         DoneEvent  = NULL;
	uint32_t                priority   = 0;
	CompressClass           cObj;
};
#else
struct ThreadData {
	uint32_t dummy = 0;
};
#endif // LAGARITH_MULTITHREAD_SUPPORT
} // namespace Lagarith

#include "prediction.h"
