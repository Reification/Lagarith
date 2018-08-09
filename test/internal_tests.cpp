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

	int64x2_t(int64_t lo, int64_t hi) : low(lo), high(hi) {}
	__m128i asm128i() const { return *(const __m128i*)this; }

	bool operator==(const int64x2_t& rhs) const { return low == rhs.low && high == rhs.high; }

	bool operator!=(const int64x2_t& rhs) const { return !operator==(rhs); }
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
	__m128i asm128i() const { return *(const __m128i*)this; }

	bool operator==(const int64x2_t& rhs) const { return low == rhs.low && !high && !rhs.high; }

	bool operator!=(const int64x2_t& rhs) const { return !operator==(rhs); }
};

inline __m128i vcombine_s64(int64x1_t lo, int64x1_t hi) {
	int64x2_t out(lo.low, hi.low);
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

DECLARE_TEST(loadl_epi64_Test) {
	const __m128i v128           = int64x2_t(0x4444444433333333ll, 0x2222222211111111ll).asm128i();
	const __m128i v128low_native = _mm_loadl_epi64(&v128);
	const __m128i v128low_emul   = lagx_mm_loadl_epi64(&v128);

	if (int64x2_t(v128low_native) != int64x2_t(v128low_emul)) {
		return false;
	}

	return true;
}

DECLARE_TEST(setr_epi8_Test) {
#	define ALL_16_BYTES 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15

	const __m128i v128_native = _mm_setr_epi8(ALL_16_BYTES);
	const __m128i v128_emul   = lagx_mm_setr_epi8(ALL_16_BYTES);

	if (int64x2_t(v128_native) != int64x2_t(v128_emul)) {
		return false;
	}

	return true;
#	undef ALL_16_BYTES
}

DECLARE_TEST(alignr_epi8_Test) {
	const __m128i a128 = int64x2_t(0xffeeddccbbaa9988ll, 0x7766554433221100ll).asm128i();
	const __m128i b128 = int64x2_t(0x00ff11ee22dd33ccll, 0x44bb55aa66997788ll).asm128i();

#	define AR_CASE(X)                                                                               \
	case X:                                                                                          \
		v128_native = _mm_alignr_epi8(a128, b128, X);                                                  \
		v128_emul   = lagx_mm_alignr_epi8(a128, b128, X);                                              \
		break

	for (uint32_t i = 0; i <= 32; i++) {
		__m128i v128_native;
		__m128i v128_emul;

		switch (i) {
			AR_CASE(0);
			AR_CASE(1);
			AR_CASE(2);
			AR_CASE(3);
			AR_CASE(4);
			AR_CASE(5);
			AR_CASE(6);
			AR_CASE(7);
			AR_CASE(8);
			AR_CASE(9);
			AR_CASE(10);
			AR_CASE(11);
			AR_CASE(12);
			AR_CASE(13);
			AR_CASE(14);
			AR_CASE(15);
			AR_CASE(16);
			AR_CASE(17);
			AR_CASE(18);
			AR_CASE(19);
			AR_CASE(20);
			AR_CASE(21);
			AR_CASE(22);
			AR_CASE(23);
			AR_CASE(24);
			AR_CASE(25);
			AR_CASE(26);
			AR_CASE(27);
			AR_CASE(28);
			AR_CASE(29);
			AR_CASE(30);
			AR_CASE(31);
			AR_CASE(32);
		}

		if (int64x2_t(v128_native) != int64x2_t(v128_emul)) {
			return false;
		}
	}

	return true;
#	undef AR_CASE
}

DECLARE_TEST(unpacklo_epi64_Test) {
	const __m128i a128        = int64x2_t(0xffeeddccbbaa9988ll, 0x7766554433221100ll).asm128i();
	const __m128i b128        = int64x2_t(0x00ff11ee22dd33ccll, 0x44bb55aa66997788ll).asm128i();
	const __m128i v128_native = _mm_unpacklo_epi64(a128, b128);
	const __m128i v128_emul   = lagx_mm_unpacklo_epi64(a128, b128);

	if (int64x2_t(v128_native) != int64x2_t(v128_emul)) {
		return false;
	}

	return true;
}

DECLARE_TEST(lddqu_si128_Test) {
	uint8_t data[17] = {0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	                    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
	static_assert(sizeof(data) == sizeof(__m128i) + 1, "test data is wrong size.");
	const __m128i* pData = (const __m128i*)(data + 1);

	if (!(((uintptr_t)pData) & 0xf)) {
		fprintf(stderr, "warning: unaligned load test being done with aligned pointer.\n");
	}

	const __m128i v128_native = _mm_lddqu_si128(pData);
	const __m128i v128_emul   = lagx_mm_lddqu_si128(pData);

	if (int64x2_t(v128_native) != int64x2_t(v128_emul)) {
		return false;
	}

	return true;
}

DECLARE_TEST(shufflelo_epi16_Test) {
	const int     shuffle0     = _MM_SHUFFLE(1, 0, 3, 2);
	const int     shuffle1     = _MM_SHUFFLE(1, 1, 2, 2);
	const __m128i v128         = int64x2_t(0xffeeddccbbaa9988ll, 0x7766554433221100ll).asm128i();
	const __m128i s128_native0 = _mm_shufflelo_epi16(v128, shuffle0);
	const __m128i s128_emul0   = lagx_mm_shufflelo_epi16(v128, shuffle0);
	const __m128i s128_native1 = _mm_shufflelo_epi16(v128, shuffle1);
	const __m128i s128_emul1   = lagx_mm_shufflelo_epi16(v128, shuffle1);

	if (int64x2_t(s128_native0) != int64x2_t(s128_emul0)) {
		return false;
	}

	if (int64x2_t(s128_native1) != int64x2_t(s128_emul1)) {
		return false;
	}

	return true;
}

DECLARE_TEST(shuffle_epi8_Test) {
	//#	define lag_mm_shuffle_epi8 _mm_shuffle_epi8
	const __m128i shuffle0 = _mm_setr_epi8(0, 3, 6, 9, 2, 5, 8, 11, 1, 4, 7, 10, -1, -1, -1, -1);
	const __m128i shuffle1 = _mm_setr_epi8(0, 3, 6, 9, 2, 5, 8, 11, 1, 4, 7, 10, 12, 14, 15, 13);

	const __m128i v128         = int64x2_t(0xffeeddccbbaa9988ll, 0x7766554433221100ll).asm128i();
	const __m128i s128_native0 = _mm_shuffle_epi8(v128, shuffle0);
	const __m128i s128_emul0   = lagx_mm_shuffle_epi8(v128, shuffle0);
	const __m128i s128_native1 = _mm_shuffle_epi8(v128, shuffle1);
	const __m128i s128_emul1   = lagx_mm_shuffle_epi8(v128, shuffle1);

	if (int64x2_t(s128_native0) != int64x2_t(s128_emul0)) {
		return false;
	}

	if (int64x2_t(s128_native1) != int64x2_t(s128_emul1)) {
		return false;
	}

	return true;
}

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