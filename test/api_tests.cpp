#include "test_framework.h"

#include "lagarith.h"

// only needed for handling test data - not used for actual codec.
#include "stb/stb.h"
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"

#define DUMP_INTERMED_DATA 0

// keep saved avis for now - using for other diagnostics
#define REMOVE_SAVED_AVIS_ON_SUCCESS 0

// bug in original lagarith - an odd diagonal line of bad pixels appears - due to non-multiple of 4 image width.
// does not occur in 32 bit RGB. fortunately we only need 32 bit RGB.
#define TEST_RGB24_FORMAT 1
#define TEST_RGB32_FORMAT 1

using namespace Lagarith;

static_assert(sizeof(PixelRGB) == 3, "That's not 24 bits");
static_assert(sizeof(PixelRGBX) == 4, "That's not 32 bits");

inline bool operator==(const PixelRGB& rgb, const PixelRGBX& rgbx) {
	return rgb.r == rgbx.bgrx.r && rgb.g == rgbx.bgrx.g && rgb.b == rgbx.bgrx.b;
}

inline bool operator==(const PixelRGBX& rgbx, const PixelRGB& rgb) {
	return rgb == rgbx;
}

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
class RasterSequence : public VideoSequence {
public:
	bool Load(uint32_t channels, const char* format, uint32_t first, uint32_t last,
	          CacheMode cacheMode) {
		char     imageName[128];
		uint32_t j   = first;
		uint32_t end = last + 1;

		Initialize(FrameDimensions());

		Raster tempFrame;

		for (uint32_t i = 0; (j != end); i++, j += 1) {
			sprintf_s(imageName, format, j);

			if (!tempFrame.Load(imageName, channels)) {
				fprintf(stderr, "failed loading image %s.\n", imageName);
				return false;
			}

			const FrameDimensions frameDims = {(uint16_t)tempFrame.m_width, (uint16_t)tempFrame.m_height,
			                                   (BitsPerPixel)(channels * 8)};
			if (!i) {
				Initialize(frameDims, cacheMode);
			}

			if (!AddFrame({(uint8_t*)tempFrame.m_pBits, frameDims})) {
				FrameDimensions actualDims = GetFrameDimensions();
				fprintf(stderr, "image %s (%dx%d) does not match sequence dimensions (%dx%d)\n", imageName,
				        frameDims.w, frameDims.h, actualDims.w, actualDims.h);
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
	const VideoSequence::CacheMode cacheMode = VideoSequence::CacheMode::kRaster;

	const char*    fmtName = (channelCount == 3) ? "rgb24" : "rgb32";
	RasterSequence srcFrames;

	CHECK_MSG(srcFrames.Load(channelCount, "test_data/frame_%02d.png", 0, 4, cacheMode),
	          "failed loading %s format", fmtName);

	const FrameDimensions frameDims               = srcFrames.GetFrameDimensions();
	const uint32_t        inputFrameSize          = frameDims.GetSizeBytes();
	uint32_t              compressedFrameSizes[5] = {};
	void*                 compressedFrames[5]     = {};
	uint32_t              totalCompressedSize     = 0;
	std::vector<uint8_t>  compressedBuf;
	RasterSequence        decompressedFrames;

	CHECK_MSG(decompressedFrames.Initialize(frameDims, VideoSequence::CacheMode::kRaster),
	          "failed sequence: %d x %dx%dx%d", srcFrames.GetFrameCount(), frameDims.w, frameDims.h,
	          frameDims.GetBytesPerPixel())

	decompressedFrames.AllocateRasterFrames(srcFrames.GetFrameCount());

	compressedBuf.resize(
	  (size_t)(Codec::GetMaxCompressedSize(frameDims) * srcFrames.GetFrameCount()));

	// encode execution
	{
		std::unique_ptr<Codec> pCodec(new Codec());

		uint8_t* pDstBits = compressedBuf.data();
		uint32_t i        = 0;

		for (; i < srcFrames.GetFrameCount(); i++) {
			RasterRef srcRaster = srcFrames.GetRasterFrameRef(i);

			CHECK_MSG(pCodec->Compress(srcRaster, pDstBits, &(compressedFrameSizes[i])),
			          "failed compressing frame %d/%d", i, srcFrames.GetFrameCount());

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
		Codec    codec;

		for (uint32_t i = 0; i < decompressedFrames.GetFrameCount(); i++) {
			RasterRef dstRaster = decompressedFrames.GetRasterFrameRef(i);
			CHECK_MSG(codec.Decompress(compressedFrames[i], compressedFrameSizes[i], dstRaster),
			          "failed decoding frame %d/%d", i, decompressedFrames.GetFrameCount());
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
				const uint32_t framePixCount = srcFrames.GetWidth() * srcFrames.GetHeight();
				Raster         diff;
				char           imageName[128];

				sprintf_s(imageName, "test_data/mismatched_%s_%02d.png", fmtName, i);
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

		CHECK_EQUAL(0, mismatches);
	}

	return true;
}

static bool testVideoSequenceCache(uint32_t channelCount) {
	const char*    fmtName = (channelCount == 3) ? "rgb24" : "rgb32";
	RasterSequence srcFrames;

	CHECK_MSG(srcFrames.Load(channelCount, "test_data/frame_%02d.png", 0, 4,
	                         VideoSequence::CacheMode::kRaster),
	          "failed loading %s fmt", fmtName);

	const FrameDimensions frameDims = srcFrames.GetFrameDimensions();

	Raster decoded;
	decoded.Alloc(frameDims.w, frameDims.h, (uint32_t)frameDims.bpp / 8);

	VideoSequence testEncDec(frameDims, VideoSequence::CacheMode::kCompressed);

	for (uint32_t f = 0, e = srcFrames.GetFrameCount(); f < e; f++) {
		const uint8_t* pSrc = srcFrames.GetRasterFrame(f);

		CHECK_MSG(testEncDec.AddFrame({pSrc, frameDims}), "failed adding %s fmt frame %d", fmtName, f);
		CHECK_MSG(!testEncDec.GetRasterFrame(f),
		          "compressed %s seq should not provide raster pointers.", fmtName);
		CHECK_MSG(testEncDec.DecodeFrame(f, {decoded.m_pBits, frameDims}),
		          "failed decoding %s fmt frame %d", fmtName, f);
		CHECK_MSG(!memcmp(decoded.m_pBits, pSrc, frameDims.GetSizeBytes()),
		          "round trip failure %s fmt frame %d", fmtName, f);
	}

	VideoSequence testCopyComp(frameDims, VideoSequence::CacheMode::kCompressed);

	for (uint32_t f = 0, e = testEncDec.GetFrameCount(); f < e; f++) {
		uint32_t    srcCmpSize = 0;
		uint32_t    dstCmpSize = 0;
		const void* pSrc       = nullptr;
		const void* pDst       = nullptr;

		CHECK_MSG((pSrc = testEncDec.GetCompressedFrame(f, &srcCmpSize)),
		          "compressed %s seq should provide compressed pointers.", fmtName);
		CHECK_MSG(srcCmpSize != 0, "compressed seq buffer size should not be zero.");
		CHECK_MSG(testCopyComp.AddFrame(pSrc, srcCmpSize),
		          "Failed adding pre-compressed %s fmt frame %d", fmtName, f);
		CHECK((pDst = testCopyComp.GetCompressedFrame(f, &dstCmpSize)));
		CHECK_EQUAL_MSG(srcCmpSize, dstCmpSize, "copied buffer size mismatch!");
		CHECK_MSG(!memcmp(pSrc, pDst, srcCmpSize), "copied buffer content mismatch!");
	}

	CHECK_MSG(srcFrames.DecodeFrame(0, {decoded.m_pBits, frameDims}), "decode straight copy failed!");
	CHECK_MSG(!memcmp(decoded.m_pBits, srcFrames.GetRasterFrame(0), frameDims.GetSizeBytes()),
	          "decode straight copy produced corrupted data!");

	if (channelCount == 3) {
		const PixelRGB*              pSrc = (const PixelRGB*)srcFrames.GetRasterFrame(0);
		std::unique_ptr<PixelRGBX[]> pTranscode(new PixelRGBX[frameDims.GetPixelCount()]);
		PixelRGBX*                   pTC = pTranscode.get();

		CHECK_MSG(srcFrames.DecodeFrame(0, {pTC, frameDims}), "transcoding from RGB to RGBX failed");

		for (uint32_t p = 0, pc = frameDims.GetPixelCount(); p < pc; p++) {
			CHECK_EQUAL_MSG(pSrc[p], pTC[p],
			                "mismatch transcoding from RGB to RGBX (%d, %d, %d) vs. (%d, %d, %d)",
			                pSrc[p].r, pSrc[p].g, pSrc[p].b, pTC[p].bgrx.r, pTC[p].bgrx.g, pTC[p].bgrx.b);
		}

	} else if (channelCount == 4) {
		const PixelRGBX*            pSrc = (const PixelRGBX*)srcFrames.GetRasterFrame(0);
		std::unique_ptr<PixelRGB[]> pTranscode(new PixelRGB[frameDims.GetPixelCount()]);
		PixelRGB*                   pTC = pTranscode.get();

		CHECK_MSG(srcFrames.DecodeFrame(0, {pTC, frameDims}), "transcoding from RGBX to RGB failed");

		for (uint32_t p = 0, pc = frameDims.GetPixelCount(); p < pc; p++) {
			CHECK_EQUAL_MSG(pSrc[p], pTC[p],
			                "mismatch transcoding from RGBX to RGB (%d, %d, %d) vs. (%d, %d, %d)",
			                pSrc[p].bgrx.r, pSrc[p].bgrx.g, pSrc[p].bgrx.b, pTC[p].r, pTC[p].g, pTC[p].b);
		}
	} else {
		FAIL_MSG("internal testing error - channelCount should only be 3 or 4");
	}

	{
		const uint32_t compSize      = 5937;
		uint32_t       checkCompSize = 0;
		void*          pComp         = nullptr;
		void*          pCheckComp    = nullptr;

		CHECK((pComp = testCopyComp.AllocateCompressedFrame(compSize)));
		CHECK((pCheckComp =
		         testCopyComp.GetCompressedFrame(testCopyComp.GetFrameCount() - 1, &checkCompSize)));
		CHECK_EQUAL(pComp, pCheckComp);
		CHECK_EQUAL(compSize, checkCompSize);
	}

	return true;
}

static bool testVideoSequenceAvi(uint32_t channelCount, bool compressed) {
	const char*                    fmtName  = (channelCount == 3) ? "rgb24" : "rgb32";
	const char*                    modeName = compressed ? "cmpr" : "rstr";
	const VideoSequence::CacheMode cacheMode =
	  compressed ? VideoSequence::CacheMode::kCompressed : VideoSequence::CacheMode::kRaster;
	RasterSequence seq0;

	char lagsPath[128] = {};
	sprintf_s(lagsPath, "test_data/%s_%s_test.avi", modeName, fmtName);

	CHECK_MSG(seq0.Load(channelCount, "test_data/frame_%02d.png", 0, 4, cacheMode),
	          "failed loading %d channels in %s mode", channelCount, modeName);

	// verify that the specified cache mode was actually used
	{
		uint32_t sz = 0;

		CHECK_EQUAL(cacheMode, seq0.GetCacheMode());

		if (compressed) {
			CHECK(!seq0.GetRasterFrame(0));
			CHECK(seq0.GetCompressedFrame(0, &sz) && sz);
		} else {
			CHECK(seq0.GetRasterFrame(0));
			CHECK(!seq0.GetCompressedFrame(0, &sz));
		}
	}

	const FrameDimensions dims0 = seq0.GetFrameDimensions();

	CHECK_MSG(seq0.SaveLagsFile(lagsPath), "failed saving %s", lagsPath);

	RasterSequence seq1;

	CHECK_MSG(seq1.LoadLagsFile(lagsPath, (BitsPerPixel)(channelCount * 8), cacheMode),
	          "failed loading %s", lagsPath);

	const FrameDimensions dims1 = seq1.GetFrameDimensions();

	CHECK(dims0.IsRectEqual(dims1));
	CHECK_EQUAL(seq0.GetFrameCount(), seq1.GetFrameCount());

	const uint32_t frameSizeBytes = dims0.GetSizeBytes();

	if (compressed) {
		for (uint32_t f = 0, e = seq0.GetFrameCount(); f < e; f++) {
			uint32_t    sz0 = 0, sz1 = 0;
			const void* pR0 = seq0.GetCompressedFrame(f, &sz0);
			const void* pR1 = seq1.GetCompressedFrame(f, &sz1);
			CHECK_EQUAL_MSG(sz0, sz1, "compressed buf size mismatch  %d vs %d at frame %d", sz0, sz1, f);
			CHECK_MSG(!memcmp(pR0, pR1, sz0), "loaded/saved buffers mismatch at frame %d\n", f);
		}
	} else {
		for (uint32_t f = 0, e = seq0.GetFrameCount(); f < e; f++) {
			const void* pR0 = seq0.GetRasterFrame(f);
			const void* pR1 = seq1.GetRasterFrame(f);
			CHECK_MSG(!memcmp(pR0, pR1, frameSizeBytes), "loaded/saved rasters mismatch at frame %d\n",
			          f);
		}
	}

#if REMOVE_SAVED_AVIS_ON_SUCCESS
	remove(lagsPath);
#endif

	return true;
}

#if TEST_RGB24_FORMAT
DECLARE_TEST(testEncodeDecodeRGB) {
	return testEncodeDecode(3);
}
#endif // TEST_RGB24_FORMAT

#if TEST_RGB32_FORMAT
DECLARE_TEST(testEncodeDecodeRGBX) {
	return testEncodeDecode(4);
}
#endif // TEST_RGB32_FORMAT

#if TEST_RGB24_FORMAT
DECLARE_TEST(testVideoSequenceCacheRGB) {
	return testVideoSequenceCache(3);
}
#endif // TEST_RGB24_FORMAT

#if TEST_RGB32_FORMAT
DECLARE_TEST(testVideoSequenceCacheRGBX) {
	return testVideoSequenceCache(4);
}
#endif // TEST_RGB32_FORMAT

#if TEST_RGB24_FORMAT
DECLARE_TEST(testRasterVideoSequenceAviRGB) {
	return testVideoSequenceAvi(3, false);
}
#endif // TEST_RGB24_FORMAT

#if TEST_RGB32_FORMAT
DECLARE_TEST(testRasterVideoSequenceAviRGBX) {
	return testVideoSequenceAvi(4, false);
}
#endif // TEST_RGB32_FORMAT

#if TEST_RGB24_FORMAT
DECLARE_TEST(testCompressedVideoSequenceAviRGB) {
	return testVideoSequenceAvi(3, true);
}
#endif // TEST_RGB24_FORMAT

#if TEST_RGB32_FORMAT
DECLARE_TEST(testCompressedVideoSequenceAviRGBX) {
	return testVideoSequenceAvi(4, true);
}
#endif // TEST_RGB32_FORMAT
