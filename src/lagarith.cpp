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

Codec::Codec() : cObj(new CompressClass()), threads(new ThreadData[2]) {}

Codec::~Codec() {
	Reset();
}

void Codec::Reset() {
	try {
		if (isCompressing()) {
			CompressEnd();
		} else if (isDecompressing()) {
			DecompressEnd();
		}
	} catch (...) {
	};
}

bool Codec::SetMultithreaded(bool mt) {
	assert(width == 0 &&
	       "multithreading must be configured before calling CompressBegin or DecompressBegin.");
#if LAGARITH_MULTITHREAD_SUPPORT
	if (!width)
		multithreading = mt;
	return multithreading;
#else
	return false;
#endif
}

//
// VideoSequence implementation
//

bool VideoSequence::Initialize(const FrameDimensions& frameDims, CacheMode cacheMode) {
	m_frames.clear();

	if (!frameDims.IsValid()) {
		m_frameDims = FrameDimensions();
		return false;
	}

	m_frameDims = frameDims;
	m_cacheMode = cacheMode;

	return true;
}

bool VideoSequence::LoadLagsFile(const std::string& lagsFilePath, BitsPerPixel cacheBPP,
                                 CacheMode cacheMode) {
	LagsFile src;

	Initialize(FrameDimensions());

	if (!src.OpenRead(lagsFilePath)) {
		return false;
	}

	{
		FrameDimensions cacheDims = src.GetFrameDimensions();
		cacheDims.bpp             = cacheBPP;

		if (!Initialize(cacheDims, cacheMode)) {
			return false;
		}
	}

	if (cacheMode == CacheMode::kRaster) {
		AllocateRasterFrames(src.GetFrameCount());

		for (uint32_t f = 0, e = src.GetFrameCount(); f < e; f++) {
			if (!src.ReadFrame(f, {GetRasterFrame(f), m_frameDims})) {
				assert(false && "corrupted lags file!");
				return false;
			}
		}
	} else {
		for (uint32_t f = 0, e = src.GetFrameCount(); f < e; f++) {
			uint32_t compressedSize = 0;
			if (!src.ReadCompressedFrame(f, nullptr, &compressedSize)) {
				assert(false && "corrupted lags file!");
				return false;
			}
			uint8_t* pCompressed      = new uint8_t[sizeof(uint32_t) + compressedSize];
			*((uint32_t*)pCompressed) = compressedSize;

			if (!src.ReadCompressedFrame(f, pCompressed + sizeof(uint32_t), &compressedSize)) {
				assert(false && "corrupted lags file!");
				return false;
			}

			m_frames.push_back(RasterBuf(pCompressed));
		}
	}

	return true;
}

bool VideoSequence::SaveLagsFile(const std::string& lagsFilePath) const {
	if (!GetFrameCount()) {
		return false;
	}

	LagsFile dst;

	if (!dst.OpenWrite(lagsFilePath, m_frameDims)) {
		return false;
	}

	if (m_cacheMode == CacheMode::kRaster) {
		for (uint32_t f = 0, e = GetFrameCount(); f < e; f++) {
			if (!dst.WriteFrame({GetRasterFrame(f), m_frameDims})) {
				// should only happen if out of disk space.
				dst.Close();
				remove(lagsFilePath.c_str());
				return false;
			}
		}
	} else {
		for (uint32_t f = 0, e = GetFrameCount(); f < e; f++) {
			uint32_t    compressedSize = 0;
			const void* pCompressed    = GetCompressedFrame(f, &compressedSize);
			if (!pCompressed || !dst.WriteCompressedFrame(pCompressed, compressedSize)) {
				// should only happen if out of disk space.
				dst.Close();
				remove(lagsFilePath.c_str());
				return false;
			}
		}
	}

	return true;
}

static void copyRaster(const RasterRef& src, const RasterRef& dst) {
	if (!src.GetDims().IsRectEqual(dst.GetDims())) {
		assert(false && "internal error. misuse of internal utility.");
		return;
	}

	if (src.GetDims().bpp == dst.GetDims().bpp) {
		memcpy_s(dst.GetBufRef(dst.GetDims()), dst.GetDims().GetSizeBytes(),
		         src.GetBufConstRef(src.GetDims()), src.GetDims().GetSizeBytes());
		return;
	}

	const uint32_t pixCount = src.GetDims().GetPixelCount();

	if (src.GetDims().bpp == BitsPerPixel::kRGB) {
		const uint8_t* spx = src.GetBufConstRef(src.GetDims());
		uint32_t*      dpx = (uint32_t*)dst.GetBufRef(dst.GetDims());

		assert(dst.GetDims().bpp == BitsPerPixel::kRGBX);

		for (uint32_t p = 0; p < pixCount; ++p, spx += 3) {
			dpx[p] = 0xff00000000 | ((uint32_t)spx[2] << 16) | ((uint32_t)spx[1] << 8) | spx[0];
		}

		return;
	}

	if (src.GetDims().bpp == BitsPerPixel::kRGBX) {
		const uint32_t* spx = (const uint32_t*)src.GetBufConstRef(src.GetDims());
		uint8_t*        dpx = dst.GetBufRef(dst.GetDims());

		assert(dst.GetDims().bpp == BitsPerPixel::kRGB);

		for (uint32_t p = 0; p < pixCount; ++p, dpx += 3) {
			dpx[0] = (uint8_t)(spx[p]);
			dpx[1] = (uint8_t)(spx[p] >> 8);
			dpx[2] = (uint8_t)(spx[p] >> 16);
		}

		return;
	}

	assert(false && "unsupported bpp - corrupted arguments or implementation needed.");
}


bool VideoSequence::AddFrame(const RasterRef& frame) {
	if (!frame.GetDims().IsRectEqual(m_frameDims) || !frame.IsValid()) {
		return false;
	}

	bool result = true;

	if (m_cacheMode == CacheMode::kRaster) {
		const uint32_t frameSizeBytes = m_frameDims.GetSizeBytes();
		uint8_t*       pRaster        = new uint8_t[frameSizeBytes];

		copyRaster(frame, {pRaster, m_frameDims});

		m_frames.push_back(RasterBuf(pRaster));
	} else {
		RasterBuf intermed(new uint8_t[Codec::GetMaxCompressedSize(m_frameDims)]);
		uint32_t  compressedSize = 0;

		result = m_encoder.Compress(frame, intermed.get(), &compressedSize);

		if (result) {
			uint8_t* pCompressed = new uint8_t[sizeof(uint32_t) + compressedSize];

			*((uint32_t*)pCompressed) = compressedSize;
			memcpy_s(pCompressed + sizeof(uint32_t), compressedSize, intermed.get(), compressedSize);

			m_frames.push_back(RasterBuf(pCompressed));
		}
	}

	return result;
}

bool VideoSequence::AddFrame(const void* pCompressedData, uint32_t compressedSize) {
	if (!m_frameDims.IsValid() || !pCompressedData || !compressedSize) {
		return false;
	}

	bool result = true;

	if (m_cacheMode == CacheMode::kCompressed) {
		uint8_t* pDstBuf = new uint8_t[sizeof(uint32_t) + compressedSize];

		*((uint32_t*)pDstBuf) = compressedSize;
		memcpy_s(pDstBuf + sizeof(uint32_t), compressedSize, pCompressedData, compressedSize);

		m_frames.push_back(RasterBuf(pDstBuf));
	} else {
		const uint32_t frameSizeBytes = m_frameDims.GetSizeBytes();
		uint8_t*       pRaster        = new uint8_t[frameSizeBytes];

		result = m_decoder.Decompress(pCompressedData, compressedSize, {pRaster, m_frameDims});

		if (result) {
			m_frames.push_back(RasterBuf(pRaster));
		} else {
			delete[] pRaster;
		}
	}

	return result;
}

bool VideoSequence::DecodeFrame(uint32_t frameIndex, const RasterRef& dstBuf) {
	if (frameIndex >= GetFrameCount() || !dstBuf.GetDims().IsRectEqual(m_frameDims) ||
	    !dstBuf.IsValid()) {
		return false;
	}

	bool result = true;

	if (m_cacheMode == CacheMode::kRaster) {
		copyRaster({GetRasterFrame(frameIndex), m_frameDims}, dstBuf);
	} else {
		uint32_t    compressedSize = 0;
		const void* pSrc           = GetCompressedFrame(frameIndex, &compressedSize);
		result                     = m_decoder.Decompress(pSrc, compressedSize, dstBuf);
	}

	return result;
}

void* VideoSequence::GetCompressedFrame(uint32_t frameIndex, uint32_t* pCompressedSizeOut) const {
	const void* pCompressed =
	  (m_cacheMode == CacheMode::kCompressed && (frameIndex < m_frames.size()))
	    ? m_frames[frameIndex].get()
	    : nullptr;

	if (pCompressed) {
		*pCompressedSizeOut = *((uint32_t*)pCompressed);
		return ((uint8_t*)pCompressed) + sizeof(uint32_t);
	}

	return nullptr;
}

bool VideoSequence::AllocateRasterFrames(uint32_t frameCount) {
	if (m_cacheMode == CacheMode::kRaster) {
		if (const uint32_t frameSizeBytes = m_frameDims.GetSizeBytes()) {
			for (uint32_t i = 0; i < frameCount; i++) {
				uint8_t* pRaster = new uint8_t[frameSizeBytes];

				memset(pRaster, 0, frameSizeBytes);
				m_frames.push_back(RasterBuf(pRaster));
			}

			return true;
		}
	}

	return false;
}

void* VideoSequence::AllocateCompressedFrame(uint32_t compressedSize) {
	if (m_cacheMode == CacheMode::kCompressed) {
		uint8_t* pCompressedBuf      = new uint8_t[sizeof(uint32_t) + compressedSize];

		*((uint32_t*)pCompressedBuf) = compressedSize;

		m_frames.push_back(RasterBuf(pCompressedBuf));

		return pCompressedBuf + sizeof(uint32_t);
	}

	return nullptr;
}

} // namespace Lagarith