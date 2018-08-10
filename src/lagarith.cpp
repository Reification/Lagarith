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
	try {
		if (started == 0x1337) {
			if (buffer2) {
				CompressEnd();
			} else {
				DecompressEnd();
			}
		}
		started = 0;
	} catch (...) {
	};
}

void Codec::SetMultithreaded(bool mt) {
	assert(width == 0 &&
	       "multithreading must be configured before calling CompressBegin or DecompressBegin.");
#if LAGARITH_MULTITHREAD_SUPPORT
	if (!width)
		multithreading = mt;
#endif
}

bool VideoSequence::Initialize(const FrameDimensions& frameDims, uint32_t frameCount) {
	m_frames.clear();

	if (!frameDims.IsValid()) {
		m_frameDims = FrameDimensions();
		return false;
	}

	m_frameDims = frameDims;

	return AddEmptyFrames(frameCount);
}

bool VideoSequence::LoadLagsFile(const std::string& lagsFilePath) {
	LagsFile src;

	Initialize(FrameDimensions());

	if (!src.OpenRead(lagsFilePath)) {
		return false;
	}

	if (!Initialize(src.GetFrameDimensions(), src.GetFrameCount())) {
		return false;
	}

	for (uint32_t f = 0, e = GetFrameCount(); f < e; f++) {
		if (!src.ReadFrame(f, GetFrameRaster(f))) {
			assert(false && "corrupted lags file!");
			return false;
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

	for (uint32_t f = 0, e = GetFrameCount(); f < e; f++) {
		if (!dst.WriteFrame(GetFrameRaster(f))) {
			// should only happen if out of disk space.
			dst.Close();
			remove(lagsFilePath.c_str());
			return false;
		}
	}

	return true;
}

uint8_t* VideoSequence::AddFrame(const FrameDimensions& frameDims, const uint8_t* pRasterSrc) {
	if (!pRasterSrc || !frameDims.IsValid() || frameDims != m_frameDims) {
		return nullptr;
	}

	const uint32_t frameSizeBytes = m_frameDims.GetSizeBytes();
	uint8_t*       pRaster        = new uint8_t[frameSizeBytes];

	m_frames.push_back(RasterBuf(pRaster));
	memcpy_s(pRaster, frameSizeBytes, pRasterSrc, frameSizeBytes);

	return pRaster;
}

bool VideoSequence::AddEmptyFrames(uint32_t frameCount) {
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