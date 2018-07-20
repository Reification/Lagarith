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
#include <stdint.h>
#include "interface.h"
#include "lagarith.h"
#include "prediction.h"
#include "threading.h"
#include <float.h>

DWORD CodecInst::CompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
	if (started == 0x1337) {
		CompressEnd();
	}
	started = 0;

	const int error = CompressQuery(lpbiIn, lpbiOut);
	if (error != ICERR_OK) {
		return error;
	}

	return CompressBegin(lpbiIn->biWidth, lpbiIn->biHeight, lpbiIn->biBitCount);
}
// initalize the codec for compression
DWORD CodecInst::CompressBegin(unsigned int w, unsigned int h, unsigned int bitsPerChannel) {
	if (started == 0x1337) {
		CompressEnd();
	}
	started = 0;

	width  = w;
	height = h;
	format = bitsPerChannel;

	length = width * height * format / 8;

	unsigned int buffer_size;
	buffer_size = align_round(width, 16) * height * 4 + 1024;

	buffer  = (unsigned char*)lag_aligned_malloc(buffer, buffer_size, 16, "buffer");
	buffer2 = (unsigned char*)lag_aligned_malloc(buffer2, buffer_size, 16, "buffer2");
	prev    = (unsigned char*)lag_aligned_malloc(prev, buffer_size, 16, "prev");

	if (!buffer || !buffer2 || !prev) {
		lag_aligned_free(buffer, "buffer");
		lag_aligned_free(buffer2, "buffer2");
		lag_aligned_free(prev, "prev");
		return ICERR_MEMORY;
	}

	if (!cObj.InitCompressBuffers(width * height)) {
		return ICERR_MEMORY;
	}

	if (multithreading) {
		int code = InitThreads(true);
		if (code != ICERR_OK) {
			return code;
		}
	}

	started = 0x1337;

	return ICERR_OK;
}

// get the maximum size a compressed frame will take;
// 105% of image size + 1KB should be plenty even for random static

DWORD CodecInst::CompressGetSize(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
	return (DWORD)(
	  align_round(lpbiIn->biWidth, 16) * lpbiIn->biHeight * lpbiIn->biBitCount / 8 * 1.05 + 1024);
}

// release resources when compression is done

DWORD CodecInst::CompressEnd() {
	if (started == 0x1337) {
		if (multithreading) {
			EndThreads();
		}

		lag_aligned_free(buffer, "buffer");
		lag_aligned_free(buffer2, "buffer2");
		lag_aligned_free(prev, "prev");
		cObj.FreeCompressBuffers();
	}
	started = 0;
	return ICERR_OK;
}

// see this doc entry for setting thread priority of std::thread using native handle + pthreads.
// https://en.cppreference.com/w/cpp/thread/thread/native_handle

unsigned int CodecInst::HandleTwoCompressionThreads(unsigned int chan_size) {
	unsigned int channel_sizes[3];
	channel_sizes[0] = chan_size;

	unsigned int current_size = chan_size + 9;

	HANDLE events[2];
	events[0] = threads[0].DoneEvent;
	events[1] = threads[1].DoneEvent;

	int pos = WaitForMultipleObjects(2, &events[0], false, INFINITE);
	pos -= WAIT_OBJECT_0;

	ThreadData* finished = &threads[pos];
	ThreadData* running  = &threads[!pos];

	((DWORD*)(out + 1))[pos] = current_size;
	channel_sizes[pos + 1]   = finished->size;
	memcpy(out + current_size, (unsigned char*)finished->dest, channel_sizes[pos + 1]);
	current_size += finished->size;

	pos                      = !pos;
	((DWORD*)(out + 1))[pos] = current_size;

	WaitForSingleObject(running->DoneEvent, INFINITE);

	finished               = running;
	channel_sizes[pos + 1] = finished->size;

	memcpy(out + current_size, (unsigned char*)finished->dest, channel_sizes[pos + 1]);
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
}

unsigned int CodecInst::HandleThreeCompressionThreads(unsigned int chan_size) {
	unsigned int channel_sizes[4];
	channel_sizes[0] = chan_size;

	unsigned int current_size = chan_size + 13;

	HANDLE events[3];
	events[0] = threads[0].DoneEvent;
	events[1] = threads[1].DoneEvent;
	events[2] = threads[2].DoneEvent;

	int positions[] = {0, 1, 2};

	for (int a = 3; a > 0; a--) {
		int pos = WaitForMultipleObjects(a, &events[0], false, INFINITE);
		pos -= WAIT_OBJECT_0;

		int thread_num = positions[pos];
		positions[pos] = positions[a - 1];
		events[pos]    = events[a - 1];

		ThreadData* finished = &threads[thread_num];

		unsigned int fsize              = finished->size;
		((DWORD*)(out + 1))[thread_num] = current_size;
		channel_sizes[thread_num + 1]   = fsize;

		memcpy(out + current_size, (unsigned char*)finished->dest, fsize);
		current_size += fsize;
	}

	if (channel_sizes[3] >= channel_sizes[1] && channel_sizes[3] >= channel_sizes[2] &&
	    channel_sizes[3] >= channel_sizes[0]) {
		if (threads[2].priority != THREAD_PRIORITY_ABOVE_NORMAL) {
			if (threads[0].priority != THREAD_PRIORITY_NORMAL) {
				SetThreadPriority(threads[0].thread, THREAD_PRIORITY_NORMAL);
				threads[0].priority = THREAD_PRIORITY_NORMAL;
			}
			if (threads[1].priority != THREAD_PRIORITY_NORMAL) {
				SetThreadPriority(threads[1].thread, THREAD_PRIORITY_NORMAL);
				threads[1].priority = THREAD_PRIORITY_NORMAL;
			}
			SetThreadPriority(threads[2].thread, THREAD_PRIORITY_ABOVE_NORMAL);
			threads[2].priority = THREAD_PRIORITY_ABOVE_NORMAL;
		}
	} else if (channel_sizes[0] >= channel_sizes[1] && channel_sizes[0] >= channel_sizes[2]) {
		if (threads[0].priority != THREAD_PRIORITY_BELOW_NORMAL) {
			SetThreadPriority(threads[0].thread, THREAD_PRIORITY_BELOW_NORMAL);
			threads[0].priority = THREAD_PRIORITY_BELOW_NORMAL;
		}
		if (threads[1].priority != THREAD_PRIORITY_BELOW_NORMAL) {
			SetThreadPriority(threads[1].thread, THREAD_PRIORITY_BELOW_NORMAL);
			threads[1].priority = THREAD_PRIORITY_BELOW_NORMAL;
		}
		if (threads[2].priority != THREAD_PRIORITY_BELOW_NORMAL) {
			SetThreadPriority(threads[2].thread, THREAD_PRIORITY_BELOW_NORMAL);
			threads[2].priority = THREAD_PRIORITY_BELOW_NORMAL;
		}
	} else if (channel_sizes[1] >= channel_sizes[2]) {
		if (threads[0].priority != THREAD_PRIORITY_ABOVE_NORMAL) {
			if (threads[2].priority != THREAD_PRIORITY_NORMAL) {
				SetThreadPriority(threads[2].thread, THREAD_PRIORITY_NORMAL);
				threads[2].priority = THREAD_PRIORITY_NORMAL;
			}
			if (threads[1].priority != THREAD_PRIORITY_NORMAL) {
				SetThreadPriority(threads[1].thread, THREAD_PRIORITY_NORMAL);
				threads[1].priority = THREAD_PRIORITY_NORMAL;
			}
			SetThreadPriority(threads[0].thread, THREAD_PRIORITY_ABOVE_NORMAL);
			threads[0].priority = THREAD_PRIORITY_ABOVE_NORMAL;
		}
	} else {
		if (threads[1].priority != THREAD_PRIORITY_ABOVE_NORMAL) {
			if (threads[0].priority != THREAD_PRIORITY_NORMAL) {
				SetThreadPriority(threads[0].thread, THREAD_PRIORITY_NORMAL);
				threads[0].priority = THREAD_PRIORITY_NORMAL;
			}
			if (threads[2].priority != THREAD_PRIORITY_NORMAL) {
				SetThreadPriority(threads[2].thread, THREAD_PRIORITY_NORMAL);
				threads[2].priority = THREAD_PRIORITY_NORMAL;
			}
			SetThreadPriority(threads[1].thread, THREAD_PRIORITY_ABOVE_NORMAL);
			threads[1].priority = THREAD_PRIORITY_ABOVE_NORMAL;
		}
	}

	return current_size;
}

// Encode a typical RGB24 frame, input can be RGB24 or RGB32
int CodecInst::CompressRGB24(unsigned int* frameSizeOut) {
	//const unsigned char* src       = in;
	const unsigned int pixels    = width * height;
	const unsigned int block_len = align_round(pixels + 32, 16);
	unsigned char*     bplane    = buffer;
	unsigned char*     gplane    = buffer + block_len;
	unsigned char*     rplane    = buffer + block_len * 2;


	// convert the interleaved channels to planer, aligning if needed
	if (format == RGB24) {
		Decorrilate_And_Split_RGB24(in, rplane, gplane, bplane, width, height, &performance);
	} else {
		Decorrilate_And_Split_RGB32(in, rplane, gplane, bplane, width, height, &performance);
	}

	int size = 0;
	if (!multithreading) { // perform prediction

		unsigned char* bpred = buffer2;
		unsigned char* gpred = buffer2 + block_len;
		unsigned char* rpred = buffer2 + block_len * 2;

		Block_Predict(bplane, bpred, width, pixels, true);
		Block_Predict(gplane, gpred, width, pixels, true);
		Block_Predict(rplane, rpred, width, pixels, true);

		size = cObj.Compact(bpred, out + 9, pixels);
		size += 9;

		*(UINT32*)(out + 1) = size;
		size += cObj.Compact(gpred, out + size, pixels);

		*(UINT32*)(out + 5) = size;
		size += cObj.Compact(rpred, out + size, pixels);

	} else {
		unsigned char* rcomp = prev;
		unsigned char* gcomp = prev + align_round(length / 2 + 128, 16);

		threads[0].source = gplane;
		threads[0].dest   = gcomp;
		threads[0].length = pixels;
		SetEvent(threads[0].StartEvent);
		threads[1].source = rplane;
		threads[1].dest   = rcomp;
		threads[1].length = pixels;
		SetEvent(threads[1].StartEvent);

		unsigned char* bpred = buffer2;
		Block_Predict(bplane, bpred, width, block_len, true);

		unsigned int blue_size = cObj.Compact(bpred, out + 9, pixels);

		size = HandleTwoCompressionThreads(blue_size);
	}

	if ((width % 4) == 0) {
		out[0] = ARITH_RGB24;
	} else {
		// old versions didn't handle the padding correctly,
		// this tag lets the codec handle the versions differently
		out[0] = UNALIGNED_RGB24;
	}

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
	return ICERR_OK;
}

// Compress an RGBA frame (alpha compression is enabled and the input is RGB32)
int CodecInst::CompressRGBA(unsigned int* frameSizeOut) {
	//const unsigned char* src       = in;
	const unsigned int pixels    = width * height;
	const unsigned int block_len = align_round(pixels + 32, 16);
	unsigned char*     bplane    = buffer;
	unsigned char*     gplane    = buffer + block_len;
	unsigned char*     rplane    = buffer + block_len * 2;
	unsigned char*     aplane    = buffer + block_len * 3;

	// convert the interleaved channels to planer
	Decorrilate_And_Split_RGBA(in, rplane, gplane, bplane, aplane, width, height, &performance);

	int size;
	out[0] = ARITH_ALPHA;
	if (!multithreading) {
		unsigned char* bpred = buffer2;
		unsigned char* gpred = buffer2 + block_len;
		unsigned char* rpred = buffer2 + block_len * 2;
		unsigned char* apred = buffer2 + block_len * 3;

		Block_Predict(bplane, bpred, width, pixels, true);
		Block_Predict(gplane, gpred, width, pixels, true);
		Block_Predict(rplane, rpred, width, pixels, true);
		Block_Predict(aplane, apred, width, pixels, true);

		size = 13;
		size += cObj.Compact(bpred, out + size, pixels);
		*(int*)(out + 1) = size;
		size += cObj.Compact(gpred, out + size, pixels);
		*(int*)(out + 5) = size;
		size += cObj.Compact(rpred, out + size, pixels);
		*(int*)(out + 9) = size;
		size += cObj.Compact(apred, out + size, pixels);
	} else {
		unsigned char* rcomp = prev;
		unsigned char* bpred = buffer2;
		unsigned char* gcomp = buffer2 + align_round(pixels + 64, 16);
		unsigned char* acomp = prev + align_round(pixels * 2 + 128, 16);

		threads[0].source = gplane;
		threads[0].dest   = gcomp;
		threads[0].length = pixels;
		SetEvent(threads[0].StartEvent);

		threads[1].source = rplane;
		threads[1].dest   = rcomp;
		threads[1].length = pixels;
		SetEvent(threads[1].StartEvent);

		threads[2].source = aplane;
		threads[2].dest   = acomp;
		threads[2].length = pixels;
		SetEvent(threads[2].StartEvent);

		Block_Predict(bplane, bpred, width, block_len, true);
		unsigned int size_blue = cObj.Compact(bpred, out + 13, pixels);

		size = HandleThreeCompressionThreads(size_blue);
	}

	if (size == 21) {
		out[0] = PIXELFRAME_ALPHA;
		out[1] = in[0];
		out[2] = in[1];
		out[3] = in[2];
		out[4] = in[3];
		size   = 5;
	}

	*frameSizeOut = size;
	return ICERR_OK;
}

// called to compress a frame; the actual compression will be
// handed off to other functions depending on the color space and settings

DWORD CodecInst::Compress(int frameNum, const void* src, void* dst, unsigned int* frameSizeOut) {
	(void)frameNum;
	in  = (const unsigned char*)src;
	out = (unsigned char*)dst;

	assert(width && height && format);

#if 0
	if (frameNum == 0) {
		if (started != 0x1337) {
			if (int error = CompressBegin(icinfo->lpbiInput, icinfo->lpbiOutput) != ICERR_OK)
				return error;
		}
	} else
#endif
	if (nullframes) {
		// compare in two parts, video is probably more likely to change in middle than at bottom
		unsigned int pos = length / 2 + 15;
		pos &= (~15);
		if (!memcmp(in + pos, prev + pos, length - pos) && !memcmp(in, prev, pos)) {
			frameSizeOut = 0;
			return ICERR_OK;
		}
	}

	// TMPGEnc (and probably some other programs) like to change the floating point
	// precision. This can occasionally produce rounding errors when scaling the probablity
	// ranges to a power of 2, which in turn produces corrupted video. Here the code checks
	// the FPU control word and sets the precision correctly if needed.
#if USE_CONTROL_FP
	unsigned int fpuword = _controlfp(0, 0);
	if (!(fpuword & _PC_53) || (fpuword & _MCW_RC)) {
		_controlfp(_PC_53 | _RC_NEAR, _MCW_PC | _MCW_RC);
#	ifndef LAGARITH_RELEASE
		static bool firsttime = true;
		if (firsttime) {
			firsttime = false;
			MessageBox(HWND_DESKTOP, "Floating point control word is not set correctly, correcting it",
			           "Error", MB_OK | MB_ICONEXCLAMATION);
		}
#	endif
	}
#endif

	int ret_val;

	if (format == RGB24 || (format == RGB32 && !use_alpha)) {
		ret_val = CompressRGB24(frameSizeOut);
	} else {
		ret_val = CompressRGBA(frameSizeOut);
	}

	if (nullframes) {
		memcpy(prev, in, length);
	}

#if USE_CONTROL_FP
	if (!(fpuword & _PC_53) || (fpuword & _MCW_RC)) {
		_controlfp(fpuword, _MCW_PC | _MCW_RC);
	}
#endif

	return (DWORD)ret_val;
}

DWORD CodecInst::Compress(ICCOMPRESS* icinfo, DWORD dwSize) {
	out = (unsigned char*)icinfo->lpOutput;
	in  = (unsigned char*)icinfo->lpInput;

	if (icinfo->lFrameNum == 0) {
		if (started != 0x1337) {
			if (int error = CompressBegin(icinfo->lpbiInput, icinfo->lpbiOutput) != ICERR_OK)
				return error;
		}
	} else if (nullframes) {
		// compare in two parts, video is probably more likely to change in middle than at bottom
		unsigned int pos = length / 2 + 15;
		pos &= (~15);
		if (!memcmp(in + pos, prev + pos, length - pos) && !memcmp(in, prev, pos)) {
			icinfo->lpbiOutput->biSizeImage = 0;
			*icinfo->lpdwFlags              = 0;
			return ICERR_OK;
		}
	}
	if (icinfo->lpckid) {
		*icinfo->lpckid = 'd' << 8 | 'c'; //'cd';
	}

	// TMPGEnc (and probably some other programs) like to change the floating point
	// precision. This can occasionally produce rounding errors when scaling the probablity
	// ranges to a power of 2, which in turn produces corrupted video. Here the code checks
	// the FPU control word and sets the precision correctly if needed.
#if USE_CONTROL_FP
	unsigned int fpuword = _controlfp(0, 0);
	if (!(fpuword & _PC_53) || (fpuword & _MCW_RC)) {
		_controlfp(_PC_53 | _RC_NEAR, _MCW_PC | _MCW_RC);
#	ifndef LAGARITH_RELEASE
		static bool firsttime = true;
		if (firsttime) {
			firsttime = false;
			MessageBox(HWND_DESKTOP, "Floating point control word is not set correctly, correcting it",
			           "Error", MB_OK | MB_ICONEXCLAMATION);
		}
#	endif
	}
#endif

	int          ret_val;
	unsigned int frameSize = 0;

	if (format == RGB24 || (format == RGB32 && !use_alpha)) {
		ret_val = CompressRGB24(&frameSize);
	} else {
		ret_val = CompressRGBA(&frameSize);
	}
	icinfo->dwFrameSize = frameSize;

	if (nullframes) {
		memcpy(prev, in, length);
	}

	if (ret_val == ICERR_OK) {
		*icinfo->lpdwFlags = AVIIF_KEYFRAME;
	}

#if USE_CONTROL_FP
	if (!(fpuword & _PC_53) || (fpuword & _MCW_RC)) {
		_controlfp(fpuword, _MCW_PC | _MCW_RC);
	}
#endif

	return (DWORD)ret_val;
}

//MessageBox (HWND_DESKTOP, msg, "Error", MB_OK | MB_ICONEXCLAMATION);