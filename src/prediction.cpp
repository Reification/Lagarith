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

//This file contains functions that perform and undo median predition that are used by both x86 and x64
#define WIN32_LEAN_AND_MEAN
#include "prediction.h"
#include <mmintrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include <tmmintrin.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

// Round x up to the next multiple of y if it is not a multiple. Y must be a power of 2.
#define align_round(x, y) ((((uintptr_t)(x)) + (y - 1)) & (~(y - 1)))

// this effectively performs a bubble sort to select the median:
// min(max(min(x,y),z),max(x,y))
// this assumes 32 bit ints, and that x and y are >=0 and <= 0x80000000
inline int median(int x, int y, int z) {
	int delta = x - y;
	delta &= delta >> 31;
	y += delta; // min
	x -= delta; // max
	delta = y - z;
	delta &= delta >> 31;
	y -= delta; // max
	delta = y - x;
	delta &= delta >> 31;
	return x + delta; // min
}

void Block_Predict_SSE2(const unsigned char* source, unsigned char* dest, unsigned int width,
                        unsigned int length, const bool rgbmode) {
	uintptr_t align_shift = (16 - ((uintptr_t)source & 15)) & 15;

	// predict the bottom row
	uintptr_t a;
	__m128i   t0 = _mm_setzero_si128();
	if (align_shift) {
		dest[0] = source[0];
		for (a = 1; a < align_shift; a++) {
			dest[a] = source[a] - source[a - 1];
		}
		t0 = _mm_cvtsi32_si128(source[align_shift - 1]);
	}
	for (a = align_shift; a < width; a += 16) {
		__m128i x           = *(__m128i*)&source[a];
		t0                  = _mm_or_si128(t0, _mm_slli_si128(x, 1));
		*(__m128i*)&dest[a] = _mm_sub_epi8(x, t0);
		t0                  = _mm_srli_si128(x, 15);
	}

	if (width >= length)
		return;

	__m128i z;
	__m128i x;

	if (rgbmode) {
		x = _mm_setzero_si128();
		z = _mm_setzero_si128();
	} else {
		x = _mm_cvtsi32_si128(source[width - 1]);
		z = _mm_cvtsi32_si128(source[0]);
	}

	a = width;
	{
		// this block makes sure that source[a] is aligned to 16
		__m128i srcs = _mm_loadu_si128((__m128i*)&source[a]);
		__m128i y    = _mm_loadu_si128((__m128i*)&source[a - width]);

		x = _mm_or_si128(x, _mm_slli_si128(srcs, 1));
		z = _mm_or_si128(z, _mm_slli_si128(y, 1));

		__m128i tx = _mm_unpackhi_epi8(x, _mm_setzero_si128());
		__m128i ty = _mm_unpackhi_epi8(y, _mm_setzero_si128());
		__m128i tz = _mm_unpackhi_epi8(z, _mm_setzero_si128());

		tz = _mm_add_epi16(_mm_sub_epi16(tx, tz), ty);

		tx = _mm_unpacklo_epi8(x, _mm_setzero_si128());
		ty = _mm_unpacklo_epi8(y, _mm_setzero_si128());
		z  = _mm_unpacklo_epi8(z, _mm_setzero_si128());
		z  = _mm_add_epi16(_mm_sub_epi16(tx, z), ty);
		z  = _mm_packus_epi16(z, tz);

		__m128i i = _mm_min_epu8(x, y);
		x         = _mm_max_epu8(x, y);
		i         = _mm_max_epu8(i, z);
		x         = _mm_min_epu8(x, i);

		srcs = _mm_sub_epi8(srcs, x);
		_mm_storeu_si128((__m128i*)&dest[a], srcs);
	}

	if (align_shift) {
		a = align_round(a, 16);
		a += align_shift;
		if (a > width + 16) {
			a -= 16;
		}
	} else {
		a += 16;
		a &= (~15);
	}
	// source[a] is now aligned
	x = _mm_cvtsi32_si128(source[a - 1]);
	z = _mm_cvtsi32_si128(source[a - width - 1]);

	const uintptr_t end = (length >= 15) ? length - 15 : 0;

	// if width is a multiple of 16, use faster aligned reads
	// inside the prediction loop
	if (width % 16 == 0) {
		for (; a < end; a += 16) {
			__m128i srcs = *(__m128i*)&source[a];
			__m128i y    = *(__m128i*)&source[a - width];

			x = _mm_or_si128(x, _mm_slli_si128(srcs, 1));
			z = _mm_or_si128(z, _mm_slli_si128(y, 1));

			__m128i tx = _mm_unpackhi_epi8(x, _mm_setzero_si128());
			__m128i ty = _mm_unpackhi_epi8(y, _mm_setzero_si128());
			__m128i tz = _mm_unpackhi_epi8(z, _mm_setzero_si128());

			tz = _mm_add_epi16(_mm_sub_epi16(tx, tz), ty);

			tx = _mm_unpacklo_epi8(x, _mm_setzero_si128());
			ty = _mm_unpacklo_epi8(y, _mm_setzero_si128());
			z  = _mm_unpacklo_epi8(z, _mm_setzero_si128());
			z  = _mm_add_epi16(_mm_sub_epi16(tx, z), ty);
			z  = _mm_packus_epi16(z, tz);

			__m128i i = _mm_min_epu8(x, y);
			x         = _mm_max_epu8(x, y);
			i         = _mm_max_epu8(i, z);
			x         = _mm_min_epu8(x, i);

			i = _mm_srli_si128(srcs, 15);

			srcs = _mm_sub_epi8(srcs, x);

			z = _mm_srli_si128(y, 15);
			x = i;

			*(__m128i*)&dest[a] = srcs;
		}
	} else {
		// main prediction loop, source[a-width] is unaligned
		for (; a < end; a += 16) {
			__m128i srcs = *(__m128i*)&source[a];
			__m128i y = _mm_loadu_si128((__m128i*)&source[a - width]); // only change from aligned version

			x = _mm_or_si128(x, _mm_slli_si128(srcs, 1));
			z = _mm_or_si128(z, _mm_slli_si128(y, 1));

			__m128i tx = _mm_unpackhi_epi8(x, _mm_setzero_si128());
			__m128i ty = _mm_unpackhi_epi8(y, _mm_setzero_si128());
			__m128i tz = _mm_unpackhi_epi8(z, _mm_setzero_si128());

			tz = _mm_add_epi16(_mm_sub_epi16(tx, tz), ty);

			tx = _mm_unpacklo_epi8(x, _mm_setzero_si128());
			ty = _mm_unpacklo_epi8(y, _mm_setzero_si128());
			z  = _mm_unpacklo_epi8(z, _mm_setzero_si128());
			z  = _mm_add_epi16(_mm_sub_epi16(tx, z), ty);
			z  = _mm_packus_epi16(z, tz);

			__m128i i = _mm_min_epu8(x, y);
			x         = _mm_max_epu8(x, y);
			i         = _mm_max_epu8(i, z);
			x         = _mm_min_epu8(x, i);

			i = _mm_srli_si128(srcs, 15);

			srcs = _mm_sub_epi8(srcs, x);

			z = _mm_srli_si128(y, 15);
			x = i;

			*(__m128i*)&dest[a] = srcs;
		}
	}
	for (; a < length; a++) {
		int x   = source[a - 1];
		int y   = source[a - width];
		int z   = x + y - source[a - width - 1];
		dest[a] = source[a] - median(x, y, z);
	}
}

void Decorrelate_And_Split_RGB24_SSE2(const unsigned char* in, unsigned char* rdst,
                                      unsigned char* gdst, unsigned char* bdst, unsigned int width,
                                      unsigned int height) {
	const uintptr_t stride = align_round(width * 3, 4);

	{
		// 10
		const uintptr_t vsteps = (width & 7) ? height : 1;
		const uintptr_t wsteps = (width & 7) ? (width & (~7)) : width * height;

		const __m128i shuffle =
		  _mm_setr_epi8(0, 3, 6, 9, 2, 5, 8, 11, 1, 4, 7, 10, -1, -1, -1, -1); // bbbb rrrr gggg 0000
		for (uintptr_t y = 0; y < vsteps; y++) {
			uintptr_t x;
			for (x = 0; x < wsteps; x += 8) {
				__m128i s0 = _mm_lddqu_si128((__m128i*)&in[y * stride + x * 3 + 0]);
				__m128i s1 = _mm_loadl_epi64((__m128i*)&in[y * stride + x * 3 + 16]);
				s1         = _mm_alignr_epi8(s1, s0, 12);
				s0         = _mm_shuffle_epi8(s0, shuffle);
				s1         = _mm_shuffle_epi8(s1, shuffle);

				__m128i g  = _mm_unpackhi_epi32(s0, s1);
				__m128i br = _mm_unpacklo_epi32(s0, s1);
				g          = _mm_unpacklo_epi64(g, g);
				br         = _mm_sub_epi8(br, g);


				_mm_storel_epi64((__m128i*)&bdst[y * width + x], br);
				_mm_storel_epi64((__m128i*)&gdst[y * width + x], g);
				_mm_storel_epi64((__m128i*)&rdst[y * width + x], _mm_srli_si128(br, 8));
			}
			for (; x < width; x++) {
				bdst[y * width + x] = in[y * stride + x * 3 + 0] - in[y * stride + x * 3 + 1];
				gdst[y * width + x] = in[y * stride + x * 3 + 1];
				rdst[y * width + x] = in[y * stride + x * 3 + 2] - in[y * stride + x * 3 + 1];
			}
		}
	}
}

void Decorrelate_And_Split_RGB32_SSE2(const unsigned char* in, unsigned char* rdst,
                                      unsigned char* gdst, unsigned char* bdst, unsigned int width,
                                      unsigned int height) {
	uintptr_t a     = 0;
	uintptr_t align = (uintptr_t)in;
	align &= 15;
	align /= 4;

	for (a = 0; a < align; a++) {
		bdst[a] = in[a * 4 + 0] - in[a * 4 + 1];
		gdst[a] = in[a * 4 + 1];
		rdst[a] = in[a * 4 + 2] - in[a * 4 + 1];
	}

	const uintptr_t end  = (width * height - align) & (~7);
	const __m128i   mask = _mm_set1_epi16(255);
	for (; a < end; a += 8) {
		__m128i x0 = *(__m128i*)&in[a * 4 + 0];
		__m128i x2 = *(__m128i*)&in[a * 4 + 16];
		__m128i x1 = x0;
		__m128i x3 = x2;
		x0         = _mm_srli_epi16(x0, 8);   // ga
		x1         = _mm_and_si128(x1, mask); // br
		x2         = _mm_srli_epi16(x2, 8);
		x3         = _mm_and_si128(x3, mask);

		x0 = _mm_packus_epi16(x0, x2);
		x1 = _mm_packus_epi16(x1, x3);
		x2 = x0;
		x3 = x1;

		__m128i g = _mm_and_si128(x2, mask);
		__m128i r = _mm_srli_epi16(x1, 8);
		__m128i b = _mm_and_si128(x3, mask);

		b = _mm_packus_epi16(b, r);
		g = _mm_packus_epi16(g, g);
		b = _mm_sub_epi8(b, g);


		_mm_storel_epi64((__m128i*)&bdst[a], b);
		_mm_storel_epi64((__m128i*)&gdst[a], g);
		_mm_storel_epi64((__m128i*)&rdst[a], _mm_srli_si128(b, 8));
	}
	for (; a < width * height; a++) {
		bdst[a] = in[a * 4 + 0] - in[a * 4 + 1];
		gdst[a] = in[a * 4 + 1];
		rdst[a] = in[a * 4 + 2] - in[a * 4 + 1];
	}
}

void Interleave_And_Restore_RGB24_SSE2(unsigned char* output, const unsigned char* rsrc,
                                       const unsigned char* gsrc, const unsigned char* bsrc,
                                       unsigned int width, unsigned int height) {
	const uintptr_t stride = align_round(width * 3, 4);

	// restore the bottom row
	{
		int r = 0;
		int g = 0;
		int b = 0;
		for (uintptr_t a = 0; a < width; a++) {
			output[a * 3]     = b += bsrc[a];
			output[a * 3 + 1] = g += gsrc[a];
			output[a * 3 + 2] = r += rsrc[a];
		}
	}

	if (!height)
		return;

	// 81
	// 70
	// 55
	// 52
	__m128i z = _mm_setzero_si128();
	__m128i x = _mm_setzero_si128();

	uintptr_t w = 0;
	uintptr_t h = 0;

	const __m128i mask = _mm_setr_epi8(0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0);

	// if there is no need for padding, treat it as one long row
	const uintptr_t vsteps = (width & 3) ? height : 2;
	const uintptr_t wsteps = (width & 3) ? (width & (~3)) : (width * height - width - 4);

	for (h = 1; h < vsteps; h++) {
		w = 0;
		for (; w < wsteps; w += 4) {
			uintptr_t a = stride * h + w * 3;
			__m128i   b = _mm_cvtsi32_si128(*(unsigned int*)&bsrc[width * h + w]);
			__m128i   g = _mm_cvtsi32_si128(*(unsigned int*)&gsrc[width * h + w]);
			__m128i   r = _mm_cvtsi32_si128(*(unsigned int*)&rsrc[width * h + w]);
			b           = _mm_unpacklo_epi8(b, g);
			r           = _mm_unpacklo_epi8(r, r);

			__m128i src1 = _mm_unpacklo_epi16(b, r);
			__m128i src2 = _mm_unpackhi_epi8(src1, _mm_setzero_si128());
			src1         = _mm_unpacklo_epi8(src1, _mm_setzero_si128());

			__m128i y  = _mm_loadl_epi64((__m128i*)&output[a - stride]);
			__m128i ys = _mm_cvtsi32_si128(*(int*)&output[a - stride + 8]);
			y          = _mm_unpacklo_epi64(y, ys);


			__m128i recorrilate = mask;
			recorrilate         = _mm_and_si128(recorrilate, y);
			recorrilate = _mm_or_si128(_mm_srli_si128(recorrilate, 1), _mm_slli_si128(recorrilate, 1));
			recorrilate = _mm_add_epi8(recorrilate, y);
			_mm_storel_epi64((__m128i*)&output[a - stride], recorrilate);
			*(int*)&output[a - stride + 8] = _mm_cvtsi128_si32(_mm_srli_si128(recorrilate, 8));

			ys = y;
			y  = _mm_slli_si128(y, 1);
			y  = _mm_unpacklo_epi8(y, _mm_setzero_si128());
			y  = _mm_shufflelo_epi16(y, (1 << 0) + (2 << 2) + (3 << 4) + (0 << 6));
			ys = _mm_srli_si128(ys, 5);
			ys = _mm_unpacklo_epi8(ys, _mm_setzero_si128());
			ys = _mm_shufflelo_epi16(ys, (1 << 0) + (2 << 2) + (3 << 4) + (0 << 6));
			z  = _mm_unpacklo_epi64(z, y);

			z = _mm_sub_epi16(_mm_add_epi16(x, y), z);

			__m128i i = _mm_min_epi16(x, y); //min
			__m128i j = _mm_max_epi16(x, y); //max
			i         = _mm_max_epi16(i, z); //max
			j         = _mm_min_epi16(i, j); //min
			j         = _mm_add_epi8(j, src1);
			j         = _mm_slli_si128(j, 8);
			x         = _mm_or_si128(x, j);
			z         = _mm_add_epi16(z, j);

			/*******/

			i = _mm_min_epi16(x, y); //min
			x = _mm_max_epi16(x, y); //max
			i = _mm_max_epi16(i, z); //max
			x = _mm_min_epi16(i, x); //min
			x = _mm_add_epi8(x, src1);

			__m128i temp = x;
			x            = _mm_srli_si128(x, 8);
			z            = _mm_srli_si128(y, 8);
			z            = _mm_unpacklo_epi64(z, ys);
			z            = _mm_sub_epi16(_mm_add_epi16(x, ys), z);
			y            = ys;


			/*******/

			i = _mm_min_epi16(x, y); //min
			j = _mm_max_epi16(x, y); //max
			i = _mm_max_epi16(i, z); //max
			j = _mm_min_epi16(i, j); //min
			j = _mm_add_epi8(j, src2);
			j = _mm_slli_si128(j, 8);
			x = _mm_or_si128(x, j);
			z = _mm_add_epi16(z, j);

			/*******/

			i = _mm_min_epi16(x, y); //min
			x = _mm_max_epi16(x, y); //max
			i = _mm_max_epi16(i, z); //max
			x = _mm_min_epi16(i, x); //min
			x = _mm_add_epi8(x, src2);

			temp = _mm_shufflelo_epi16(temp, (3 << 0) + (0 << 2) + (1 << 4) + (2 << 6));
			temp = _mm_slli_si128(temp, 2);

			x    = _mm_shufflelo_epi16(x, (3 << 0) + (0 << 2) + (1 << 4) + (2 << 6));
			temp = _mm_packus_epi16(temp, _mm_srli_si128(x, 2));

			_mm_storel_epi64((__m128i*)&output[a], _mm_srli_si128(temp, 2));
			*(int*)&output[a + 8] = _mm_cvtsi128_si32(_mm_srli_si128(temp, 10));

			x = _mm_srli_si128(x, 8);
			z = _mm_srli_si128(y, 8);
		}
		if (h < vsteps - 1) {
			// if the width is not mod 4, take care of the remaining pixels in the row
			for (; w < width; w++) {
				uintptr_t a = stride * h + w * 3;

				__m128i src = _mm_cvtsi32_si128(bsrc[width * h + w] + (gsrc[width * h + w] << 8) +
				                                (rsrc[width * h + w] << 16));
				src         = _mm_unpacklo_epi8(src, _mm_setzero_si128());

				// row has padding, no overrun risk
				__m128i y = _mm_cvtsi32_si128(*(int*)&output[a - stride]);
				y         = _mm_unpacklo_epi8(y, _mm_setzero_si128());

				output[a + 0 - stride] += output[a + 1 - stride];
				output[a + 2 - stride] += output[a + 1 - stride];
				z = _mm_sub_epi16(_mm_add_epi16(x, y), z);

				__m128i i = _mm_min_epi16(x, y); //min
				x         = _mm_max_epi16(x, y); //max
				i         = _mm_max_epi16(i, z); //max
				x         = _mm_min_epi16(i, x); //min
				x         = _mm_add_epi8(x, src);

				// row has padding, no overrun risk
				*(int*)&output[a] = _mm_cvtsi128_si32(_mm_packus_epi16(x, x));

				z = y;
			}
		}
	}

	h = height - 1;
	w %= width;
	// take care of any remaining pixels
	for (; w < width; w++) {
		uintptr_t a = stride * h + w * 3;

		__m128i src = _mm_cvtsi32_si128(bsrc[width * h + w] + (gsrc[width * h + w] << 8) +
		                                (rsrc[width * h + w] << 16));
		src         = _mm_unpacklo_epi8(src, _mm_setzero_si128());

		// row has padding, no overrun risk
		__m128i y = _mm_cvtsi32_si128(*(int*)&output[a - stride]);
		y         = _mm_unpacklo_epi8(y, _mm_setzero_si128());

		output[a + 0 - stride] += output[a + 1 - stride];
		output[a + 2 - stride] += output[a + 1 - stride];
		z = _mm_sub_epi16(_mm_add_epi16(x, y), z);

		__m128i i = _mm_min_epi16(x, y); //min
		x         = _mm_max_epi16(x, y); //max
		i         = _mm_max_epi16(i, z); //max
		x         = _mm_min_epi16(i, x); //min
		x         = _mm_add_epi8(x, src);

		unsigned int temp = _mm_cvtsi128_si32(_mm_packus_epi16(x, x));
		output[a + 0]     = temp;
		output[a + 1]     = temp >> 8;
		output[a + 2]     = temp >> 16;

		z = y;
	}

	for (uintptr_t a = stride * (height - 1); a < stride * height; a += 3) {
		//for ( uintptr_t a=0;a<stride*height;a+=3){
		output[a] += output[a + 1];
		output[a + 2] += output[a + 1];
	}
}

void Interleave_And_Restore_RGB32_SSE2(unsigned char* output, const unsigned char* rsrc,
                                       const unsigned char* gsrc, const unsigned char* bsrc,
                                       unsigned int width, unsigned int height) {
	const uintptr_t stride = width * 4;
	{
		int r = 0;
		int g = 0;
		int b = 0;
		for (uintptr_t a = 0; a < width; a++) {
			output[a * 4 + 0] = b += bsrc[a];
			output[a * 4 + 1] = g += gsrc[a];
			output[a * 4 + 2] = r += rsrc[a];
			output[a * 4 + 3] = 255;
		}
	}

	if (height == 1)
		return;

	__m128i z = _mm_setzero_si128();
	__m128i x = _mm_setzero_si128();

	const uintptr_t end = ((width * (height - 1)) & (~3)) + width;
	uintptr_t       a   = width;

	if ((stride & 15) == 0) {
		// 85
		// 77
		// 65
		// 46

		for (; a < end; a += 4) {
			__m128i b = _mm_cvtsi32_si128(*(unsigned int*)&bsrc[a]);
			__m128i g = _mm_cvtsi32_si128(*(unsigned int*)&gsrc[a]);
			__m128i r = _mm_cvtsi32_si128(*(unsigned int*)&rsrc[a]);
			b         = _mm_unpacklo_epi8(b, g);
			r         = _mm_unpacklo_epi8(r, _mm_setzero_si128());

			__m128i src1 = _mm_unpacklo_epi16(b, r);
			__m128i src2 = _mm_unpackhi_epi8(src1, _mm_setzero_si128());
			src1         = _mm_unpacklo_epi8(src1, _mm_setzero_si128());

			__m128i temp2 = _mm_load_si128((__m128i*)&output[a * 4 - stride]); // must be %16
			__m128i y     = _mm_unpacklo_epi8(temp2, _mm_setzero_si128());
			z             = _mm_sub_epi16(_mm_add_epi16(x, y), z);

			__m128i i = _mm_min_epi16(x, y); //min
			x         = _mm_max_epi16(x, y); //max
			i         = _mm_max_epi16(i, z); //max
			x         = _mm_min_epi16(i, x); //min
			x         = _mm_add_epi8(x, src1);
			src1      = _mm_srli_si128(src1, 8);

			z             = y;
			y             = _mm_srli_si128(y, 8);
			__m128i temp1 = x;

			/*******/
			z = _mm_sub_epi16(_mm_add_epi16(x, y), z);

			i = _mm_min_epi16(x, y); //min
			x = _mm_max_epi16(x, y); //max
			i = _mm_max_epi16(i, z); //max
			x = _mm_min_epi16(i, x); //min
			x = _mm_add_epi8(x, src1);

			temp1 = _mm_unpacklo_epi64(temp1, x);

			z = y;

			/*******/
			i     = temp2;
			y     = _mm_unpackhi_epi8(temp2, _mm_setzero_si128());
			temp2 = _mm_srli_epi16(temp2, 8);
			temp2 = _mm_shufflelo_epi16(temp2, (0 << 0) + (0 << 2) + (2 << 4) + (2 << 6));
			temp2 = _mm_shufflehi_epi16(temp2, (0 << 0) + (0 << 2) + (2 << 4) + (2 << 6));
			temp2 = _mm_add_epi8(temp2, i);
			_mm_store_si128((__m128i*)&output[a * 4 - stride], temp2); // must be %16

			z = _mm_sub_epi16(_mm_add_epi16(x, y), z);

			i    = _mm_min_epi16(x, y); //min
			x    = _mm_max_epi16(x, y); //max
			i    = _mm_max_epi16(i, z); //max
			x    = _mm_min_epi16(i, x); //min
			x    = _mm_add_epi8(x, src2);
			src2 = _mm_srli_si128(src2, 8);

			z     = y;
			y     = _mm_srli_si128(y, 8);
			temp2 = x;

			/*******/
			z = _mm_sub_epi16(_mm_add_epi16(x, y), z);

			i = _mm_min_epi16(x, y); //min
			x = _mm_max_epi16(x, y); //max
			i = _mm_max_epi16(i, z); //max
			x = _mm_min_epi16(i, x); //min
			x = _mm_add_epi8(x, src2);

			temp2 = _mm_unpacklo_epi64(temp2, x);
			temp1 = _mm_packus_epi16(temp1, temp2);

			_mm_store_si128((__m128i*)&output[a * 4], temp1);

			z = y;
		}
	} else { // width is not mod 4, change pixel store to unaligned move

		// 52
		for (; a < end; a += 4) {
			__m128i b = _mm_cvtsi32_si128(*(unsigned int*)&bsrc[a]);
			__m128i g = _mm_cvtsi32_si128(*(unsigned int*)&gsrc[a]);
			__m128i r = _mm_cvtsi32_si128(*(unsigned int*)&rsrc[a]);
			b         = _mm_unpacklo_epi8(b, g);
			r         = _mm_unpacklo_epi8(r, _mm_setzero_si128());

			__m128i src1 = _mm_unpacklo_epi16(b, r);
			__m128i src2 = _mm_unpackhi_epi8(src1, _mm_setzero_si128());
			src1         = _mm_unpacklo_epi8(src1, _mm_setzero_si128());

			__m128i temp2 = _mm_load_si128((__m128i*)&output[a * 4 - stride]); // must be %16
			__m128i y     = _mm_unpacklo_epi8(temp2, _mm_setzero_si128());
			z             = _mm_sub_epi16(_mm_add_epi16(x, y), z);

			__m128i i = _mm_min_epi16(x, y); //min
			x         = _mm_max_epi16(x, y); //max
			i         = _mm_max_epi16(i, z); //max
			x         = _mm_min_epi16(i, x); //min
			x         = _mm_add_epi8(x, src1);
			src1      = _mm_srli_si128(src1, 8);

			z             = y;
			y             = _mm_srli_si128(y, 8);
			__m128i temp1 = x;

			/*******/
			z = _mm_sub_epi16(_mm_add_epi16(x, y), z);

			i = _mm_min_epi16(x, y); //min
			x = _mm_max_epi16(x, y); //max
			i = _mm_max_epi16(i, z); //max
			x = _mm_min_epi16(i, x); //min
			x = _mm_add_epi8(x, src1);

			temp1 = _mm_unpacklo_epi64(temp1, x);

			z = y;

			/*******/
			i     = temp2;
			y     = _mm_unpackhi_epi8(temp2, _mm_setzero_si128());
			temp2 = _mm_srli_epi16(temp2, 8);
			temp2 = _mm_shufflelo_epi16(temp2, (0 << 0) + (0 << 2) + (2 << 4) + (2 << 6));
			temp2 = _mm_shufflehi_epi16(temp2, (0 << 0) + (0 << 2) + (2 << 4) + (2 << 6));
			temp2 = _mm_add_epi8(temp2, i);
			_mm_store_si128((__m128i*)&output[a * 4 - stride], temp2); // must be %16

			z = _mm_sub_epi16(_mm_add_epi16(x, y), z);

			i    = _mm_min_epi16(x, y); //min
			x    = _mm_max_epi16(x, y); //max
			i    = _mm_max_epi16(i, z); //max
			x    = _mm_min_epi16(i, x); //min
			x    = _mm_add_epi8(x, src2);
			src2 = _mm_srli_si128(src2, 8);

			z     = y;
			y     = _mm_srli_si128(y, 8);
			temp2 = x;

			/*******/
			z = _mm_sub_epi16(_mm_add_epi16(x, y), z);

			i = _mm_min_epi16(x, y); //min
			x = _mm_max_epi16(x, y); //max
			i = _mm_max_epi16(i, z); //max
			x = _mm_min_epi16(i, x); //min
			x = _mm_add_epi8(x, src2);

			temp2 = _mm_unpacklo_epi64(temp2, x);
			temp1 = _mm_packus_epi16(temp1, temp2);

			_mm_storeu_si128((__m128i*)&output[a * 4], temp1); // only change from aligned version

			z = y;
		}
	}

	// finish off any remaining pixels (0-3 pixels)
	for (; a < width * height; a++) {
		__m128i src = _mm_cvtsi32_si128(bsrc[a] + (gsrc[a] << 8) + (rsrc[a] << 16));
		src         = _mm_unpacklo_epi8(src, _mm_setzero_si128());

		__m128i y = _mm_cvtsi32_si128(*(int*)&output[a * 4 - stride]);
		output[a * 4 - stride + 0] += output[a * 4 - stride + 1];
		output[a * 4 - stride + 2] += output[a * 4 - stride + 1];
		y = _mm_unpacklo_epi8(y, _mm_setzero_si128());
		z = _mm_subs_epu16(_mm_add_epi16(x, y), z);

		__m128i i = _mm_min_epi16(x, y); //min
		x         = _mm_max_epi16(x, y); //max
		i         = _mm_max_epi16(i, z); //max
		x         = _mm_min_epi16(x, i); //min
		x         = _mm_add_epi8(x, src);

		*(int*)&output[a * 4] = _mm_cvtsi128_si32(_mm_packus_epi16(x, x));

		z = y;
	}

	// finish recorrilating top row
	for (a = stride * (height - 1); a < stride * height; a += 4) {
		output[a] += output[a + 1];
		output[a + 2] += output[a + 1];
	}
}

#if 0  // reference for non simd implementation
void Interleave_And_Restore_Old_Unaligned(unsigned char* bsrc, unsigned char* gsrc,
                                          unsigned char* rsrc, unsigned char* dst,
                                          unsigned char* buffer, bool rgb24, unsigned int width,
                                          unsigned int height) {
	const uintptr_t stride = align_round(width * 3, 4);
	unsigned char*  output = (rgb24) ? dst : buffer;

	output[0] = bsrc[0];
	output[1] = gsrc[0];
	output[2] = rsrc[0];

	for (uintptr_t a = 1; a < width; a++) {
		output[a * 3 + 0] = bsrc[a] + output[a * 3 - 3];
		output[a * 3 + 1] = gsrc[a] + output[a * 3 - 2];
		output[a * 3 + 2] = rsrc[a] + output[a * 3 - 1];
	}

	output[width * 3 + 0] = bsrc[width] + output[0];
	output[width * 3 + 1] = gsrc[width] + output[1];
	output[width * 3 + 2] = rsrc[width] + output[2];

	for (uintptr_t a = width + 1; a < width * height; a++) {
		int x             = output[a * 3 - 3];
		int y             = output[a * 3 - width * 3];
		int z             = x + y - output[a * 3 - width * 3 - 3];
		output[a * 3 + 0] = bsrc[a] + median(x, y, z);

		x                 = output[a * 3 - 2];
		y                 = output[a * 3 - width * 3 + 1];
		z                 = x + y - output[a * 3 - width * 3 - 2];
		output[a * 3 + 1] = gsrc[a] + median(x, y, z);

		x                 = output[a * 3 - 1];
		y                 = output[a * 3 - width * 3 + 2];
		z                 = x + y - output[a * 3 - width * 3 - 1];
		output[a * 3 + 2] = rsrc[a] + median(x, y, z);
	}

	for (uintptr_t a = 0; a < width * height * 3; a += 3) {
		output[a + 0] += output[a + 1];
		output[a + 2] += output[a + 1];
	}

	memcpy(output + width * height * 3, output + width * height * 3 - stride,
	       height * stride - width * height * 3);
	if (!rgb24) {
		for (uintptr_t y = 0; y < height; y++) {
			for (uintptr_t x = 0; x < width; x++) {
				dst[y * width * 4 + x * 4 + 0] = buffer[y * stride + x * 3 + 0];
				dst[y * width * 4 + x * 4 + 1] = buffer[y * stride + x * 3 + 1];
				dst[y * width * 4 + x * 4 + 2] = buffer[y * stride + x * 3 + 2];
				dst[y * width * 4 + x * 4 + 3] = 255;
			}
		}
	}
}
#endif // 0

void Block_Predict(const unsigned char* source, unsigned char* dest, unsigned int width,
                   unsigned int length, bool rgbmode) {
	Block_Predict_SSE2(source, dest, width, length, rgbmode);
}

void Decorrelate_And_Split_RGB24(const unsigned char* in, unsigned char* rdst, unsigned char* gdst,
                                 unsigned char* bdst, unsigned int width, unsigned int height) {
	Decorrelate_And_Split_RGB24_SSE2(in, rdst, gdst, bdst, width, height);
}

void Decorrelate_And_Split_RGB32(const unsigned char* in, unsigned char* rdst, unsigned char* gdst,
                                 unsigned char* bdst, unsigned int width, unsigned int height) {
	Decorrelate_And_Split_RGB32_SSE2(in, rdst, gdst, bdst, width, height);
}

void Interleave_And_Restore_RGB24(unsigned char* out, const unsigned char* rsrc,
                                  const unsigned char* gsrc, const unsigned char* bsrc,
                                  unsigned int width, unsigned int height) {
	Interleave_And_Restore_RGB24_SSE2(out, rsrc, gsrc, bsrc, width, height);
}

void Interleave_And_Restore_RGB32(unsigned char* out, const unsigned char* rsrc,
                                  const unsigned char* gsrc, const unsigned char* bsrc,
                                  unsigned int width, unsigned int height) {
	Interleave_And_Restore_RGB32_SSE2(out, rsrc, gsrc, bsrc, width, height);
}
