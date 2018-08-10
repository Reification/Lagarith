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

class VideoSequenceImpl
{
public:
};

VideoSequence::VideoSequence() {}
VideoSequence::VideoSequence(const std::string& lagsFilePath) {}
VideoSequence::VideoSequence(const FrameDimensions& frameDims, uint32_t frameCount) {}
VideoSequence::~VideoSequence() {}

void VideoSequence::Initialize(const FrameDimensions& frameDims, uint32_t frameCount) {}

bool VideoSequence::LoadLagsFile(const std::string& lagsFilePath) {
	return false;
}

bool VideoSequence::SaveLagsFile(const std::string& lagsFilePath) const {
	return false;
}

FrameDimensions VideoSequence::GetFrameDimensions() const {
	return FrameDimensions();
}

uint32_t VideoSequence::GetFrameCount() const {
	return 0;
}

uint8_t* VideoSequence::GetFrameData(uint32_t frameIndex) const {
	return nullptr;
}

uint8_t* VideoSequence::AddFrame(const uint8_t* pRasterSrc) {
	return nullptr;
}

} // namespace Lagarith