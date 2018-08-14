#include "test_framework.h"

#include "lagarith.h"

// only needed for handling test data - not used for actual codec.
#include "stb/stb.h"
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"

#define DUMP_INTERMED_DATA 0

// bug in original lagarith - an odd diagonal line of bad pixels appears - due to non-multiple of 4 image width.
// does not occur in 32 bit RGB. fortunately we only need 32 bit RGB.
#define TEST_RGB24_FORMAT 1
#define TEST_RGB32_FORMAT 1

//
// simple wrapper on stb image load and save.
//
struct Raster {
	stbi_uc* m_pBits    = nullptr;
	uint32_t m_width    = 0;
	uint32_t m_height   = 0;
	uint32_t m_channels = 0;
	uint16_t m_bAlloced = false;
	uint16_t m_bOwned   = false;

	Raster() = default;
	~Raster() { Free(); }

	uint32_t GetSizeBytes() const { return m_width * m_height * m_channels; }

	bool Load(const char* path, uint32_t channels) {
		Free();

		//stbi_set_flip_vertically_on_load(true);
		m_pBits = stbi_load(path, (int*)&m_width, (int*)&m_height, (int*)&m_channels, (int)channels);
		//stbi_set_flip_vertically_on_load(false);

		if (m_pBits) {
			m_channels = channels;
			m_bOwned   = true;

			swizzleFlip();
		}

		if (m_pBits) {
			return true;
		}

		return false;
	}

	bool Save(const char* path) {
		if (!GetSizeBytes()) {
			return false;
		}

		//stbi_flip_vertically_on_write(true);
		swizzleFlip();
		const bool result = stbi_write_png(path, (int)m_width, (int)m_height, (int)m_channels, m_pBits,
		                                   (int)(m_width * m_channels));
		swizzleFlip();
		//stbi_flip_vertically_on_write(false);

		if (result) {
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
			m_pBits    = new stbi_uc[sizeBytes];
			m_bAlloced = true;
			m_bOwned   = true;
		}
	}

	void Free() {
		if (m_pBits) {
			if (m_bOwned) {
				if (m_bAlloced) {
					delete[] m_pBits;
				} else {
					stbi_image_free(m_pBits);
				}
			}
			m_pBits = nullptr;
		}
		m_bAlloced = m_bOwned = false;
		m_width = m_height = m_channels = 0;
	}

private:
	void swizzleFlip() {
		switch (m_channels) {
		case 3: swizzleFlip3(); break;
		case 4: swizzleFlip4(); break;
		default: assert(false && "Not Implemented!"); break;
		}
	}


	static inline uint32_t swizzle4(uint32_t p) {
		//0xAABBGGRR <-> 0xAARRGGBB
		return (p & 0xff00ff00) | ((p & 0x00ff0000) >> 16) | ((p & 0x000000ff) << 16);
	}

	static inline uint32_t load_u32_u8x3(const uint8_t* p) {
		return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
	}

	static inline void store_u32_u8x3(uint8_t* pd, uint32_t ps) {
		pd[0] = (uint8_t)ps;
		pd[1] = (uint8_t)(ps >> 8);
		pd[2] = (uint8_t)(ps >> 16);
	}

	void swizzleFlip3() {
		const uint32_t pitch  = m_width * 3;
		uint8_t*       pLoRow = (uint8_t*)m_pBits;
		uint8_t*       pHiRow = pLoRow + (pitch * (m_height - 1));
		uint32_t       tpix0 = 0, tpix1 = 0;

		for (; pLoRow < pHiRow; pLoRow += pitch, pHiRow -= pitch) {
			for (uint32_t x = 0; x < pitch; x += 3) {
				tpix0 = swizzle4(load_u32_u8x3(pLoRow + x));
				tpix1 = swizzle4(load_u32_u8x3(pHiRow + x));

				store_u32_u8x3(pLoRow + x, tpix1);
				store_u32_u8x3(pHiRow + x, tpix0);
			}
		}
	}

	void swizzleFlip4() {
		uint32_t* pLoRow = (uint32_t*)m_pBits;
		uint32_t* pHiRow = pLoRow + (m_width * (m_height - 1));
		uint32_t  tpix0 = 0, tpix1 = 0;

		for (; pLoRow < pHiRow; pLoRow += m_width, pHiRow -= m_width) {
			for (uint32_t x = 0, w = m_width; x < w; ++x) {
				tpix0 = swizzle4(pLoRow[x]);
				tpix1 = swizzle4(pHiRow[x]);

				pLoRow[x] = tpix1;
				pHiRow[x] = tpix0;
			}
		}
	}
};

// used to test VideoSequence AVI i/o and as utility in EncodeDecode test
class RasterSequence : public Lagarith::VideoSequence {
public:
	bool Load(uint32_t channels, const char* format, uint32_t first, uint32_t last,
	          uint32_t step = 1) {
		char     imageName[128];
		uint32_t j   = first;
		uint32_t end = last + step;

		Initialize(Lagarith::FrameDimensions());

		Raster tempFrame;

		for (uint32_t i = 0; (j != end); i++, j += step) {
			sprintf_s(imageName, format, j);

			if (!tempFrame.Load(imageName, channels)) {
				fprintf(stderr, "failed loading image %s.\n", imageName);
				return false;
			}

			const Lagarith::FrameDimensions frameDims = {(uint16_t)tempFrame.m_width,
			                                             (uint16_t)tempFrame.m_height,
			                                             (Lagarith::BitsPerPixel)(channels * 8)};
			if (!i) {
				Initialize(frameDims);
			}

			if (!AddFrame({ (uint8_t*)tempFrame.m_pBits, frameDims})) {
				Lagarith::FrameDimensions actualDims = GetFrameDimensions();
				fprintf(stderr, "image %s (%dx%d) does not match sequence dimensions (%dx%d)\n", imageName,
				        frameDims.w, frameDims.h, actualDims.w, actualDims.h);
				return false;
			}
		}

		return true;
	}

	bool Save(const char* format) {
		char imageName[128];

		const Lagarith::FrameDimensions frameDims = GetFrameDimensions();
		Raster                          tempFrame;

		tempFrame.m_bOwned   = false;
		tempFrame.m_width    = frameDims.w;
		tempFrame.m_height   = frameDims.h;
		tempFrame.m_channels = frameDims.GetBytesPerPixel();

		for (uint32_t i = 0, e = GetFrameCount(); i < e; i++) {
			sprintf_s(imageName, format, i);
			tempFrame.m_pBits = (stbi_uc*)GetRasterFrame(i);
			if (!tempFrame.Save(imageName)) {
				fprintf(stderr, "failed to save frame %s\n", imageName);
				return false;
			}
		}

		return true;
	}

	uint32_t GetWidth() const { return GetFrameDimensions().w; }
	uint32_t GetHeight() const { return GetFrameDimensions().h; }
	uint32_t GetChannels() const { return GetFrameDimensions().GetBytesPerPixel(); }
	uint32_t GetFrameSizeBytes() const { return GetFrameDimensions().GetSizeBytes(); }
};

static bool testEncodeDecode(uint32_t channelCount) {
	const char*    fmtName = (channelCount == 3) ? "rgb24" : "rgb32";
	bool           result  = false;
	RasterSequence srcFrames;

	if (!srcFrames.Load(channelCount, "test_data/frame_%02d.png", 0, 4)) {
		return false;
	}

	const Lagarith::FrameDimensions frameDims               = srcFrames.GetFrameDimensions();
	const uint32_t                  inputFrameSize          = frameDims.GetSizeBytes();
	uint32_t                        compressedFrameSizes[5] = {};
	void*                           compressedFrames[5]     = {};
	uint32_t                        totalCompressedSize     = 0;
	std::vector<uint8_t>            compressedBuf;
	RasterSequence                  decompressedFrames;

	if (!decompressedFrames.Initialize(frameDims, Lagarith::VideoSequence::CacheMode::kRaster)) {
		fprintf(stderr, "Failed initializing raster sequence: %d frames of %dx%dx%d rasters.\n",
		        srcFrames.GetFrameCount(), frameDims.w, frameDims.h, frameDims.GetBytesPerPixel());
		return false;
	}

	decompressedFrames.AddEmptyFrames(srcFrames.GetFrameCount());

	compressedBuf.resize((size_t)(inputFrameSize * srcFrames.GetFrameCount() * 1.1));

	// encode execution
	{
		std::unique_ptr<Lagarith::Codec> pCodec(new Lagarith::Codec());

		const uint8_t* pSrcBits = nullptr;
		uint8_t*    pDstBits = compressedBuf.data();
		uint32_t    i        = 0;

		for (; !!(pSrcBits = srcFrames.GetRasterFrame(i)); i++) {
			result = pCodec->Compress({pSrcBits, frameDims}, pDstBits, &(compressedFrameSizes[i]));

			if (!result) {
				fprintf(stderr, "Compress failed.\n");
				return false;
			}

			compressedFrames[i] = pDstBits;
			pDstBits += compressedFrameSizes[i];
			totalCompressedSize += compressedFrameSizes[i];
		}

		printf("Original size: %d bytes. Total compressed size: %d bytes. compression factor: %g\n",
		       srcFrames.GetFrameSizeBytes() * srcFrames.GetFrameCount(), totalCompressedSize,
		       (double)totalCompressedSize /
		         (double)(srcFrames.GetFrameSizeBytes() * srcFrames.GetFrameCount()));
	}

	// decode execution
	{
		std::unique_ptr<Lagarith::Codec> pCode(new Lagarith::Codec());

		uint8_t* pDstBits = nullptr;
		uint32_t i        = 0;

		for (; !!(pDstBits = decompressedFrames.GetRasterFrame(i)); i++) {
			result = pCode->Decompress(compressedFrames[i], compressedFrameSizes[i], { pDstBits, frameDims });

			if (!result) {
				fprintf(stderr, "Decompress failed.\n");
				return false;
			}
		}
	}

#if DUMP_INTERMED_DATA
	{
		char path[128];
		sprintf_s(path, "test_data/decompressed_%s_%%02d.png", fmtName);

		decompressedFrames.Save(path);
	}
#endif

	// round trip verification
	{
		uint32_t mismatches = 0;

		for (uint32_t i = 0; i < srcFrames.GetFrameCount(); i++) {
			const stbi_uc* pOrigBits      = (stbi_uc*)srcFrames.GetRasterFrame(i);
			const stbi_uc* pRoundTripBits = (stbi_uc*)decompressedFrames.GetRasterFrame(i);
			if (memcmp(pOrigBits, pRoundTripBits, inputFrameSize) != 0) {
				char imageName[128];
				sprintf_s(imageName, "test_data/mismatched_%s_%02d.png", fmtName, i);
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

static bool testVideoSequenceAvi(uint32_t channelCount) {
	const char* pLagsPath = (channelCount == 3) ? "test_data/lags_save_rgb24_test.avi"
	                                            : "test_data/lags_save_rgb32_test.avi";

	RasterSequence seq0;

	if (!seq0.Load(channelCount, "test_data/frame_%02d.png", 0, 4)) {
		return false;
	}

	const Lagarith::FrameDimensions d0 = seq0.GetFrameDimensions();

	if (!seq0.SaveLagsFile(pLagsPath)) {
		fprintf(stderr, "failed saving lags avi file.\n");
		return false;
	}

	RasterSequence seq1;

	if (!seq1.LoadLagsFile(pLagsPath)) {
		fprintf(stderr, "failed loading lags avi file.\n");
		return false;
	}

	const Lagarith::FrameDimensions d1 = seq1.GetFrameDimensions();

	if (d0 != d1 || seq0.GetFrameCount() != seq1.GetFrameCount()) {
		fprintf(
		  stderr,
		  "loaded lags video sequence (%dx%dx%dx%d) does not match saved sequence (%dx%dx%dx%d).\n",
		  d0.w, d0.h, d0.GetBytesPerPixel(), seq0.GetFrameCount(), d1.w, d1.h, d1.GetBytesPerPixel(),
		  seq1.GetFrameCount());
		return false;
	}

	const uint32_t frameSizeBytes = d0.GetSizeBytes();

	for (uint32_t f = 0, e = seq0.GetFrameCount(); f < e; f++) {
		const void* pR0 = seq0.GetRasterFrame(f);
		const void* pR1 = seq1.GetRasterFrame(f);
		if (memcmp(pR0, pR1, frameSizeBytes)) {
			fprintf(stderr, "loaded/saved rasters don't match at frame %d\n", f);
			return false;
		}
	}

	return true;
}

#if TEST_RGB24_FORMAT
DECLARE_TEST(testVideoSequenceAviRGB) {
	return testVideoSequenceAvi(3);
}

DECLARE_TEST(testEncodeDecodeRGB) {
	return testEncodeDecode(3);
}
#endif // TEST_RGB24_FORMAT

#if TEST_RGB32_FORMAT
DECLARE_TEST(testVideoSequenceAviRGBX) {
	return testVideoSequenceAvi(4);
}

DECLARE_TEST(testEncodeDecodeRGBX) {
	return testEncodeDecode(4);
}
#endif // TEST_RGB32_FORMAT
