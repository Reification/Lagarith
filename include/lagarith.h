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

	void SetMultithreaded(bool mt);

	bool CompressBegin(unsigned int w, unsigned int h, unsigned int bitsPerPixel);
	bool Compress(const void* src, void* dst, unsigned int* frameSizeOut);
	void CompressEnd();

	bool DecompressBegin(unsigned int w, unsigned int h, unsigned int bitsPerPixel);
	bool Decompress(const void* src, unsigned int compressedFrameSize, void* dst);
	void DecompressEnd();

private:
	bool InitThreads(int encode);
	void EndThreads();
	void  CompressRGB24(unsigned int* frameSizeOut);

	unsigned int HandleTwoCompressionThreads(unsigned int chan_size);

	void SetSolidFrameRGB24(const unsigned int r, const unsigned int g, const unsigned int b);
	void SetSolidFrameRGB32(const unsigned int r, const unsigned int g, const unsigned int b,
	                        const unsigned int a);
	void Decode3Channels(unsigned char* dst1, unsigned int len1, unsigned char* dst2,
	                     unsigned int len2, unsigned char* dst3, unsigned int len3);
	void ArithRGBDecompress();

private:
	int                  started = 0;
	unsigned char*       buffer  = nullptr;
	unsigned char*       prev    = nullptr;
	const unsigned char* in      = nullptr;
	unsigned char*       out     = nullptr;
	unsigned char*       buffer2 = nullptr;

	unsigned int length = 0;
	unsigned int width  = 0;
	unsigned int height = 0;
	//input format for compressing, output format for decompression. Also the bitdepth.
	unsigned int format = 0;

	bool          multithreading = false;
	unsigned int  compressed_size = 0;

	std::unique_ptr<CompressClass> cObj;
	std::unique_ptr<ThreadData[]> threads;
};

} // Lagarith
