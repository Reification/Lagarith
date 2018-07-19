#include "tests.h"
#include "lagarith.h"

#include <stdlib.h>
#include <memory>

// only needed for loading test images.
#include "stb/stb.h"
#include "stb/stb_image.h"

struct Raster {
	stbi_uc* m_pBits    = nullptr;
	int      m_width    = 0;
	int      m_height   = 0;
	int      m_channels = 0;

	~Raster() { free(); }

	bool load(const char* path, int channels = 0) {
		free();
		m_pBits = stbi_load(path, &m_width, &m_height, &m_channels, channels);
		if (channels && m_pBits) {
			m_channels = channels;
		}
		return !!m_pBits;
	}

	void free() {
		if (m_pBits) {
			stbi_image_free(m_pBits);
			m_pBits = nullptr;
		}
		m_width = m_height = m_channels = 0;
	}
};

struct RasterSequence {
	bool load(int channels, const char* format, int first, int last, int step = 1) {
		char imageName[128];
		int j = first;
		int  end = last + step;
		
		for (uint32_t i = 0; i < (sizeof(m_frames) / sizeof(m_frames[0])) && (j != end);
		     i++, j += step) {
			sprintf_s(imageName, format, j);
			if (!m_frames[i].load(imageName, channels)) {
				fprintf(stderr, "failed loading image %s.\n", imageName);
				return false;
			}
		}

		if (j != end) {
			fprintf(stderr, "warning: only loaded from %d to %d.\n", first, j);
		}

		return true;
	}
	Raster m_frames[5];
};

bool testEncodeDecodeRGB() {
	std::unique_ptr<CodecInst> pCode(new CodecInst());
	RasterSequence             testFrames;
	if (!testFrames.load(3, "frame_%02d.png", 0, 4)) {
		return false;
	}
	return false;
}

bool testEncodeDecodeRGBA() {
	std::unique_ptr<CodecInst> pCode(new CodecInst());
	RasterSequence             testFrames;
	if (!testFrames.load(4, "frame_%02d.png", 0, 4)) {
		return false;
	}
	return false;
}
