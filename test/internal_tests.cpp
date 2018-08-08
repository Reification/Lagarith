#include "test_internal.h"

#if defined(_WINDOWS)

#	include <tmmintrin.h>
#	include <intrin.h>

struct int64x2_t {
	int64_t low  = 0;
	int64_t high = 0;

public:
	int64x2_t() = default;
	explicit int64x2_t(__m128i rhs)
	  : low(((const int64x2_t*)&rhs)->low), high(((const int64x2_t*)&rhs)->high) {}
	int64x2_t(int64_t hi, int64_t lo) : low(lo), high(hi) {}
};

struct int64x1_t {
	int64_t low = 0;

private:
	int64_t high = 0;

public:
	int64x1_t() = default;
	int64x1_t(__m128i rhs, int hl)
	  : low(hl ? ((const int64x1_t*)&rhs)->high : ((const int64x1_t*)&rhs)->low) {
		(void)high;
	}
	explicit int64x1_t(int64_t lo) : low(lo) {}
};

inline __m128i vcombine_s64(int64x1_t hi, int64x1_t lo) {
	int64x2_t out(hi.low, lo.low);
	return *((__m128i*)&out);
}

#	define vget_low_s64(a) int64x1_t(a, 0)

#	define vreinterpretq_m128i_s8(a) (a)
#	define vreinterpretq_m128i_s16(a) (a)
#	define vreinterpretq_m128i_s32(a) (a)
#	define vreinterpretq_m128i_s64(a) (a)
#	define vreinterpretq_s64_m128i(a) (a)

#	define vld1q_s8(a) (*((__m128i*)a))
#	define vld1q_s16(a) (*((__m128i*)a))
#	define vld1q_s32(a) (*((__m128i*)a))
#	define vld1q_s64(a) (*((__m128i*)a))

#	include "lag_sse2neon_ext.h"

#	define lag_mm_loadl_epi64 _mm_loadl_epi64
#	define lag_mm_setr_epi8 _mm_setr_epi8
#	define lag_mm_alignr_epi8 _mm_alignr_epi8
#	define lag_mm_unpacklo_epi64 _mm_unpacklo_epi64
#	define lag_mm_lddqu_si128 _mm_lddqu_si128
#	define lag_mm_shuffle_epi8 _mm_shuffle_epi8
#	define lag_mm_shufflelo_epi16 _mm_shufflelo_epi16
#	define lag__emulu __emulu

#if 0
DECLARE_TEST(loadl_epi64_Test) {
	return false;
}

DECLARE_TEST(setr_epi8_Test) {
	return false;
}

DECLARE_TEST(alignr_epi8_Test) {
	return false;
}

DECLARE_TEST(unpacklo_epi64_Test) {
	return false;
}

DECLARE_TEST(lddqu_si128_Test) {
	return false;
}

DECLARE_TEST(shuffle_epi8_Test) {
	return false;
}

DECLARE_TEST(shufflelo_epi16_Test) {
	return false;
}
#endif // 0

DECLARE_TEST(emulu_Test) {
	for (uint32_t i = 0; i < 32; i++) {
		uint32_t a      = (1 << i);
		uint32_t b      = (1 << (i / 2));
		uint64_t prodEx = __emulu(a, b);
		uint64_t prodAc = lagx__emulu(a, b);

		if (prodEx != prodAc) {
			return false;
		}
	}
	return true;
}

#endif // _WINDOWS