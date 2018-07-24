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

#define LAGARITH_RELEASE // if this is a version to release, disables all debugging info

inline void* lag_aligned_malloc(void* ptr, int size, int align, const char* str) {
	if (ptr) {
		try {
#ifndef LAGARITH_RELEASE
			char msg[128];
			sprintf_s(msg, 128, "Buffer '%s' is not null, attempting to free it...", str);
			MessageBox(HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION);
#endif
			_aligned_free(ptr);
		} catch (...) {
#ifndef LAGARITH_RELEASE
			char msg[256];
			sprintf_s(
			  msg, 128,
			  "An exception occurred when attempting to free non-null buffer '%s' in lag_aligned_malloc",
			  str);
			MessageBox(HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION);
#endif
		}
	}
	return _aligned_malloc(size, align);
}

#ifdef LAGARITH_RELEASE
#	define lag_aligned_free(ptr, str)                                                               \
		{                                                                                              \
			if (ptr) {                                                                                   \
				try {                                                                                      \
					_aligned_free((void*)ptr);                                                               \
				} catch (...) {                                                                            \
				}                                                                                          \
			}                                                                                            \
			ptr = NULL;                                                                                  \
		}
#else
#	define lag_aligned_free(ptr, str)                                                               \
		{                                                                                              \
			if (ptr) {                                                                                   \
				try {                                                                                      \
					_aligned_free(ptr);                                                                      \
				} catch (...) {                                                                            \
					char err_msg[256];                                                                       \
					sprintf_s(err_msg, 256,                                                                  \
					          "Error when attempting to free pointer %s, value = 0x%X - file %s line %d",    \
					          str, ptr, __FILE__, __LINE__);                                                 \
					MessageBox(HWND_DESKTOP, err_msg, "Error", MB_OK | MB_ICONEXCLAMATION);                  \
				}                                                                                          \
			}                                                                                            \
			ptr = NULL;                                                                                  \
		}
#endif

// y must be 2^n
#define align_round(x, y) ((((unsigned int)(x)) + (y - 1)) & (~(y - 1)))

static constexpr DWORD FOURCC_LAGS = mmioFOURCC('L', 'A', 'G', 'S');

// possible frame flags

#define UNCOMPRESSED 1 // Used for debugging
#define ARITH_RGB24 4  // Standard RGB24 keyframe frame
#define BYTEFRAME 5    // solid greyscale color frame
#define PIXELFRAME 6   // solid non-greyscale color frame

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
