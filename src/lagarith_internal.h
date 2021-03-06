#pragma once

#include "lagarith_platform.h"

#if defined(LAGARITH_X64)
#	include <tmmintrin.h>
#	include <intrin.h>

#	define lag_mm_loadl_epi64 _mm_loadl_epi64
#	define lag_mm_setr_epi8 _mm_setr_epi8
#	define lag_mm_alignr_epi8 _mm_alignr_epi8
#	define lag_mm_unpacklo_epi64 _mm_unpacklo_epi64
#	define lag_mm_lddqu_si128 _mm_lddqu_si128
#	define lag_mm_shuffle_epi8 _mm_shuffle_epi8
#	define lag_mm_shufflelo_epi16 _mm_shufflelo_epi16
#	define lag__emulu __emulu

#elif defined(LAGARITH_ARM64)
#	include "sse2neon/SSE2NEON.h"
#	include "lag_sse2neon_ext.h"

#endif // LAGARITH_ARM64

template <typename T> inline void lag_aligned_free(T*& ptr, const char* str) {
	assert(!(((uintptr_t)ptr) & 0xf) && "not an aligned allocation!");

	if (ptr) {
		delete[] reinterpret_cast<__m128i*>((void*)ptr);
		ptr = nullptr;
	}
}

inline void* lag_aligned_malloc(void* ptr, uint32_t size, uint32_t align, const char* str) {
	assert(align <= 16 && !(align & (align - 1)) && "invalid alignment");

	(void)str;
	if (ptr) {
		lag_aligned_free(ptr, str);
	}

	if (align > 16) {
		return nullptr;
	}
	align = 16;

	const uint32_t numInt128s = (size >> 4) + !!(size & 0xf);

	ptr = new __m128i[numInt128s];
	assert(!(((uintptr_t)ptr) & 0xf) && "not an aligned allocation!");

	return ptr;
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
