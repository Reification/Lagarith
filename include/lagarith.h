//	Lagarith v1.3.25, copyright 2011 by Ben Greenwood.
//	http://lags.leetcode.net/codec.html
//
//	This program is free software; you can redistribute it and/or
//	modify it under the terms of the GNU General Public License
//	as published by the Free Software Foundation; either version 2
//	of the License, or (at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#pragma once

#include <process.h>
#include <malloc.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vfw.h>
#include <malloc.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include "prediction.h"

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

#include "compact.h"

static const DWORD FOURCC_LAGS = mmioFOURCC('L', 'A', 'G', 'S');

// possible frame flags

#define UNCOMPRESSED 1 // Used for debugging
#define ARITH_RGB24 4      // Standard RGB24 keyframe frame
#define BYTEFRAME 5        // solid greyscale color frame
#define PIXELFRAME 6       // solid non-greyscale color frame

// possible colorspaces
#define RGB24 24
#define RGB32 32

struct ThreadData {
	volatile const unsigned char* source = nullptr;
	volatile unsigned char*       dest = nullptr;
	volatile unsigned char*       buffer = nullptr;
	volatile unsigned int         width = 0;
	volatile unsigned int         height = 0;
	volatile unsigned int         format = 0;
	volatile unsigned int         length = 0; // uncompressed data length
	volatile unsigned int         size = 0;   // compressed data length
	volatile HANDLE               thread = NULL;
	volatile HANDLE               StartEvent = NULL;
	volatile HANDLE               DoneEvent  = NULL;
	unsigned int                  priority = 0;
	CompressClass                 cObj;
};

class CodecInst {
public:
	CodecInst();
	~CodecInst();

	void SetMultithreaded(bool mt) {
		assert(width == 0 && "multithreading must be configured before calling CompressBegin or DecompressBegin.");
		if (!width)
			multithreading = mt;
	}

	DWORD CompressBegin(unsigned int w, unsigned int h, unsigned int bitsPerPixel);
	DWORD Compress(const void* src, void* dst, unsigned int* frameSizeOut);
	DWORD CompressEnd();

	DWORD DecompressBegin(unsigned int w, unsigned int h, unsigned int bitsPerPixel);
	DWORD Decompress(const void* src, unsigned int compressedFrameSize, void* dst);
	DWORD DecompressEnd();

private:
	int  InitThreads(int encode);
	void EndThreads();
	int  CompressRGB24(unsigned int* frameSizeOut);

	unsigned int HandleTwoCompressionThreads(unsigned int chan_size);
	
	void SetSolidFrameRGB24(const unsigned int r, const unsigned int g, const unsigned int b);
	void SetSolidFrameRGB32(const unsigned int r, const unsigned int g, const unsigned int b,
	                        const unsigned int a);
	void Decode3Channels(unsigned char* dst1, unsigned int len1, unsigned char* dst2,
	                     unsigned int len2, unsigned char* dst3, unsigned int len3);
	void ArithRGBDecompress();
private:
	int                  started = 0;
	unsigned char*       buffer = nullptr;
	unsigned char*       prev = nullptr;
	const unsigned char* in = nullptr;
	unsigned char*       out = nullptr;
	unsigned char*       buffer2 = nullptr;

	unsigned int length = 0;
	unsigned int width = 0;
	unsigned int height = 0;
	//input format for compressing, output format for decompression. Also the bitdepth.
	unsigned int format = 0;

	bool          multithreading = false;
	CompressClass cObj;
	unsigned int  compressed_size = 0;
	ThreadData    threads[2];
};
