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
#include <string>

namespace Lagarith {
struct ThreadData;
class CompressClass;

enum BitsPerPixel : uint8_t { kRGB = 24, kRGBX = 32 };

struct FrameDimensions {
	uint16_t     w   = 0;
	uint16_t     h   = 0;
	BitsPerPixel bpp = BitsPerPixel::kRGBX;

	const uint32_t GetPixelCount() const { return w * h; }
	const uint32_t GetSizeBytes() const { return w * h * (bpp >> 3); }
};

class Codec {
public:
	Codec();
	~Codec();

	bool CompressBegin(const FrameDimensions& frameDims);
	bool Compress(const void* src, void* dst, uint32_t* frameSizeBytesOut);
	void CompressEnd();

	bool DecompressBegin(const FrameDimensions& frameDims);
	bool Decompress(const void* src, uint32_t compressedFrameSizeBytes, void* dst);
	void DecompressEnd();

	void SetMultithreaded(bool mt);

private:
	bool InitThreads(int encode);
	void EndThreads();
	void CompressRGB24(unsigned int* frameSizeBytesOut);

	uint32_t HandleTwoCompressionThreads(uint32_t chan_size);

	void SetSolidFrameRGB24(const uint32_t r, const uint32_t g, const uint32_t b);
	void SetSolidFrameRGB32(const uint32_t r, const uint32_t g, const uint32_t b, const uint32_t a);
	void Decode3Channels(uint8_t* dst1, uint32_t len1, uint8_t* dst2, uint32_t len2, uint8_t* dst3,
	                     uint32_t len3);
	void ArithRGBDecompress();

private:
	int            started = 0;
	uint8_t*       buffer  = nullptr;
	uint8_t*       prev    = nullptr;
	const uint8_t* in      = nullptr;
	uint8_t*       out     = nullptr;
	uint8_t*       buffer2 = nullptr;

	uint32_t length = 0;
	uint32_t width  = 0;
	uint32_t height = 0;
	//input format for compressing, output format for decompression. Also the bitdepth.
	uint32_t format          = 0;
	uint32_t compressed_size = 0;

	std::unique_ptr<CompressClass> cObj;
	std::unique_ptr<ThreadData[]>  threads;
	bool                           multithreading = false;
};

class VideoSequenceImpl;

//
// simple RAM-resident video sequence class.
//
class VideoSequence {
public:
	VideoSequence();
	VideoSequence(const std::string& lagsFilePath);
	VideoSequence(const FrameDimensions& frameDims, uint32_t frameCount = 0);
	~VideoSequence();

	// these will initialize or destructively reinitialize the video sequence.
	void Initialize(const FrameDimensions& frameDims, uint32_t frameCount = 0);
	bool LoadLagsFile(const std::string& lagsFilePath);

	bool SaveLagsFile(const std::string& lagsFilePath) const;

	FrameDimensions GetFrameDimensions() const;
	uint32_t        GetFrameCount() const;

	// returns nullptr if frameIndex out of range.
	uint8_t* GetFrameData(uint32_t frameIndex) const;

	// video sequence must have been initialized first.
	// pRaster must be nullptr or sized/formatted to match current video frame format
	// return value is pointer to new raster
	uint8_t* AddFrame(const uint8_t* pRasterSrc);
	uint8_t* AddEmptyFrame() { return AddFrame(nullptr); }

private:
	std::unique_ptr<VideoSequenceImpl> m_impl;
};

} // Lagarith
