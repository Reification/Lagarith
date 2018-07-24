#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vfw.h>
#include <malloc.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <float.h>
#include <process.h>

inline void* lag_aligned_malloc(void* ptr, int size, int align, const char* str) {
	(void)str;
	if (ptr) {
		try {
			_aligned_free(ptr);
		} catch (...) {
		}
	}
	return _aligned_malloc(size, align);
}

template<typename T>
inline void lag_aligned_free(T*& ptr, const char* str) {
	(void)str;
	try {
		_aligned_free((void*)ptr);
	} catch (...) {
	}
	ptr = nullptr;
}

// y must be 2^n
inline unsigned int align_round( unsigned int x, unsigned int y )
{
	return (((x) + (y - 1)) & (~(y - 1)));
}

static constexpr DWORD FOURCC_LAGS = mmioFOURCC('L', 'A', 'G', 'S');

// possible frame flags
#define UNCOMPRESSED 1	// Used for debugging
#define ARITH_RGB24 4		// Standard RGB24 keyframe frame
#define BYTEFRAME 5			// solid greyscale color frame
#define PIXELFRAME 6		// solid non-greyscale color frame

// possible colorspaces
#define RGB24 24
#define RGB32 32

#include "compact.h"

namespace Lagarith {
struct ThreadData {
	volatile const unsigned char* source     = nullptr;
	volatile unsigned char*       dest       = nullptr;
	volatile unsigned char*       buffer     = nullptr;
	volatile unsigned int         width      = 0;
	volatile unsigned int         height     = 0;
	volatile unsigned int         format     = 0;
	volatile unsigned int         length     = 0; // uncompressed data length
	volatile unsigned int         size       = 0; // compressed data length
	volatile HANDLE               thread     = NULL;
	volatile HANDLE               StartEvent = NULL;
	volatile HANDLE               DoneEvent  = NULL;
	unsigned int                  priority   = 0;
	CompressClass                 cObj;
};
} // namespace Lagarith

#include "prediction.h"
