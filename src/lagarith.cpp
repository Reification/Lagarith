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

bool VideoSequence::LoadLagsFile(const std::string& lagsFilePath, CacheMode cacheMode) {
	LagsFile src;

	Initialize(FrameDimensions());

	if (!src.OpenRead(lagsFilePath)) {
		return false;
	}

	if (!Initialize(src.GetFrameDimensions(), cacheMode)) {
		return false;
	}

	if (cacheMode == CacheMode::kRaster) {
		AddEmptyFrames(src.GetFrameCount());

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
			uint8_t* pCompressed = new uint8_t[sizeof(uint32_t) + compressedSize];
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

bool VideoSequence::AddFrame(const RasterRef& frame) {
	const uint8_t* pRasterSrc = frame.GetBufConstRef(m_frameDims);

	if (frame.GetDims() != m_frameDims || !pRasterSrc) {
		return false;
	}

	bool result = true;

	if (m_cacheMode == CacheMode::kRaster) {
		const uint32_t frameSizeBytes = m_frameDims.GetSizeBytes();
		uint8_t*       pRaster        = new uint8_t[frameSizeBytes];

		memcpy_s(pRaster, frameSizeBytes, pRasterSrc, frameSizeBytes);
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
	if (frameIndex >= GetFrameCount() || dstBuf.GetDims() != m_frameDims) {
		return false;
	}

	bool result = true;

	if (m_cacheMode == CacheMode::kRaster) {
		const uint8_t* pSrc = GetRasterFrame(frameIndex);
		memcpy_s(dstBuf.GetBufRef(m_frameDims), dstBuf.GetDims().GetSizeBytes(), pSrc,
		         m_frameDims.GetSizeBytes());
	} else {
		uint32_t    compressedSize = 0;
		const void* pSrc           = GetCompressedFrame(frameIndex, &compressedSize);
		result                     = m_decoder.Decompress(pSrc, compressedSize, dstBuf);
	}

	return result;
}

const void* VideoSequence::GetCompressedFrame(uint32_t  frameIndex,
                                              uint32_t* pCompressedSizeOut) const {
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

bool VideoSequence::AddEmptyFrames(uint32_t frameCount) {
	if (m_cacheMode == CacheMode::kCompressed) {
		return false;
	}

	if (const uint32_t frameSizeBytes = m_frameDims.GetSizeBytes()) {
		for (uint32_t i = 0; i < frameCount; i++) {
			uint8_t* pRaster = new uint8_t[frameSizeBytes];
			memset(pRaster, 0, frameSizeBytes);
			m_frames.push_back(RasterBuf(pRaster));
		}

		return true;
	}

	return false;
}

} // namespace Lagarith