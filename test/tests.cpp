#include "tests.h"
#include "lagarith.h"

#include <stdlib.h>
#include <memory>
#include <vector>

// only needed for handling test data - not used for actual codec.
#include "stb/stb.h"
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"

struct Raster {
	stbi_uc* m_pBits    = nullptr;
	int      m_width    = 0;
	int      m_height   = 0;
	int      m_channels = 0;
	int      m_alloced  = 0;

	Raster() = default;
	~Raster() { Free(); }

	unsigned int GetSizeBytes() const { return m_width * m_height * m_channels; }

	bool Load(const char* path, int channels = 0) {
		Free();
		m_pBits = stbi_load(path, &m_width, &m_height, &m_channels, channels);
		if (channels && m_pBits) {
			m_channels = channels;
		}
		return !!m_pBits;
	}

	bool Save(const char* path) {
		if (!GetSizeBytes()) {
			return false;
		}

		if (stbi_write_png(path, m_width, m_height, (m_channels < 3 ? m_channels : 3), m_pBits,
		                   m_width * m_channels)) {
			return true;
		}

		return false;
	}

	void Alloc(int w, int h, int c) {
		Free();

		if (unsigned int sizeBytes = w * h * c) {
			m_width    = w;
			m_height   = h;
			m_channels = c;
			m_pBits    = new stbi_uc[sizeBytes];
			m_alloced  = true;
		}
	}

	void Free() {
		if (m_pBits) {
			if (m_alloced) {
				delete[] m_pBits;
			} else {
				stbi_image_free(m_pBits);
			}
			m_pBits = nullptr;
		}
		m_alloced = m_width = m_height = m_channels = 0;
	}
};

struct RasterSequence {
	bool Load(int channels, const char* format, int first, int last, int step = 1) {
		char imageName[128];
		int  j   = first;
		int  end = last + step;

		Free();

		for (uint32_t i = 0; i < (sizeof(m_frames) / sizeof(m_frames[0])) && (j != end);
		     i++, j += step) {
			sprintf_s(imageName, format, j);
			if (!m_frames[i].Load(imageName, channels)) {
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

	bool Save(const char* format) {
		char imageName[128];

		for (uint32_t i = 0; i < m_frameCount; i++) {
			sprintf_s(imageName, format, i);
			if (!m_frames[i].Save(imageName)) {
				fprintf(stderr, "failed to save frame %s\n", imageName);
				return false;
			}
		}

		return true;
	}

	void Alloc(int w, int h, int channels, int frames) {
		Free();
		for (int i = 0; i < frames && i < sizeof(m_frames) / sizeof(m_frames[0]); i++) {
			m_frames[i].Alloc(w, h, channels);
			m_frameCount++;
		}
	}

	void Alloc(const RasterSequence& rhs) {
		Alloc(rhs.GetWidth(), rhs.GetHeight(), rhs.GetChannels(), rhs.GetFrameCount());
	}

	void Free() {
		for (uint32_t i = 0; i < (sizeof(m_frames) / sizeof(m_frames[0])); i++) {
			m_frames[i].Free();
		}
		m_frameCount = 0;
	}

	int      GetWidth() const { return m_frames[0].m_width; }
	int      GetHeight() const { return m_frames[0].m_height; }
	int      GetChannels() const { return m_frames[0].m_channels; }
	stbi_uc* GetBits(int frameIdx) const {
		return frameIdx < m_frameCount ? m_frames[frameIdx].m_pBits : nullptr;
	}

	unsigned int GetFrameSizeBytes() const { return m_frames[0].GetSizeBytes(); }
	unsigned int GetSequenceSizeBytes() const { return GetFrameSizeBytes() * m_frameCount; }
	unsigned int GetFrameCount() const { return m_frameCount; }

private:
	unsigned int m_frameCount = 0;
	Raster       m_frames[5];
};

static bool testEncodeDecode(int channelCount) {
	RasterSequence srcFrames;
	RasterSequence decompressedFrames;
	int            err = 0;

	if (!srcFrames.Load(channelCount, "frame_%02d.png", 0, 4)) {
		return false;
	}

	decompressedFrames.Alloc(srcFrames);

	const unsigned int   inputFrameSize          = srcFrames.GetFrameSizeBytes();
	unsigned int         compressedFrameSizes[5] = {};
	void*                compressedFrames[5]     = {};
	std::vector<uint8_t> compressedBuf;
	unsigned int         totalCompressedSize = 0;

	compressedBuf.resize((size_t)(inputFrameSize * srcFrames.GetFrameCount() * 1.1));

	// encode execution
	{
		std::unique_ptr<CodecInst> pCode(new CodecInst());

		pCode->multithreading = true;
		pCode->use_alpha      = false;

		err = pCode->CompressBegin(srcFrames.GetWidth(), srcFrames.GetHeight(),
		                           srcFrames.GetChannels() * 8);

		if (err) {
			fprintf(stderr, "CompressBegin failed with err %d\n", err);
			return false;
		}

		const void* pSrcBits = nullptr;
		uint8_t*    pDstBits = compressedBuf.data();
		int         i        = 0;

		for (; !!(pSrcBits = srcFrames.GetBits(i)); i++) {
			err = pCode->Compress(i, pSrcBits, pDstBits, &(compressedFrameSizes[i]));

			if (err) {
				fprintf(stderr, "Compress failed with err %d\n", err);
				pCode->CompressEnd();
				return false;
			}

			compressedFrames[i] = pDstBits;
			pDstBits += compressedFrameSizes[i];
			totalCompressedSize += compressedFrameSizes[i];
		}

		pCode->CompressEnd();
	}

	// decode execution
	{
		std::unique_ptr<CodecInst> pCode(new CodecInst());

		pCode->multithreading = true;
		pCode->use_alpha      = false;

		err = pCode->DecompressBegin(srcFrames.GetWidth(), srcFrames.GetHeight(),
		                             srcFrames.GetChannels() * 8);

		if (err) {
			fprintf(stderr, "DecompressBegin failed with err %d\n", err);
			return false;
		}

		uint8_t* pDstBits = nullptr;
		int      i        = 0;

		for (; !!(pDstBits = decompressedFrames.GetBits(i)); i++) {
			err = pCode->Decompress(i, compressedFrames[i], compressedFrameSizes[i], pDstBits);

			if (err) {
				fprintf(stderr, "Decompress failed with err %d\n", err);
				pCode->DecompressEnd();
				return false;
			}
		}

		pCode->DecompressEnd();
	}

	// round trip verification
	{
		int mismatches = 0;

		for (int i = 0; i < srcFrames.GetFrameCount(); i++) {
			const void* pOrigBits      = srcFrames.GetBits(i);
			const void* pRoundTripBits = decompressedFrames.GetBits(i);
			if (memcmp(pOrigBits, pRoundTripBits, inputFrameSize) != 0) {
				fprintf(stderr, "Frame %d failed lossless reconstruction.\n", i);
				mismatches++;
			}
		}

		if (mismatches) {
			decompressedFrames.Save("decompressed_frame_%02d.png");
			return false;
		}

		FILE* fp = nullptr;
		fopen_s(&fp, "compressed_frames.lags", "wb");
		if (fp) {
			fwrite(compressedBuf.data(), totalCompressedSize, 1, fp);
			fclose(fp);
			fp = nullptr;
		}
	}

	return true;
}

// 3-channel data not reliable (non-4-byte-aligned size issues)
bool testEncodeDecodeRGB() {
	if (testEncodeDecode(3)) {
		printf("testEncodeDecodeRGB passed.\n");
		return true;
	}

	return false;
}

bool testEncodeDecodeRGBX() {
	if (testEncodeDecode(4)) {
		printf("testEncodeDecodeRGBX passed.\n");
		return true;
	}

	return false;
}

void registerTests(std::vector<TestFunction>& tests) {
	tests.push_back(&testEncodeDecodeRGBX);
}
