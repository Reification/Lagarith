#pragma once

#pragma push_macro("FORCE_INLINE")
#pragma push_macro("ALIGN_STRUCT")

#if !defined(__INTELLISENSE__)
#	define FORCE_INLINE static inline __attribute__((always_inline))
#	define ALIGN_STRUCT(x) __attribute__((aligned(x)))
#else
#	define FORCE_INLINE inline
#	define ALIGN_STRUCT(x)
#	include <stdint.h>
#endif

#ifndef __constrange
#	define __constrange(a, b) const
#endif

FORCE_INLINE __m128i lag_mm_loadl_epi64(const __m128i* a) {
	const int64_t ALIGN_STRUCT(16) data[2] = {((const int64_t*)a)[0], 0ll};
	return vreinterpretq_m128i_s64(vld1q_s64(data));
}

FORCE_INLINE __m128i lag_mm_setr_epi8(int8_t i15, int8_t i14, int8_t i13, int8_t i12, int8_t i11,
                                       int8_t i10, int8_t i9, int8_t i8, int8_t i7, int8_t i6,
                                       int8_t i5, int8_t i4, int8_t i3, int8_t i2, int8_t i1,
                                       int8_t i0) {
	const int8_t ALIGN_STRUCT(16)
	  data[16] = {i15, i14, i13, i12, i11, i10, i9, i8, i7, i6, i5, i4, i3, i2, i1, i0};
	return vreinterpretq_m128i_s8(vld1q_s8(data));
}

FORCE_INLINE __m128i lag_mm_alignr_epi8(__m128i a, __m128i b,
                                         __constrange(0, 32) uint32_t ralign) {
	const __m128i val256[2]                 = {b, a};
	const int8_t* pv                        = (const int8_t*)val256;
	int8_t        ALIGN_STRUCT(16) data[16] = {};
	for (int i = 0, j = ralign; i < 16 && j < 32; i++, j++) {
		data[i] = pv[j];
	}
	return vreinterpretq_m128i_s8(vld1q_s8(data));
}

//   https://msdn.microsoft.com/en-us/library/x8atst9d(v=vs.100).aspx
// Interleaves the lower signed or unsigned 64 - bit integer in a with the lower signed or unsigned 64 - bit integer in b.
FORCE_INLINE __m128i lag_mm_unpacklo_epi64(__m128i a, __m128i b) {
	int64x1_t a0 = vget_low_s64(vreinterpretq_s64_m128i(a));
	int64x1_t b0 = vget_low_s64(vreinterpretq_s64_m128i(b));
	return vreinterpretq_m128i_s64(vcombine_s64(a0, b0));
}

FORCE_INLINE __m128i lag_mm_lddqu_si128(const __m128i* v) {
	const int32_t* pv                       = (const int32_t*)v;
	const int32_t  ALIGN_STRUCT(16) data[16] = {pv[0], pv[1], pv[2], pv[3]};
	return vreinterpretq_m128i_s32(vld1q_s32(data));
}

FORCE_INLINE __m128i lag_mm_shuffle_epi8(__m128i v, __m128i s) {
	const int8_t* ps = (const int8_t*)&s;
	const int8_t* pv = (const int8_t*)&v;
#define _vb(i) ((int8_t)(!(ps[i] & 0xf0) * pv[ps[i]]))
	const int8_t ALIGN_STRUCT(16)
	  data[16] = {_vb(0), _vb(1), _vb(2),  _vb(3),  _vb(4),  _vb(5),  _vb(6),  _vb(7),
	              _vb(8), _vb(9), _vb(10), _vb(11), _vb(12), _vb(13), _vb(14), _vb(15)};
#undef _vb
	return vreinterpretq_m128i_s8(vld1q_s8(data));
}

FORCE_INLINE __m128i lag_mm_shufflelo_epi16(__m128i v, const int s) {
	const int16_t* pv                        = (const int16_t*)&v;
	const int16_t  ALIGN_STRUCT(16) data[16] = {
    pv[s & 0x3], pv[(s >> 2) & 0x3], pv[(s >> 4) & 0x3], pv[(s >> 6) & 0x3], pv[4], pv[5], pv[6],
    pv[7]};
	return vreinterpretq_m128i_s16(vld1q_s16(data));
}

FORCE_INLINE uint64_t lag__emulu(uint32_t a, uint32_t b) {
	return (uint64_t)a * (uint64_t)b;
}

#pragma pop_macro("ALIGN_STRUCT")
#pragma pop_macro("FORCE_INLINE")
