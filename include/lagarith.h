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

// X channel is in alpha slot, but is just padding.
// value is always 0xff, so not referred to as A or alpha.

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

//! 24 bit pixel byte order of rasters is always b, g, r
struct PixelRGB {
	uint8_t b;
	uint8_t g;
	uint8_t r;
};

//! 32 bit pixel byte order of rasters is always b, g, r, a
struct PixelRGBX {
	union {
		struct : PixelRGB {
			uint8_t x;
		} bgrx;
		uint32_t xxbbggrr;
	};
};

//! Reference to a raster buffer with information on size, format and read/write permissions
class RasterRef {
public:
	RasterRef() = default;

	RasterRef(uint8_t* p, FrameDimensions dims)
	  : m_ref(p), m_dims(dims) { /*trust bpp member of dims*/
	}

	RasterRef(PixelRGB* p, FrameDimensions dims) : m_ref(p), m_dims({dims.w, dims.h, kRGB}) {}

	RasterRef(PixelRGBX* p, FrameDimensions dims) : m_ref(p), m_dims({dims.w, dims.h, kRGBX}) {}

	RasterRef(const uint8_t* p, FrameDimensions dims) : m_ref(p), m_dims(dims), m_isConst(true) {
		/*trust bpp member of _dims*/
	}

	RasterRef(const PixelRGB* p, FrameDimensions dims)
	  : m_ref(p), m_dims({dims.w, dims.h, kRGB}), m_isConst(true) {}

	RasterRef(const PixelRGBX* p, FrameDimensions dims)
	  : m_ref(p), m_dims({dims.w, dims.h, kRGBX}), m_isConst(true) {}

	uint8_t* GetBufRef(const FrameDimensions& expected) const {
		return (!m_isConst && (m_dims == expected)) ? (uint8_t*)m_ref.pRGBX : nullptr;
	}

	const uint8_t* GetBufConstRef(const FrameDimensions& expected) const {
		return (m_dims == expected) ? (const uint8_t*)m_ref.pRGBX : nullptr;
	}

	PixelRGB* GetRGBRef(const FrameDimensions& expected) const {
		return (!m_isConst && (m_dims == expected) && (m_dims.bpp == kRGB)) ? m_ref.pRGB : nullptr;
	}

	const PixelRGB* GetRGBConstRef(const FrameDimensions& expected) const {
		return ((m_dims == expected) && (m_dims.bpp == kRGB)) ? m_ref.pRGB : nullptr;
	}

	PixelRGBX* GetRGBXRef(const FrameDimensions& expected) const {
		return (!m_isConst && (m_dims == expected) && (m_dims.bpp == kRGBX)) ? m_ref.pRGBX : nullptr;
	}

	const PixelRGBX* GetRGBXConstRef(const FrameDimensions& expected) const {
		return ((m_dims == expected) && (m_dims.bpp == kRGBX)) ? m_ref.pRGBX : nullptr;
	}

	const FrameDimensions& GetDims() const { return m_dims; }

	bool IsConst() const { return !!m_isConst; }

private:
	union Ref {
		PixelRGB*  pRGB;
		PixelRGBX* pRGBX;
		Ref() = default;
		Ref(void* p) : pRGB((PixelRGB*)p) {}
		Ref(const void* p) : pRGB((PixelRGB*)p) {}
	} m_ref;

	FrameDimensions m_dims = {};

	uint16_t m_isConst = false;
};

class Codec {
public:
	Codec();
	~Codec();

	//! worst case scenario - recommended size of target buffer for calls to Compress()
	static uint32_t GetMaxCompressedSize(const FrameDimensions& frameDims) {
		const uint32_t rgbSize = (frameDims.w * frameDims.h * 3);
		return rgbSize + (rgbSize >> 16) + 0x100;
	}

	/*! compress a single frame. 
	 * initialization overhead incurred on first call since construction or Reset()
	 * returns true on success
	 * returns false when
	 * - last Compress call was made with different frame resolution or pixel format
	 * - last call was to Decompress()
	 * - dst for frameSizeBytesOut is null
	 * - src buffer is null or any source dimension is 0
	 * When changing operation (compress/decompress) or resolution, Reset()
	 * must be called first.
	 */
	bool Compress(const RasterRef& src, void* dst, uint32_t* frameSizeBytesOut);

	/*! compress a single frame. 
	 * initialization overhead incurred on first call since construction or Reset()
	 * returns true on success
	 * returns false when
	 * - last Decompress call was made with different frame resolution or pixel format
	 * - last call was to Compress()
	 * - src pointer is null
	 * - dst buffer is null, marked const, or any dst dimension is 0
	 * When changing operation (compress/decompress) or resolution, Reset()
	 * must be called first.
	 */
	bool Decompress(const void* src, uint32_t compressedFrameSizeBytes, const RasterRef& dst);

	/*! Release all buffers and return to constructed state.
	    Must be called when switching between compression and decompression operations or when
			changing frame resolutions/pixel format
			Called from destructor - does not need to be manually called when not switching operations.
	 */
	void Reset();

	/*! Enable multithreading on systems that support it.
	 * returns true if supported and enabled.
	 * returns false when disabling or not supported.
	 */
	bool SetMultithreaded(bool mt);

private:
	bool CompressBegin(const FrameDimensions& frameDims);
	void CompressEnd();
	bool DecompressBegin(const FrameDimensions& frameDims);
	void DecompressEnd();

	bool InitThreads(int encode);
	void EndThreads();
	void CompressRGB24(unsigned int* frameSizeBytesOut);

	uint32_t HandleTwoCompressionThreads(uint32_t chan_size);

	void SetSolidFrameRGB24(const uint32_t r, const uint32_t g, const uint32_t b);
	void SetSolidFrameRGB32(const uint32_t r, const uint32_t g, const uint32_t b, const uint32_t a);
	void Decode3Channels(uint8_t* dst1, uint32_t len1, uint8_t* dst2, uint32_t len2, uint8_t* dst3,
	                     uint32_t len3);
	void ArithRGBDecompress();
	bool isCompressing() const { return (started == 0x1337) && buffer && prev; }
	bool isDecompressing() const { return (started == 0x1337) && buffer && !prev; }


private:
	int            started         = 0;
	uint8_t*       buffer          = nullptr;
	uint8_t*       buffer2         = nullptr;
	uint8_t*       prev            = nullptr;
	const uint8_t* in              = nullptr;
	uint8_t*       out             = nullptr;
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
	bool ReadFrame(uint32_t frameIdx, const RasterRef& dst);

	/*! reads LAGS-encoded video frame from file into destination buffer, skipping decoding step.
	 * target buffer should be at least as large as specified by Codec::GetMaxCompressedSize()
	 * if pDstCompressedBuf is null, pCompressedBufSize out is set and then true is returned.
	 */
	bool ReadCompressedFrame(uint32_t frameIdx, void* pDstCompressedBuf,
	                         uint32_t* pCompressedBufSizeOut);

	//! write a video frame. only valid on files opened with OpenWrite()
	bool WriteFrame(const RasterRef& src);

	/*! writes LAGS-encoded video frame from source buffer into lags file, skipping encoding step.
	* target buffer should be at least as large as specified by Codec::GetMaxCompressedSize()
	*/
	bool WriteCompressedFrame(const void* pSrcCompressedBuf, uint32_t compressedBufSize);

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
	enum class CacheMode { kRaster, kCompressed };

public:
	VideoSequence() = default;

	//! instantiate and attempt to load video file.
	explicit VideoSequence(const std::string& lagsFilePath,
	                       CacheMode          cacheMode = CacheMode::kRaster) {
		LoadLagsFile(lagsFilePath, cacheMode);
	}

	//! instantiate and initialize frame size, allocate frameCount frames if non-zero
	explicit VideoSequence(const FrameDimensions& frameDims,
	                       CacheMode              cacheMode = CacheMode::kRaster) {
		Initialize(frameDims, cacheMode);
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
	bool Initialize(const FrameDimensions& frameDims, CacheMode cacheMode = CacheMode::kRaster);

	/*! will always initialize or destructively reinitialize the video sequence.
	 *returns true on success, false for missing or incompatible file.
	 */
	bool LoadLagsFile(const std::string& lagsFilePath, CacheMode cacheMode = CacheMode::kRaster);

	//! returns true on success, false if file can't be saved or if current frame count is zero.
	bool SaveLagsFile(const std::string& lagsFilePath) const;

	//! returns current allocated frame count
	uint32_t GetFrameCount() const { return (uint32_t)m_frames.size(); }

	/*! returns pointer to cached raster frame at specified index.
	 * returns nullptr if frameIndex out of range or sequence is in compressed mode
	 */
	uint8_t* GetRasterFrame(uint32_t frameIndex) const {
		return (m_cacheMode == CacheMode::kRaster && (frameIndex < m_frames.size())) ? m_frames[frameIndex].get() : nullptr;
	}

	/*! returns pointer to cached compressed frame at specified index.
	 * returns nullptr if frameIndex out of range or sequence is in raster mode
	 */
	const void* GetCompressedFrame(uint32_t frameIndex, uint32_t* pCompressedSizeOut) const;

	/*!
	 * allocates a new frame and copies or decodes from pCompressedData into it.
	 * video sequence must have been initialized first.
	 */
	bool AddFrame(const void* pCompressedData, uint32_t compressedSize);

	/*! decodes or copies frame at specified index into target buffer.
	 * returns false if frameIndex out of range or dstBuf is invalid.
	 */
	bool DecodeFrame(uint32_t frameIndex, const RasterRef& dstBuf);

	/*! allocates new frame and copies or encodes from pRaster into it.
	 * video sequence must have been initialized first.
	 * dstBuf must be valid and match current initalized size and format.
	 * returns pointer to newly allocated frame raster on success.
	 * returns nullptr if VideoSequence has not been initialized or dstBuf does not meet requirements.
	 */
	bool AddFrame(const RasterRef& frame);

	/*! allocates frameCount new frames.
	 * only valid when CacheMode is kRaster
	 * returns true on success
	 * returns false if
	 * - Initialize() or LoadLagsFile() has not been successfully called
	 * - CacheMode is kCompressed
	 */
	bool AddEmptyFrames(uint32_t frameCount);

	//! returns currently configured frame dimensions
	const FrameDimensions& GetFrameDimensions() const { return m_frameDims; }

	//! returns currently configured cache mode
	CacheMode GetCacheMode() const { return m_cacheMode; }

private:
	using RasterBuf = std::unique_ptr<uint8_t[]>;

private:
	std::vector<RasterBuf> m_frames;
	FrameDimensions        m_frameDims;
	Codec                  m_encoder;
	Codec                  m_decoder;
	CacheMode              m_cacheMode = CacheMode::kRaster;
};

} // namespace Lagarith
