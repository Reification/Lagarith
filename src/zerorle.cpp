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

#include "lagarith_internal.h"
#include "fibonacci.h"

static void TestAndRLE_SSE2(uint8_t* const in, uint8_t** const out1, uint8_t** const out3,
                            const uint32_t length);

// this lookup table is used for encoding run lengths so that
// the run byte distribution should roughly match the data
// distribution, improving compression.
static const int8_t dist_match[] = {
  0,   0,    0,   -1,   1,   -2,   2,   -3,   3,   -4,   4,   -5,   5,   -6,   6,   -7,   7,   -8,
  8,   -9,   9,   -10,  10,  -11,  11,  -12,  12,  -13,  13,  -14,  14,  -15,  15,  -16,  16,  -17,
  17,  -18,  18,  -19,  19,  -20,  20,  -21,  21,  -22,  22,  -23,  23,  -24,  24,  -25,  25,  -26,
  26,  -27,  27,  -28,  28,  -29,  29,  -30,  30,  -31,  31,  -32,  32,  -33,  33,  -34,  34,  -35,
  35,  -36,  36,  -37,  37,  -38,  38,  -39,  39,  -40,  40,  -41,  41,  -42,  42,  -43,  43,  -44,
  44,  -45,  45,  -46,  46,  -47,  47,  -48,  48,  -49,  49,  -50,  50,  -51,  51,  -52,  52,  -53,
  53,  -54,  54,  -55,  55,  -56,  56,  -57,  57,  -58,  58,  -59,  59,  -60,  60,  -61,  61,  -62,
  62,  -63,  63,  -64,  64,  -65,  65,  -66,  66,  -67,  67,  -68,  68,  -69,  69,  -70,  70,  -71,
  71,  -72,  72,  -73,  73,  -74,  74,  -75,  75,  -76,  76,  -77,  77,  -78,  78,  -79,  79,  -80,
  80,  -81,  81,  -82,  82,  -83,  83,  -84,  84,  -85,  85,  -86,  86,  -87,  87,  -88,  88,  -89,
  89,  -90,  90,  -91,  91,  -92,  92,  -93,  93,  -94,  94,  -95,  95,  -96,  96,  -97,  97,  -98,
  98,  -99,  99,  -100, 100, -101, 101, -102, 102, -103, 103, -104, 104, -105, 105, -106, 106, -107,
  107, -108, 108, -109, 109, -110, 110, -111, 111, -112, 112, -113, 113, -114, 114, -115, 115, -116,
  116, -117, 117, -118, 118, -119, 119, -120, 120, -121, 121, -122, 122, -123, 123, -124, 124, -125,
  125, -126, 126, -127, 127, -128};

extern const unsigned int* dist_rest;

#define dist_match_max 0x80

// this table is used in preforming RLE; it is used to look up how
// many leading zeros or non-zeros were found in an eight-byte block
static const uint32_t countlookup[] = {
  8, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5,
  0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6, 0,
  1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1,
  0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 7, 0, 1, 0,
  2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2,
  0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6, 0, 1, 0, 2, 0,
  1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1,
  0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 8};

static const uint32_t lvl3_lookup[] = {
  8, 8, 8, 8, 8, 8, 8, 0, 8, 8, 8, 8, 8, 8, 1, 0, 8, 8, 8, 8, 8, 8, 8, 0, 8, 8, 8, 8, 2, 2, 1, 0,
  8, 8, 8, 8, 8, 8, 8, 0, 8, 8, 8, 8, 8, 8, 1, 0, 8, 8, 8, 8, 8, 8, 8, 0, 3, 3, 3, 3, 2, 2, 1, 0,
  8, 8, 8, 8, 8, 8, 8, 0, 8, 8, 8, 8, 8, 8, 1, 0, 8, 8, 8, 8, 8, 8, 8, 0, 8, 8, 8, 8, 2, 2, 1, 0,
  8, 8, 8, 8, 8, 8, 8, 0, 8, 8, 8, 8, 8, 8, 1, 0, 4, 4, 4, 4, 4, 4, 4, 0, 3, 3, 3, 3, 2, 2, 1, 0,
  7, 7, 7, 7, 7, 7, 7, 0, 7, 7, 7, 7, 7, 7, 1, 0, 7, 7, 7, 7, 7, 7, 7, 0, 7, 7, 7, 7, 2, 2, 1, 0,
  7, 7, 7, 7, 7, 7, 7, 0, 7, 7, 7, 7, 7, 7, 1, 0, 7, 7, 7, 7, 7, 7, 7, 0, 3, 3, 3, 3, 2, 2, 1, 0,
  6, 6, 6, 6, 6, 6, 6, 0, 6, 6, 6, 6, 6, 6, 1, 0, 6, 6, 6, 6, 6, 6, 6, 0, 6, 6, 6, 6, 2, 2, 1, 0,
  5, 5, 5, 5, 5, 5, 5, 0, 5, 5, 5, 5, 5, 5, 1, 0, 4, 4, 4, 4, 4, 4, 4, 0, 3, 3, 3, 3, 2, 2, 1, 0};

#if 0 // not used - long run encoding appears to be implemented in SSE2 variants - kept for reference
// This function encodes zero runs longer than 256 bytes;
// This is rare and takes a variable amount of bytes per level
static void Encode_Long_Run(uint8_t** l1, uint8_t** l3, uint32_t count) {
	uint8_t* lvl1 = l1[0];
	uint8_t* lvl3 = l3[0];
	uint32_t   x    = count;
	while (x > 256) {
		lvl1[0] = 0;
		lvl1[1] = dist_match_max;
		x -= 256;
		lvl1 += 2;
	}
	lvl1[0] = 0;
	lvl1[1] = (uint8_t)dist_match[x + 1];
	lvl1 += 2;

	while (count > 258) {
		lvl3[0] = 0;
		lvl3[1] = 0;
		lvl3[2] = 0;
		lvl3[3] = dist_match_max;
		count -= 258;
		lvl3 += 4;
	}
	if (count == 1) {
		lvl3[0] = 0;
		lvl3++;
	} else if (count == 2) {
		lvl3[0] = 0;
		lvl3[1] = 0;
		lvl3 += 2;
	} else {
		lvl3[0] = 0;
		lvl3[1] = 0;
		lvl3[2] = 0;
		lvl3[3] = (uint8_t)dist_match[count - 1];
		lvl3 += 4;
	}
	l1[0] = lvl1;
	l3[0] = lvl3;
}
#endif // 0

/*
This function takes the input data and performs RLE on runs of zeros.
After performing the encoding, the function looks at the length of the
resulting byte sequences, and tries to select the RLE level that gives
the best compression & speed. The possible levels are 0 (no RLE
compression), 1 (each zero is followed by a byte indicating how many
additional zeros follow), and 3 (three sequential zero values are
followed by a byte indicating how many additional zeros follow).
There is also a special case for data in which every byte after the
first is zero, in this case the function will return -1, and only
the first byte needs to be saved.
*/

uint32_t TestAndRLE(uint8_t* const in, uint8_t* const out1, uint8_t* const out3, uint32_t length,
                    int* level) {
	uint8_t* lvl1 = out1;
	uint8_t* lvl3 = out3;

	// end marker values to prevent overrunning
	in[length]     = 255;
	in[length + 1] = 0;
	in[length + 2] = 0;
	in[length + 3] = 0;

	uint32_t a = 0;

	TestAndRLE_SSE2(in, &lvl1, &lvl3, length);

	uint32_t len1 = (int)(lvl1 - out1);
	uint32_t len3 = (int)(lvl3 - out3);

	// check if the data was one long run of zeros
	if (len1 <= (length / 256) * 2 + 3) {
		a = (in[0] > 0);
		if (((len1 - a) % 2) == 0) {
			bool solid = true;
			for (; a < len1 - 2; a += 2) {
				if (out1[a] || out1[a + 1] != 128) {
					solid = false;
					break;
				}
			}
			if (solid && out1[len1 - 2] == 0) {
				*level = -1;
				return 2;
			}
		}
	}

	// Level selection method:
	// None typically gives the best compression if level 3 cannot reduce the size
	// by at least %1.5. Level 1 typically gives better compression if it is
	// significantly smaller than level 3 (98% of level 3) and level 3 is much smaller than
	// none (32% of none). For very simple data (level 1 < 10% of none), none typically
	// will compress a few bytes less than level 1, but the speed tradeoff isn't
	// worth it in my opinion.

	// 197/200 = 98.5%
	if (length * 197 / 200 <= len3) {
		*level = 0;
		return length;
	}

	if (len1 < len3 * 98 / 100 && len3 * 100 / length <= 32) {
		*level = 1;
		return len1;
	}

	*level = 3;
	return len3;
}

static void TestAndRLE_SSE2(uint8_t* const in, uint8_t** const out1, uint8_t** const out3,
                            uint32_t length) {
	//23
	//20
	//11
	//10.3
	uint32_t a    = 0;
	uint8_t* lvl3 = *out3;

	const __m128i zero = _mm_setr_epi32(0, 0, -1, -1);
	// Perform RLE on the data using runs of length 1 and 3 using SSE
	while (true) {
		uint32_t step;
		do {
			// copy bytes until a zero run is found
			__m128i s = lag_mm_loadl_epi64((__m128i*)&in[a]);
			_mm_storel_epi64((__m128i*)lvl3, s);
			s              = _mm_cmpeq_epi8(s, zero);
			uint32_t index = _mm_movemask_epi8(s);
			step           = lvl3_lookup[index];
			lvl3 += step;
			a += step;
		} while (step >= 6);
		if (a >= length) {
			break;
		}

		uint32_t count = 3;
		a += 3;
		do {
			// count the number of zeros in the current run
			__m128i s = lag_mm_loadl_epi64((__m128i*)&in[a]);
			s         = _mm_cmpeq_epi8(s, zero);
			step      = _mm_movemask_epi8(s);
			step      = countlookup[step + 1]; // step now equals the number of sequential zeros
			a += step;
			count += step;
		} while (step == 8);

		if (count <= 258) {
			lvl3[0] = 0;
			lvl3[1] = 0;
			lvl3[2] = 0;
			lvl3[3] = (uint8_t)dist_match[count - 1];
			lvl3 += 4;
		} else {
			// encode the run of zeros
			while (count > 258) {
				lvl3[0] = 0;
				lvl3[1] = 0;
				lvl3[2] = 0;
				lvl3[3] = dist_match_max;

				count -= 258;
				lvl3 += 4;
			}
			lvl3[0] = 0;
			lvl3[1] = 0;
			lvl3[2] = 0;
			lvl3[3] = (uint8_t)dist_match[count - 1];
			lvl3 += (count >= 3) ? 4 : count;
		}
	}
	if (a > length) {
		lvl3--;
	}

	// if level 3 RLE is > 32% of no RLE (length), then level 1 RLE will not
	// be used and does not need to be calculated.
	uint32_t len  = (int)(lvl3 - *out3);
	uint8_t* lvl1 = *out1;
	if (len * 100 / length <= 32) {
		a = 0;
		if (in[0] == 0) {
			goto RLE_lvl1_0_start_SSE2;
		}
		while (true) {
			{
				uint32_t step;
				do {
					// copy non-zero bytes until a zero is found
					__m128i s = lag_mm_loadl_epi64((__m128i*)&in[a]);
					_mm_storel_epi64((__m128i*)lvl1, s);
					s    = _mm_cmpeq_epi8(s, zero);
					step = _mm_movemask_epi8(s) & 255;
					step = countlookup[step]; // step now equals the number of sequential non-zeros
					lvl1 += step;
					a += step;
				} while (step == 8);
			}
			if (a >= length) {
				break;
			}
		RLE_lvl1_0_start_SSE2:
			uint32_t count = 0;
			{
				uint32_t step;
				do {
					// count the number of zeros in the current run
					__m128i s = lag_mm_loadl_epi64((__m128i*)&in[a]);
					s         = _mm_cmpeq_epi8(s, zero);
					step      = _mm_movemask_epi8(s) & 255;
					step      = countlookup[step + 1]; // step now equals the number of sequential zeros
					a += step;
					count += step;
				} while (step == 8);
			}

			// encode the run of zeros
			while (count > 256) {
				lvl1[0] = 0;
				lvl1[1] = dist_match_max;
				count -= 256;
				lvl1 += 2;
			}
			lvl1[0] = 0;
			lvl1[1] = (uint8_t)dist_match[count + 1];
			lvl1 += 2;
		}
		if (a > length) {
			lvl1--;
		}
	} else {
		lvl1 += length;
	}

	*out1 = lvl1;
	*out3 = lvl3;
}

// This undoes the modified Run Length Encoding on a byte stream.
// The 'level' parameter tells how many 0's must be read before it
// is considered a 'run', either 1, 2, or 3. Once 'level' zeros have
// been read in the byte stream, the following byte tells how many more
// 0 bytes to output. This routine is only used in the rare occurence
// where RLE compression is better than header + range coding

int deRLE(const uint8_t* in, uint8_t* out, int length, int level) {
	int a = 0;
	int b = 0;
	memset(out, 0, length);
	if (level == 1) {
		while (b < length) {
			if (in[a]) {
				out[b++] = in[a++];
			} else {
				b += dist_rest[in[a + 1]] + 1;
				a += 2;
			}
		}
	} else if (level == 2) {
		while (b < length - 1) {
			if (in[a]) {
				out[b++] = in[a++];
			} else if (in[a + 1]) {
				out[b]     = 0;
				out[b + 1] = in[a + 1];
				b += 2;
				a += 2;
			} else {
				b += dist_rest[in[a + 2]] + 2;
				a += 3;
			}
		}
		if (b < length) {
			out[b] = in[a];
		}
	} else if (level == 3) {
		while (b < length - 2) {
			if (in[a]) {
				out[b++] = in[a++];
			} else if (in[a + 1]) {
				out[b]     = 0;
				out[b + 1] = in[a + 1];
				b += 2;
				a += 2;
			} else if (in[a + 2]) {
				out[b]     = 0;
				out[b + 1] = 0;
				out[b + 2] = in[a + 2];
				b += 3;
				a += 3;
			} else {
				b += dist_rest[in[a + 3]] + 3;
				a += 4;
			}
		}
		if (b < length) {
			out[b] = in[a];
			if (b < length - 1) {
				out[b + 1] = in[a + 1];
			}
		}
	}
	return a;
}

//MessageBox (HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION);
