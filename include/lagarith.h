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
#include <vector>

namespace Lagarith {
struct ThreadData;
class CompressClass;

enum BitsPerPixel : uint16_t { kRGB = 24, kRGBX = 32 };

struct FrameDimensions {
	uint16_t     w   = 0;
	uint16_t     h   = 0;
	BitsPerPixel bpp = BitsPerPixel::kRGBX;

	bool operator==(const FrameDimensions& rhs) const {
		return w == rhs.w && h == rhs.h && bpp == rhs.bpp;
	}

	bool operator!=(const FrameDimensions& rhs) const { return !operator==(rhs); }

	bool IsValid() const {
		return w && h && (bpp == BitsPerPixel::kRGB || bpp == BitsPerPixel::kRGBX);
	}

	uint32_t GetPixelCount() const { return w * h; }
	uint32_t GetBytesPerPixel() const { return ((uint32_t)bpp) >> 3; }
	uint32_t GetSizeBytes() const { return w * h * GetBytesPerPixel(); }
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
	int            started         = 0;
	uint8_t*       buffer          = nullptr;
	uint8_t*       prev            = nullptr;
	const uint8_t* in              = nullptr;
	uint8_t*       out             = nullptr;
	uint8_t*       buffer2         = nullptr;
	uint32_t       length          = 0;
	uint32_t       width           = 0;
	uint32_t       height          = 0;
	uint32_t       format          = 0;
	uint32_t       compressed_size = 0;
	bool           multithreading  = false;

	std::unique_ptr<CompressClass> cObj;
	std::unique_ptr<ThreadData[]>  threads;
};

/*! basic video-only file i/o
 * initial testing shows size is ~25% smaller than sum separate PNGs saved via stb_image
 */

namespace Impl {
	struct LagsFileState;
}

class LagsFile {
public:
	LagsFile();
	~LagsFile();

	//! open a lags file for reading
	bool OpenRead(const std::string& path);

	//! open a lags file for writing
	bool OpenWrite(const std::string& path, const FrameDimensions& frameDims);

	//! read a specified video frame. valid on files opened with either OpenRead() or OpenWrite()
	bool ReadFrame(uint32_t frameIdx, uint8_t* pDstRaster);

	//! write a video frame. only valid on files opened with OpenWrite()
	bool WriteFrame(const uint8_t* pSrcRaster);

	//! close a file that has been opened for either reading or writing. resets all state.
	bool Close();

	//! is this file open for either reading or writing
	bool IsOpen() const { return m_frameDims.IsValid(); }

	//! can frames be written to this file
	bool IsWriteMode() const;

	const FrameDimensions& GetFrameDimensions() const { return m_frameDims; }
	uint32_t               GetFrameCount() const { return m_frameCount; }

private:
	std::unique_ptr<Impl::LagsFileState> m_state;
	FrameDimensions                      m_frameDims;
	uint32_t                             m_frameCount = 0;
};


/*! simple RAM-resident video sequence class.
 * supports lags format file i/o
 */
class VideoSequence {
public:
	VideoSequence() = default;

	//! instantiate and attempt to load video file.
	explicit VideoSequence(const std::string& lagsFilePath) { LoadLagsFile(lagsFilePath); }

	//! instantiate and initialize frame size, allocate frameCount frames if non-zero
	explicit VideoSequence(const FrameDimensions& frameDims, uint32_t frameCount = 0) {
		Initialize(frameDims, frameCount);
	}

	~VideoSequence() = default;

	//! copying not permitted
	VideoSequence(const VideoSequence&) = delete;

	//! copying not permitted
	VideoSequence& operator=(const VideoSequence&) = delete;

	/*! will always initialize or destructively reinitialize the video sequence.
	 * returns true if frameDims are valid
	 * returns false if any member of frameDims is 0. frameCount of 0 is valid.
	 */
	bool Initialize(const FrameDimensions& frameDims, uint32_t frameCount = 0);

	/*! will always initialize or destructively reinitialize the video sequence.
	 *returns true on success, false for missing or incompatible file.
	 */
	bool LoadLagsFile(const std::string& lagsFilePath);

	//! returns true on success, false if file can't be saved or if current frame count is zero.
	bool SaveLagsFile(const std::string& lagsFilePath) const;

	//! returns currently configured frame dimensions
	const FrameDimensions& GetFrameDimensions() const { return m_frameDims; }

	//! returns current allocated frame count
	uint32_t GetFrameCount() const { return (uint32_t)m_frames.size(); }

	//! returns nullptr if frameIndex out of range.
	uint8_t* GetFrameRaster(uint32_t frameIndex) const {
		return (frameIndex < m_frames.size()) ? m_frames[frameIndex].get() : nullptr;
	}

	/*! allocates new frame and copies from pRaster into it.
	 * video sequence must have been initialized first.
	 * frameDims must match current initalized format.
	 * pRaster must be non-null and sized/formatted to match specified FrameDimensions
	 * returns pointer to newly allocated frame raster on success.
	 * returns nullptr if VideoSequence has not been initialized or frameDims does not match video sequence dimensions.
	 */
	uint8_t* AddFrame(const FrameDimensions& frameDims, const uint8_t* pRasterSrc);

	//! allocates frameCount new frames. returns true on success, false if Initialize() or LoadLagsFile() has not been successfully called.
	bool AddEmptyFrames(uint32_t frameCount);

private:
	using RasterBuf = std::unique_ptr<uint8_t[]>;

private:
	std::vector<RasterBuf> m_frames;
	FrameDimensions        m_frameDims;
};

} // namespace Lagarith
