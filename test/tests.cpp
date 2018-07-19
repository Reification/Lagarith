#include "tests.h"
#include "lagarith.h"

#include <stdlib.h>
#include <memory>
#include <vector>

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
		int  j   = first;
		int  end = last + step;

		m_frameCount = 0;
		for (uint32_t i = 0; i < (sizeof(m_frames) / sizeof(m_frames[0])); i++) {
			m_frames[i].free();
		}

		for (uint32_t i = 0; i < (sizeof(m_frames) / sizeof(m_frames[0])) && (j != end);
		     i++, j += step) {
			sprintf_s(imageName, format, j);
			if (!m_frames[i].load(imageName, channels)) {
				fprintf(stderr, "failed loading image %s.\n", imageName);
				m_frameCount = 0;
				return false;
			}

			if (i && (m_frames[i].m_width != m_frames[0].m_width ||
			          m_frames[i].m_height != m_frames[0].m_height)) {
				fprintf(stderr, "image %s does not match frame 0 dimensions.\n", imageName);
				m_frameCount = 0;
				return false;
			}

			m_frameCount++;
		}

		if (j != end) {
			fprintf(stderr, "warning: only loaded from %d to %d.\n", first, j);
		}

		return true;
	}

	int      GetWidth() const { return m_frames[0].m_width; }
	int      GetHeight() const { return m_frames[0].m_height; }
	int      GetChannels() const { return m_frames[0].m_channels; }
	stbi_uc* GetBits(int frameIdx) const {
		return frameIdx < m_frameCount ? m_frames[frameIdx].m_pBits : nullptr;
	}
	unsigned int GetFrameSize() const { return GetWidth() * GetHeight() * GetChannels(); }
	unsigned int GetSequenceSize() const { return GetFrameSize() * m_frameCount; }

	int    m_frameCount = 0;
	Raster m_frames[5];
};

static bool testEncodeDecode(int channelCount) {
	std::unique_ptr<CodecInst> pCode(new CodecInst());
	RasterSequence             testFrames;
	int                        err = 0;

	if (!testFrames.load(channelCount, "frame_%02d.png", 0, 4)) {
		return false;
	}

	pCode->multithreading = true;
	pCode->use_alpha      = false;

	err = pCode->CompressBegin(testFrames.GetWidth(), testFrames.GetHeight(), testFrames.GetChannels() * 8);

	if (err) {
		fprintf(stderr, "CompressBegin failed with err %d\n", err);
		return false;
	}

	const void*          pSrcBits            = nullptr;
	int                  i                   = 0;
	unsigned int         frameSizes[5]       = {};
	void*                compressedFrames[5] = {};
	std::vector<uint8_t> compressedBuf;

	const unsigned int inputFrameSize = testFrames.GetFrameSize();
	compressedBuf.resize((size_t)(inputFrameSize * testFrames.m_frameCount *  1.1));

	uint8_t* pDstBits = compressedBuf.data();

	for (; !!(pSrcBits = testFrames.GetBits(i)); i++) {
		err = pCode->Compress(i, pSrcBits, pDstBits, &(frameSizes[i]));

		if (err) {
			fprintf(stderr, "Compress failed with err %d\n", err);
			pCode->CompressEnd();
			return false;
		}

		compressedFrames[i] = pDstBits;
		pDstBits += frameSizes[i];
	}

	pCode->CompressEnd();

	return true;
}

bool testEncodeDecodeRGB() {
	if (testEncodeDecode(3)) {
		printf("testEncodeDecodeRGB passed.\n");
		return true;
	}

	return false;
}

bool testEncodeDecodeRGBA() {
	if (testEncodeDecode(4)) {
		printf("testEncodeDecodeRGBA passed.\n");
		return true;
	}

	return false;
}
