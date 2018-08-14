//	Lagarith v1.3.27, copyright 2011 by Ben Greenwood.
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
#include "lagarith.h"
#include "lagarith_internal.h"

namespace Lagarith {

bool Codec::DecompressBegin(const FrameDimensions& frameDims) {
	if (started == 0x1337) {
		assert(false && "Reset() was not called before re-initialing!");
		return false;
	}

	started = 0;

	int buffer_size;

	width  = frameDims.w;
	height = frameDims.h;
	format = frameDims.bpp;

	length = width * height * format / 8;

	buffer_size = length + 2048;
	if (format >= RGB24) {
		buffer_size = align_round(width * 4, 8) * height + 2048;
	}

	buffer  = (uint8_t*)lag_aligned_malloc(buffer, buffer_size, 16, "Codec::buffer (decompress)");
	buffer2 = (uint8_t*)lag_aligned_malloc(prev, buffer_size, 16, "Codec::prev (decompress)");

	if (!buffer || !buffer2) {
		return false;
	}

#if LAGARITH_MULTITHREAD_SUPPORT
	if (multithreading) {
		if (!InitThreads(false)) {
			return false;
		}
	}
#endif
	started = 0x1337;
	return true;
}

// release resources when decompression is done
void Codec::DecompressEnd() {
	assert(!isCompressing() && "Calling DecompressEnd() after CompressBegin()!");

	if (isDecompressing()) {
		if (multithreading) {
			EndThreads();
		}

		lag_aligned_free(buffer, "Codec::buffer (decompress)");
		lag_aligned_free(buffer2, "Codec::buffer2 (decompress)");
		cObj->FreeCompressBuffers();

		started = 0;
	}
}

void Codec::Decode3Channels(uint8_t* dst1, uint32_t len1, uint8_t* dst2, uint32_t len2,
                            uint8_t* dst3, uint32_t len3) {
	const uint8_t* src1 = in + 9;
	const uint8_t* src2 = in + *(uint32_t*)(in + 1);
	const uint8_t* src3 = in + *(uint32_t*)(in + 5);

#if LAGARITH_MULTITHREAD_SUPPORT
	if (!multithreading)
#endif
	{
		cObj->Uncompact(src1, dst1, len1);
		cObj->Uncompact(src2, dst2, len2);
		cObj->Uncompact(src3, dst3, len3);
		return;
	}
#if LAGARITH_MULTITHREAD_SUPPORT
	else {
		int size1 = *(uint32_t*)(in + 1);
		int size2 = *(uint32_t*)(in + 5);
		int size3 = compressed_size - size2;
		size2 -= size1;
		size1 -= 9;

		// Compressed size should aproximate decoding time.
		// The priority of the largest channel is boosted to improve
		// load balancing when there are fewer cores than channels
		if (size1 >= size2 && size1 >= size3) {
			if (threads[0].priority != THREAD_PRIORITY_BELOW_NORMAL) {
				SetThreadPriority(threads[0].thread, THREAD_PRIORITY_BELOW_NORMAL);
				SetThreadPriority(threads[1].thread, THREAD_PRIORITY_BELOW_NORMAL);
				threads[0].priority = THREAD_PRIORITY_BELOW_NORMAL;
				threads[1].priority = THREAD_PRIORITY_BELOW_NORMAL;
			}
		} else if (size2 >= size3) {
			if (threads[0].priority != THREAD_PRIORITY_ABOVE_NORMAL) {
				SetThreadPriority(threads[0].thread, THREAD_PRIORITY_ABOVE_NORMAL);
				SetThreadPriority(threads[1].thread, THREAD_PRIORITY_NORMAL);
				threads[0].priority = THREAD_PRIORITY_ABOVE_NORMAL;
				threads[1].priority = THREAD_PRIORITY_NORMAL;
			}
		} else {
			if (threads[1].priority != THREAD_PRIORITY_ABOVE_NORMAL) {
				SetThreadPriority(threads[0].thread, THREAD_PRIORITY_NORMAL);
				SetThreadPriority(threads[1].thread, THREAD_PRIORITY_ABOVE_NORMAL);
				threads[0].priority = THREAD_PRIORITY_NORMAL;
				threads[1].priority = THREAD_PRIORITY_ABOVE_NORMAL;
			}
		}

		threads[0].source = src2;
		threads[0].dest   = dst2;
		threads[0].length = len2;
		SetEvent(threads[0].StartEvent);

		threads[1].source = src3;
		threads[1].dest   = dst3;
		threads[1].length = len3;
		SetEvent(threads[1].StartEvent);

		cObj->Uncompact(src1, dst1, len1);

		{
			HANDLE events[2] = {threads[0].DoneEvent, threads[1].DoneEvent};
			WaitForMultipleObjects(2, &events[0], true, INFINITE);
		}
	}
#endif // LAGARITH_MULTITHREAD_SUPPORT
}

// decompress a typical RGB24 frame
void Codec::ArithRGBDecompress() {
	const uint32_t pixels = width * height;

	uint8_t* bdst = buffer;
	uint8_t* gdst = buffer + pixels;
	uint8_t* rdst = buffer + pixels * 2;

	Decode3Channels(bdst, pixels, gdst, pixels, rdst, pixels);

	if (format == RGB24) {
		Interleave_And_Restore_RGB24(out, rdst, gdst, bdst, width, height);
	} else {
		Interleave_And_Restore_RGB32(out, rdst, gdst, bdst, width, height);
	}
}


// decompress a RGB24 pixel frame
void Codec::SetSolidFrameRGB24(const uint32_t r, const uint32_t g, const uint32_t b) {
	const uint32_t stride = align_round(width * 3, 4);
	if (r == g && r == b) {
		memset(out, r, stride * height);
	}
	for (uint32_t y = 0; y < height; y++) {
		for (uint32_t x = 0; x < width; x++) {
			out[y * stride + x * 3 + 0] = b;
			out[y * stride + x * 3 + 1] = g;
			out[y * stride + x * 3 + 2] = r;
		}
	}
}

void Codec::SetSolidFrameRGB32(const uint32_t r, const uint32_t g, const uint32_t b,
                               const uint32_t a) {
	if (r == g && r == b && r == a) {
		memset(out, r, width * height * 4);
	}
	uint32_t pixel = b + (g << 8) + (r << 16) + (a << 24);
	for (uint32_t x = 0; x < width * height; x++) {
		((unsigned int*)out)[x] = pixel;
	}
}

bool Codec::Decompress(const void* src, uint32_t compressedFrameSizeBytes, const RasterRef& dst) {
	if (!started) {
		if (!DecompressBegin(dst.GetDims())) {
			return false;
		}
	} else if (!buffer2 || dst.GetDims() != FrameDimensions({(uint16_t)width, (uint16_t)height,
	                                                         (BitsPerPixel)format})) {
		// format changed or last call was to Compress() - must call reset first.
		// we don't want silent incursion of reset/setup overhead.
		// the cost must be clear in the API
		return false;
	}

	out             = dst.GetBufRef({(uint16_t)width, (uint16_t)height, (BitsPerPixel)format});
	in              = (const uint8_t*)src;
	compressed_size = compressedFrameSizeBytes;

	assert(width && height && format && in && out &&
	       "decompression not started or invalid frame parameters!");

	if (!(width && height && format && in && out)) {
		return false;
	}

	if (compressed_size == 0) {
		return true;
	}

	if (compressed_size <= 21) {
		bool     solid = false;
		uint32_t r = 0, g = 0, b = 0;
		if (in[0] == ARITH_RGB24 && compressed_size == 15) {
			// solid frame that wasn't caught by old memcmp logic
			b = in[10];
			g = in[12];
			r = in[14];
			b += g;
			r += g;
			solid = true;
		} else if (in[0] == BYTEFRAME) {
			b = g = r = in[1];
			solid     = true;
		} else if (in[0] == PIXELFRAME) {
			b     = in[1];
			g     = in[2];
			r     = in[3];
			solid = true;
		}
		if (solid) {
			if (format == RGB24) {
				SetSolidFrameRGB24(r, g, b);
			} else {
				SetSolidFrameRGB32(r, g, b, 0xff);
			}
			return true;
		}
	}

	switch (in[0]) {
	case ARITH_RGB24: ArithRGBDecompress(); break;
	// redundant check just in case size is incorrect
	case BYTEFRAME:
	case PIXELFRAME: {
		uint32_t r = 0, g = 0, b = 0;
		if (in[0] == BYTEFRAME) {
			b = g = r = in[1];
		} else if (in[0] == PIXELFRAME) {
			b = in[1];
			g = in[2];
			r = in[3];
		}
		if (format == RGB24) {
			SetSolidFrameRGB24(r, g, b);
		} else {
			SetSolidFrameRGB32(r, g, b, 0xff);
		}
	} break;

	default: assert(false && "unrecognized frame type!"); return false;
	}

	return true;
}

} // namespace Lagarith
