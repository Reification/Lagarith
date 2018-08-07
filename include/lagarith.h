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

#include <memory>

namespace Lagarith {
struct ThreadData;
class CompressClass;

class Codec {
public:
	Codec();
	~Codec();

	bool CompressBegin(uint32_t w, uint32_t h, uint32_t bitsPerPixel);
	bool Compress(const void* src, void* dst, unsigned int* frameSizeOut);
	void CompressEnd();

	bool DecompressBegin(uint32_t w, uint32_t h, uint32_t bitsPerPixel);
	bool Decompress(const void* src, uint32_t compressedFrameSize, void* dst);
	void DecompressEnd();

	void SetMultithreaded(bool mt);

private:
	bool InitThreads(int encode);
	void EndThreads();
	void CompressRGB24(unsigned int* frameSizeOut);

	uint32_t HandleTwoCompressionThreads(uint32_t chan_size);

	void SetSolidFrameRGB24(const uint32_t r, const uint32_t g, const uint32_t b);
	void SetSolidFrameRGB32(const uint32_t r, const uint32_t g, const uint32_t b,
	                        const uint32_t a);
	void Decode3Channels(uint8_t* dst1, uint32_t len1, uint8_t* dst2,
	                     uint32_t len2, uint8_t* dst3, uint32_t len3);
	void ArithRGBDecompress();

private:
	int                  started = 0;
	uint8_t*       buffer  = nullptr;
	uint8_t*       prev    = nullptr;
	const uint8_t* in      = nullptr;
	uint8_t*       out     = nullptr;
	uint8_t*       buffer2 = nullptr;

	uint32_t length = 0;
	uint32_t width  = 0;
	uint32_t height = 0;
	//input format for compressing, output format for decompression. Also the bitdepth.
	uint32_t format = 0;
	uint32_t compressed_size = 0;

	std::unique_ptr<CompressClass> cObj;
	std::unique_ptr<ThreadData[]>  threads;
	bool multithreading = false;
};

} // Lagarith
