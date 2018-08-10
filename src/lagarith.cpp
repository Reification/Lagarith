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

//
// video sequence internal implementation
//

class VideoSequenceImpl {
public:
	VideoSequenceImpl() {}
	~VideoSequenceImpl() {}

	bool Initialize(const FrameDimensions& frameDims, uint32_t frameCount);

	bool LoadLagsFile(const std::string& lagsFilePath);

	bool SaveLagsFile(const std::string& lagsFilePath) const;

	FrameDimensions GetFrameDimensions() const;

	uint32_t GetFrameCount() const;

	// returns nullptr if frameIndex out of range.
	uint8_t* GetFrameRaster(uint32_t frameIndex) const;

	uint8_t* AddFrame(const FrameDimensions& frameDims, const uint8_t* pRasterSrc);

	bool AddEmptyFrames(uint32_t frameCount);

private:
	using RasterBuf = std::unique_ptr<uint8_t[]>;

private:
	std::vector<RasterBuf> m_frames;
	FrameDimensions        m_frameDims;
};

bool VideoSequenceImpl::Initialize(const FrameDimensions& frameDims, uint32_t frameCount) {
	m_frames.clear();

	if (!frameDims.IsValid()) {
		m_frameDims = FrameDimensions();
		return false;
	}

	m_frameDims = frameDims;

	return AddEmptyFrames(frameCount);
}

bool VideoSequenceImpl::LoadLagsFile(const std::string& lagsFilePath) {
	return false;
}

bool VideoSequenceImpl::SaveLagsFile(const std::string& lagsFilePath) const {
	return false;
}

FrameDimensions VideoSequenceImpl::GetFrameDimensions() const {
	return m_frameDims;
}

uint32_t VideoSequenceImpl::GetFrameCount() const {
	return (uint32_t)m_frames.size();
}

uint8_t* VideoSequenceImpl::GetFrameRaster(uint32_t frameIndex) const {
	if (frameIndex < m_frames.size()) {
		return m_frames[frameIndex].get();	
	}

	return nullptr;
}

uint8_t* VideoSequenceImpl::AddFrame(const FrameDimensions& frameDims, const uint8_t* pRasterSrc) {
	if (!pRasterSrc || !frameDims.IsValid() || frameDims != m_frameDims) {
		return nullptr;
	}

	const uint32_t frameSizeBytes = m_frameDims.GetSizeBytes();
	uint8_t*       pRaster        = new uint8_t[frameSizeBytes];

	m_frames.push_back(RasterBuf(pRaster));
	memcpy_s(pRaster, frameSizeBytes, pRasterSrc, frameSizeBytes);

	return pRaster;
}

bool VideoSequenceImpl::AddEmptyFrames(uint32_t frameCount) {
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

//
// Public API Implementation
//

VideoSequence::VideoSequence()
  : m_impl(std::unique_ptr<VideoSequenceImpl>(new VideoSequenceImpl())) {}

VideoSequence::VideoSequence(const std::string& lagsFilePath) : VideoSequence() {
	m_impl->LoadLagsFile(lagsFilePath);
}

VideoSequence::VideoSequence(const FrameDimensions& frameDims, uint32_t frameCount)
  : VideoSequence() {
	m_impl->Initialize(frameDims, frameCount);
}

VideoSequence::~VideoSequence() {}

bool VideoSequence::Initialize(const FrameDimensions& frameDims, uint32_t frameCount) {
	// check for impl - instance can't be copied but may have been moved.
	if (m_impl.get()) {
		return m_impl->Initialize(frameDims, frameCount);
	}

	assert(false && "instance corrupted or was invalidated with std::move()");
	return false;
}

bool VideoSequence::LoadLagsFile(const std::string& lagsFilePath) {
	if (m_impl.get()) {
		return m_impl->LoadLagsFile(lagsFilePath);
	}

	assert(false && "instance corrupted or was invalidated with std::move()");
	return false;
}

bool VideoSequence::SaveLagsFile(const std::string& lagsFilePath) const {
	if (m_impl.get()) {
		return m_impl->SaveLagsFile(lagsFilePath);
	}

	assert(false && "instance corrupted or was invalidated with std::move()");
	return false;
}

FrameDimensions VideoSequence::GetFrameDimensions() const {
	if (m_impl.get()) {
		return m_impl->GetFrameDimensions();
	}

	assert(false && "instance corrupted or was invalidated with std::move()");
	return FrameDimensions();
}

uint32_t VideoSequence::GetFrameCount() const {
	if (m_impl.get()) {
		return m_impl->GetFrameCount();
	}

	assert(false && "instance corrupted or was invalidated with std::move()");
	return 0;
}

uint8_t* VideoSequence::GetFrameRaster(uint32_t frameIndex) const {
	if (m_impl.get()) {
		return m_impl->GetFrameRaster(frameIndex);
	}

	assert(false && "instance corrupted or was invalidated with std::move()");
	return nullptr;
}

uint8_t* VideoSequence::AddFrame(const FrameDimensions& frameDims, const uint8_t* pRasterSrc) {
	if (m_impl.get()) {
		return m_impl->AddFrame(frameDims, pRasterSrc);
	}

	assert(false && "instance corrupted or was invalidated with std::move()");
	return nullptr;
}

bool VideoSequence::AddEmptyFrames(uint32_t frameCount) {
	if (m_impl.get()) {
		return m_impl->AddEmptyFrames(frameCount);
	}

	assert(false && "instance corrupted or was invalidated with std::move()");
	return false;
}

} // namespace Lagarith