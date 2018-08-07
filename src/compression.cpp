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
#include "lagarith.h"
#include "lagarith_internal.h"

namespace Lagarith {

// initalize the codec for compression
bool Codec::CompressBegin(uint32_t w, uint32_t h, uint32_t bitsPerChannel) {
	if (started == 0x1337) {
		CompressEnd();
	}
	started = 0;

	width  = w;
	height = h;
	format = bitsPerChannel;

	length = width * height * format / 8;

	uint32_t buffer_size;
	buffer_size = align_round(width, 16) * height * 4 + 1024;

	buffer  = (uint8_t*)lag_aligned_malloc(buffer, buffer_size, 16, "buffer");
	buffer2 = (uint8_t*)lag_aligned_malloc(buffer2, buffer_size, 16, "buffer2");
	prev    = (uint8_t*)lag_aligned_malloc(prev, buffer_size, 16, "prev");

	if (!buffer || !buffer2 || !prev) {
		lag_aligned_free(buffer, "buffer");
		lag_aligned_free(buffer2, "buffer2");
		lag_aligned_free(prev, "prev");
		return false;
	}

	if (!cObj->InitCompressBuffers(width * height)) {
		return false;
	}

#if LAGARITH_MULTITHREAD_SUPPORT
	if (multithreading) {
		if (!InitThreads(true)) {
			return false;
		}
	}
#endif

	started = 0x1337;

	return true;
}

// release resources when compression is done

void Codec::CompressEnd() {
	if (started == 0x1337) {
		if (multithreading) {
			EndThreads();
		}

		lag_aligned_free(buffer, "buffer");
		lag_aligned_free(buffer2, "buffer2");
		lag_aligned_free(prev, "prev");
		cObj->FreeCompressBuffers();
	}
	started = 0;
}

// see this doc entry for setting thread priority of std::thread using native handle + pthreads.
// https://en.cppreference.com/w/cpp/thread/thread/native_handle

uint32_t Codec::HandleTwoCompressionThreads(uint32_t chan_size) {
#if LAGARITH_MULTITHREAD_SUPPORT
	uint32_t channel_sizes[3];
	channel_sizes[0] = chan_size;

	uint32_t current_size = chan_size + 9;

	HANDLE events[2] = {threads[0].DoneEvent, threads[1].DoneEvent};

	int pos = WaitForMultipleObjects(2, &events[0], false, INFINITE);
	pos -= WAIT_OBJECT_0;

	ThreadData* finished = &threads[pos];
	ThreadData* running  = &threads[!pos];

	((uint32_t*)(out + 1))[pos] = current_size;
	channel_sizes[pos + 1]      = finished->size;
	memcpy(out + current_size, (uint8_t*)finished->dest, channel_sizes[pos + 1]);
	current_size += finished->size;

	pos                         = !pos;
	((uint32_t*)(out + 1))[pos] = current_size;

	WaitForSingleObject(running->DoneEvent, INFINITE);

	finished               = running;
	channel_sizes[pos + 1] = finished->size;

	memcpy(out + current_size, (uint8_t*)finished->dest, channel_sizes[pos + 1]);
	current_size += finished->size;

	if (channel_sizes[0] >= channel_sizes[1] && channel_sizes[0] >= channel_sizes[2]) {
		if (threads[0].priority != THREAD_PRIORITY_BELOW_NORMAL) {
			SetThreadPriority(threads[0].thread, THREAD_PRIORITY_BELOW_NORMAL);
			SetThreadPriority(threads[1].thread, THREAD_PRIORITY_BELOW_NORMAL);
			threads[0].priority = THREAD_PRIORITY_BELOW_NORMAL;
			threads[1].priority = THREAD_PRIORITY_BELOW_NORMAL;
		}
	} else if (channel_sizes[1] >= channel_sizes[2]) {
		if (threads[0].priority != THREAD_PRIORITY_ABOVE_NORMAL) {
			SetThreadPriority(threads[0].thread, THREAD_PRIORITY_ABOVE_NORMAL);
			SetThreadPriority(threads[1].thread, THREAD_PRIORITY_NORMAL);
			threads[0].priority = THREAD_PRIORITY_ABOVE_NORMAL;
			threads[0].priority = THREAD_PRIORITY_NORMAL;
		}
	} else {
		if (threads[1].priority != THREAD_PRIORITY_ABOVE_NORMAL) {
			SetThreadPriority(threads[0].thread, THREAD_PRIORITY_NORMAL);
			SetThreadPriority(threads[1].thread, THREAD_PRIORITY_ABOVE_NORMAL);
			threads[0].priority = THREAD_PRIORITY_NORMAL;
			threads[0].priority = THREAD_PRIORITY_ABOVE_NORMAL;
		}
	}
	return current_size;
#else  // LAGARITH_MULTITHREAD_SUPPORT
	return 0;
#endif // LAGARITH_MULTITHREAD_SUPPORT
}

// Encode a typical RGB24 frame, input can be RGB24 or RGB32
void Codec::CompressRGB24(unsigned int* frameSizeOut) {
	//const uint8_t* src       = in;
	const uint32_t pixels    = width * height;
	const uint32_t block_len = align_round(pixels + 32, 16);
	uint8_t*       bplane    = buffer;
	uint8_t*       gplane    = buffer + block_len;
	uint8_t*       rplane    = buffer + block_len * 2;


	// convert the interleaved channels to planer, aligning if needed
	if (format == RGB24) {
		Decorrelate_And_Split_RGB24(in, rplane, gplane, bplane, width, height);
	} else {
		Decorrelate_And_Split_RGB32(in, rplane, gplane, bplane, width, height);
	}

	int size = 0;
#if LAGARITH_MULTITHREAD_SUPPORT
	if (!multithreading)
#endif
	{ // perform prediction

		uint8_t* bpred = buffer2;
		uint8_t* gpred = buffer2 + block_len;
		uint8_t* rpred = buffer2 + block_len * 2;

		Block_Predict(bplane, bpred, width, pixels, true);
		Block_Predict(gplane, gpred, width, pixels, true);
		Block_Predict(rplane, rpred, width, pixels, true);

		size = cObj->Compact(bpred, out + 9, pixels);
		size += 9;

		*(uint32_t*)(out + 1) = size;
		size += cObj->Compact(gpred, out + size, pixels);

		*(uint32_t*)(out + 5) = size;
		size += cObj->Compact(rpred, out + size, pixels);

	}
#if LAGARITH_MULTITHREAD_SUPPORT
	else {
		uint8_t* rcomp = prev;
		uint8_t* gcomp = prev + align_round(length / 2 + 128, 16);

		threads[0].source = gplane;
		threads[0].dest   = gcomp;
		threads[0].length = pixels;
		SetEvent(threads[0].StartEvent);
		threads[1].source = rplane;
		threads[1].dest   = rcomp;
		threads[1].length = pixels;
		SetEvent(threads[1].StartEvent);

		uint8_t* bpred = buffer2;
		Block_Predict(bplane, bpred, width, block_len, true);

		uint32_t blue_size = cObj->Compact(bpred, out + 9, pixels);

		size = HandleTwoCompressionThreads(blue_size);
	}
#endif // LAGARITH_MULTITHREAD_SUPPORT

	out[0] = ARITH_RGB24;

	// minimum size possible, indicates the frame was one solid color
	if (size == 15) {
		if (in[0] == in[1] && in[0] == in[2]) {
			out[0] = BYTEFRAME;
			out[1] = in[0];
			size   = 2;
		} else {
			out[0] = PIXELFRAME;
			out[1] = in[0];
			out[2] = in[1];
			out[3] = in[2];
			size   = 4;
		}
	}

	*frameSizeOut = size;
}

// called to compress a frame; the actual compression will be
// handed off to other functions depending on the color space and settings

bool Codec::Compress(const void* src, void* dst, unsigned int* frameSizeOut) {
	in  = (const uint8_t*)src;
	out = (uint8_t*)dst;

	assert(width && height && format && src && dst && frameSizeOut &&
	       "CompressBegin not called or invalid frame parameters");

	if (width && height && format && src && dst && frameSizeOut) {
		CompressRGB24(frameSizeOut);
		return true;
	}

	return false;
}

} // namespace Lagarith
