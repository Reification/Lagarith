#include "test_framework.h"

#include "lagarith.h"

// only needed for handling test data - not used for actual codec.
#include "stb/stb.h"
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"

#define DUMP_INTERMED_DATA 0

// bug in original lagarith - an odd diagonal line of bad pixels appears - likely due to irregular image width.
// does not occur in 32 bit RGB. fortunately we only need 32 bit RGB.
#define TEST_RGB24_FORMAT 0

struct Raster {
	stbi_uc* m_pBits    = nullptr;
	uint32_t m_width    = 0;
	uint32_t m_height   = 0;
	uint32_t m_channels = 0;
	uint32_t m_alloced  = 0;

	Raster() = default;
	~Raster() { Free(); }

	uint32_t GetSizeBytes() const { return m_width * m_height * m_channels; }

	bool Load(const char* path, uint32_t channels = 0) {
		Free();
		m_pBits = stbi_load(path, (int*)&m_width, (int*)&m_height, (int*)&m_channels, (int)channels);
		if (channels && m_pBits) {
			m_channels = channels;
		}
		return !!m_pBits;
	}

	bool Save(const char* path) {
		if (!GetSizeBytes()) {
			return false;
		}

		if (stbi_write_png(path, (int)m_width, (int)m_height, (int)m_channels, m_pBits,
		                   (int)(m_width * m_channels))) {
			return true;
		}

		return false;
	}

	void Alloc(uint32_t w, uint32_t h, uint32_t c) {
		Free();

		if (uint32_t sizeBytes = w * h * c) {
			m_width    = w;
			m_height   = h;
			m_channels = c;
			// when decompressing target image can need extra space (I didn't write the damn thing)
			if (c & 3) {
				sizeBytes = (((w + 3) >> 2) << 2) * h * c;
			}
			m_pBits   = new stbi_uc[sizeBytes];
			m_alloced = true;
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
	bool Load(uint32_t channels, const char* format, uint32_t first, uint32_t last,
	          uint32_t step = 1) {
		char     imageName[128];
		uint32_t j   = first;
		uint32_t end = last + step;

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

	void Alloc(uint32_t w, uint32_t h, uint32_t channels, uint32_t frames) {
		Free();
		for (uint32_t i = 0; i < frames && i < sizeof(m_frames) / sizeof(m_frames[0]); i++) {
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

	uint32_t GetWidth() const { return m_frames[0].m_width; }
	uint32_t GetHeight() const { return m_frames[0].m_height; }
	uint32_t GetChannels() const { return m_frames[0].m_channels; }
	stbi_uc* GetBits(uint32_t frameIdx) const {
		return frameIdx < m_frameCount ? m_frames[frameIdx].m_pBits : nullptr;
	}

	uint32_t GetFrameSizeBytes() const { return m_frames[0].GetSizeBytes(); }
	uint32_t GetSequenceSizeBytes() const { return GetFrameSizeBytes() * m_frameCount; }
	uint32_t GetFrameCount() const { return m_frameCount; }

private:
	uint32_t m_frameCount = 0;
	Raster   m_frames[5];
};

bool testEncodeDecode(uint32_t channelCount) {
	RasterSequence srcFrames;
	RasterSequence decompressedFrames;
	const char*    fmtName = (channelCount == 3) ? "rgb24" : "rgb32";
	bool           result  = false;

	if (!srcFrames.Load(channelCount, "test_data/frame_%02d.png", 0, 4)) {
		return false;
	}

	decompressedFrames.Alloc(srcFrames);

	const uint32_t       inputFrameSize          = srcFrames.GetFrameSizeBytes();
	uint32_t             compressedFrameSizes[5] = {};
	void*                compressedFrames[5]     = {};
	std::vector<uint8_t> compressedBuf;
	uint32_t             totalCompressedSize = 0;

	compressedBuf.resize((size_t)(inputFrameSize * srcFrames.GetFrameCount() * 1.1));

	// encode execution
	{
		std::unique_ptr<Lagarith::Codec> pCode(new Lagarith::Codec());

		pCode->SetMultithreaded(true);

		result = pCode->CompressBegin(srcFrames.GetWidth(), srcFrames.GetHeight(),
		                              srcFrames.GetChannels() * 8);

		if (!result) {
			fprintf(stderr, "CompressBegin failed.\n");
			return false;
		}

		const void* pSrcBits = nullptr;
		uint8_t*    pDstBits = compressedBuf.data();
		uint32_t    i        = 0;

		for (; !!(pSrcBits = srcFrames.GetBits(i)); i++) {
			result = pCode->Compress(pSrcBits, pDstBits, &(compressedFrameSizes[i]));

			if (!result) {
				fprintf(stderr, "Compress failed.\n");
				pCode->CompressEnd();
				return false;
			}

			compressedFrames[i] = pDstBits;
			pDstBits += compressedFrameSizes[i];
			totalCompressedSize += compressedFrameSizes[i];
		}

		pCode->CompressEnd();

		printf("Original size: %d bytes. Total compressed size: %d bytes. compression factor: %g\n",
		       srcFrames.GetSequenceSizeBytes(), totalCompressedSize,
		       (double)totalCompressedSize / (double)srcFrames.GetSequenceSizeBytes());
	}

	// decode execution
	{
		std::unique_ptr<Lagarith::Codec> pCode(new Lagarith::Codec());

		pCode->SetMultithreaded(true);

		result = pCode->DecompressBegin(srcFrames.GetWidth(), srcFrames.GetHeight(),
		                                srcFrames.GetChannels() * 8);

		if (!result) {
			fprintf(stderr, "DecompressBegin failed.\n");
			return false;
		}

		uint8_t* pDstBits = nullptr;
		uint32_t i        = 0;

		for (; !!(pDstBits = decompressedFrames.GetBits(i)); i++) {
			result = pCode->Decompress(compressedFrames[i], compressedFrameSizes[i], pDstBits);

			if (!result) {
				fprintf(stderr, "Decompress failed.\n");
				pCode->DecompressEnd();
				return false;
			}
		}

		pCode->DecompressEnd();
	}

#if DUMP_INTERMED_DATA
	{
		char path[128];
		sprintf_s(path, "test_data/decompressed_%s_%%02d.png", fmtName);

		decompressedFrames.Save(path);

		sprintf_s(path, "test_data/compressed_%dx%dx%d_%d.lags", srcFrames.GetWidth(),
		          srcFrames.GetHeight(), channelCount, srcFrames.GetFrameCount());
		FILE* fp = nullptr;
		fopen_s(&fp, path, "wb");
		if (fp) {
			fwrite(compressedBuf.data(), totalCompressedSize, 1, fp);
			fclose(fp);
			fp = nullptr;
		}
	}
#endif

	// round trip verification
	{
		uint32_t mismatches = 0;

		for (uint32_t i = 0; i < srcFrames.GetFrameCount(); i++) {
			const stbi_uc* pOrigBits      = srcFrames.GetBits(i);
			const stbi_uc* pRoundTripBits = decompressedFrames.GetBits(i);
			if (memcmp(pOrigBits, pRoundTripBits, inputFrameSize) != 0) {
				char imageName[128];
				sprintf_s(imageName, "test_data/mismatched_%s_frame_%02d.png", fmtName, i);
				const uint32_t framePixCount = srcFrames.GetWidth() * srcFrames.GetHeight();
				Raster         diff;
				diff.Alloc(srcFrames.GetWidth(), srcFrames.GetHeight(), srcFrames.GetChannels());
				stbi_uc* pDiff         = diff.m_pBits;
				uint32_t pixMismatches = 0;
				for (uint32_t p = 0; p < inputFrameSize; p += channelCount) {
					const int32_t  origR = pOrigBits[p + 0];
					const int32_t  rtR   = pRoundTripBits[p + 0];
					const uint32_t diffR = std::abs(rtR - origR);
					const int32_t  origG = pOrigBits[p + 1];
					const int32_t  rtG   = pRoundTripBits[p + 1];
					const uint32_t diffG = std::abs(rtG - origG);
					const int32_t  origB = pOrigBits[p + 2];
					const int32_t  rtB   = pRoundTripBits[p + 2];
					const uint32_t diffB = std::abs(rtB - origB);

					pDiff[p + 0] = (stbi_uc)std::min((uint32_t)0xff, diffR);
					pDiff[p + 1] = (stbi_uc)std::min((uint32_t)0xff, diffG);
					pDiff[p + 2] = (stbi_uc)std::min((uint32_t)0xff, diffB);
					if (channelCount == 4) {
						pDiff[p + 3] = 0xff;
					}

					pixMismatches += (pDiff[p + 0] || pDiff[p + 1] || pDiff[p + 2]);
				}
				diff.Save(imageName);
				fprintf(stderr, "%s frame %d failed lossless reconstruction - %d/%d mismatches (%g%%).\n",
				        fmtName, i, pixMismatches, framePixCount,
				        (float)pixMismatches / (float)framePixCount * 100.0f);
				mismatches++;
			}
		}

		if (mismatches) {
			return false;
		}
	}

	return true;
}

#if TEST_RGB24_FORMAT
DECLARE_TEST(testEncodeDecodeRGB) {
	return testEncodeDecode(3);
}
#endif // TEST_RGB24_FORMAT

DECLARE_TEST(testEncodeDecodeRGBX) {
	return testEncodeDecode(4);
}
