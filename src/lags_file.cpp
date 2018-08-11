#include "lagarith_internal.h"
#include "lagarith.h"

namespace Lagarith {

#define USE_TEMP_IMPL_FORMAT 0

namespace Impl {
	inline static constexpr uint32_t genFourCC(const char* const code) noexcept {
		return static_cast<uint32_t>(code[0]) | (static_cast<uint32_t>(code[0] ? code[1] : 0) << 8) |
		       (static_cast<uint32_t>(code[0] && code[1] ? code[2] : 0) << 16) |
		       (static_cast<uint32_t>(code[0] && code[1] && code[2] ? code[3] : 0) << 24);
	}

	enum : uint32_t {
		k4CC_RIFF = genFourCC("RIFF"),
		k4CC_AVI  = genFourCC("AVI "),
		k4CC_AVIX = genFourCC("AVIX"),
		k4CC_LIST = genFourCC("LIST"),
		k4CC_JUNK = genFourCC("JUNK"),

		k4CC_hdrl = genFourCC("hdrl"),
		k4CC_avih = genFourCC("avih"),
		k4CC_idx1 = genFourCC("idx1"),
		k4CC_strl = genFourCC("strl"),
		k4CC_strh = genFourCC("strh"),
		k4CC_strf = genFourCC("strf"),
		k4CC_movi = genFourCC("movi"),
		k4CC_vids = genFourCC("vids"),

		k4CC_vframe = genFourCC("00dc"),

		k4CC_LAGS = genFourCC("LAGS")
	};

	enum : uint32_t { kDataSizePaddingGranularity = 2, kDefaultFPS = 60 };

	inline uint32_t calcPaddedDataSize(uint32_t size) {
		if (kDataSizePaddingGranularity <= 1) {
			return size;
		}

		return ((size / kDataSizePaddingGranularity) + !!(size % kDataSizePaddingGranularity)) *
		       kDataSizePaddingGranularity;
	}

	struct ChunkHeader {
		uint32_t m_fourCC;
		uint32_t m_sizeBytes;

		ChunkHeader() : m_fourCC(0), m_sizeBytes(0) {}

		explicit ChunkHeader(uint32_t fourCC);

		uint32_t getPaddedDataSize() const { return calcPaddedDataSize(m_sizeBytes); }
	};

	struct ListHeader {
		uint32_t m_listCC;    // 'RIFF' or 'LIST'
		uint32_t m_sizeBytes; // size covers m_fourCC (4) plus sizeof data.
		uint32_t m_fourCC;    // 'AVI' or 'AVIX'

		ListHeader() : m_listCC(0), m_sizeBytes(0), m_fourCC(0) {}

		explicit ListHeader(uint32_t fourCC);
	};

	// 4cc size is included in m_sizeBytes
	enum : uint32_t { kListHeaderSize = sizeof(ListHeader) - sizeof(uint32_t) };

	struct Chunk : ChunkHeader {
		uint8_t m_data[1]; //[m_sizeBytes] // contains headers or video/audio data
	};

	struct List : ListHeader {
		uint8_t m_data[1]; //[m_sizeBytes - 4] // contains Lists and Chunks
	};

	enum : uint32_t {
		AVIF_HASINDEX       = 0x00000010,
		AVIF_MUSTUSEINDEX   = 0x00000020,
		AVIF_ISINTERLEAVED  = 0x00000100,
		AVIF_TRUSTCKTYPE    = 0x00000800,
		AVIF_WASCAPTUREFILE = 0x00010000,
		AVIF_COPYRIGHTED    = 0x00020000
	};

	// The 'avih' chunk (first chunk in file under AVI list) will have this data
	struct AVIHChunkData {
		// (1000000/FPS or 0)
		uint32_t m_microSecPerFrame = 1000000 / kDefaultFPS;

		// set to largest compressed frame size times frame rate (is this reasonable?)
		uint32_t m_maxBytesPerSec = 0;

		// appears to be ignored.
		uint32_t m_paddingGranularity = 0;

		// the ever-present flags - combination of AVIF_ values above - just set AVIF_HASINDEX
		uint32_t m_flags = AVIF_HASINDEX;

		// # frames in RIFF-AVI section (does not count additional RIFF-AVIX sections)
		uint32_t m_totalFrames = 0;

		// set to 0
		uint32_t m_initialFrames = 0;

		// set to 1 (this implementation is video only)
		uint32_t m_streams = 1;

		// suggested i/o buffer size for entire avi file, not specific to video stream. probably ignored.
		uint32_t m_suggestedBufferSize = 0x10000;

		uint32_t m_width       = 0; // frame width
		uint32_t m_height      = 0; // frame height
		uint32_t m_reserved[4] = {};
	};

	struct Rect {
		int32_t left;
		int32_t top;
		int32_t right;
		int32_t bottom;
	};

	struct STRHChunkData {
		uint32_t m_fccType       = k4CC_vids;
		uint32_t m_fccHandler    = k4CC_LAGS;
		uint32_t m_flags         = 0;           // leave as 0
		uint16_t m_priority      = 0;           // leave as 0
		uint16_t m_language      = 0;           // leave as 0
		uint32_t m_initialFrames = 0;           // leave as 0
		uint32_t m_scale         = 1;           // leave as 1 - denominator for rate
		uint32_t m_rate          = kDefaultFPS; // numerator - m_rate / m_scale == samples/second
		uint32_t m_start         = 0;           // leave as 0
		uint32_t m_length        = 0;           // set to frame count
		uint32_t m_suggestedBufferSize =
		  0; // set to largest single compressed frame size + chunk header overhead
		uint32_t m_quality =
		  0; // internally meaningful to specific codec. probably ignored by LAGS codec.
		uint32_t m_sampleSize = 0; // can be left at zero - bpp comes from strf chunk
		Rect     m_frameRect  = {};
	};

	// aka bitmapinfoheader
	struct STRFChunkData {
		uint32_t m_size          = (uint32_t)sizeof(STRFChunkData);
		int32_t  m_width         = 0; // frame width
		int32_t  m_height        = 0; // frame height
		int16_t  m_planes        = 1; // must be 1
		int16_t  m_bitCount      = 0; // bits per pixel from FrameDimension
		uint32_t m_compression   = 0; // used internally by LAGS codec - means RGB(X) encoding (not YUV)
		uint32_t m_sizeImage     = 0; // image size in bytes
		int32_t  m_xPelsPerMeter = 0; // leave as 0
		int32_t  m_yPelsPerMeter = 0; // leave as 0
		uint32_t m_clrUsed       = 0; // leave as 0
		uint32_t m_clrImportant  = 0; // leave as 0
	};

	enum : uint32_t { AVIIF_KEYFRAME = 0x00000010 };

	struct AVIIndexEntry {
		uint32_t m_ckid        = k4CC_vframe;    // don't change - only video frames present in file
		uint32_t m_flags       = AVIIF_KEYFRAME; // don't change - all lags frames are keyframes
		uint32_t m_chunkOffset = 0; // offset to start of chunk header (not start of chunk data)
		uint32_t m_chunkLength = 0; // same value as m_sizeBytes member of indexed ChunkData
	};

	enum : uint64_t {
		kChunkSize_avih = sizeof(ChunkHeader) + sizeof(AVIHChunkData),
		kChunkSize_strh = sizeof(ChunkHeader) + sizeof(STRHChunkData),
		kChunkSize_strf = sizeof(ChunkHeader) + sizeof(STRFChunkData),

		kListDataSize_strl = kChunkSize_strh + kChunkSize_strf,
		kListDataSize_hdrl = sizeof(kChunkSize_avih) + sizeof(ListHeader) + kListDataSize_strl
	};

	ChunkHeader::ChunkHeader(uint32_t fourCC) : m_fourCC(fourCC), m_sizeBytes(0) {
		switch (fourCC) {
		case k4CC_avih: m_sizeBytes = sizeof(AVIHChunkData); break;
		case k4CC_strh: m_sizeBytes = sizeof(STRHChunkData); break;
		// for some reason actual files are mis-reporting this size!
		//case k4CC_strh: m_sizeBytes = sizeof(STRHChunkData) - 8; break;
		case k4CC_strf: m_sizeBytes = sizeof(STRFChunkData); break;
		}
	}

	ListHeader::ListHeader(uint32_t fourCC) : m_listCC(k4CC_LIST), m_sizeBytes(0), m_fourCC(fourCC) {
		switch (fourCC) {
		case k4CC_AVI:
		case k4CC_AVIX: m_listCC = k4CC_RIFF; break;
		case k4CC_hdrl: m_sizeBytes = (uint32_t)(sizeof(uint32_t) + kListDataSize_hdrl); break;
		case k4CC_strl: m_sizeBytes = (uint32_t)(sizeof(uint32_t) + kListDataSize_strl); break;
		}
	}

	struct FrameLocation {
		uint64_t m_frameOffset;
		uint64_t m_frameSize;
	};

	struct LagsFileState {
		~LagsFileState() {
			if (m_fp) {
				fclose(m_fp);
			}

			if (m_bEncoderInitialized) {
				m_encoder.CompressEnd();
			}

			if (m_bDecoderInitialized) {
				m_decoder.DecompressEnd();
			}
		}

		bool openRead(const char* path) {
			assert(!m_fp && !m_bCanRead && !m_bCanWrite && !m_bOpenedWrite);

			fopen_s(&m_fp, path, "rb");

			if (!m_fp) {
				return false;
			}

			m_bOpenedWrite = false;
			m_bCanRead     = true;
			return true;
		}

		bool openWrite(const char* path) {
			assert(!m_fp && !m_bCanRead && !m_bCanWrite && !m_bOpenedWrite);

			fopen_s(&m_fp, path, "w+b");

			if (!m_fp) {
				return false;
			}

			m_bOpenedWrite = true;
			m_bCanWrite    = true;
			return true;
		}

		bool seek_cur(uint64_t offset = 0) {
			if (m_fp && (_fseeki64(m_fp, offset, SEEK_CUR) == 0)) {
				if (m_bOpenedWrite) {
					m_bCanRead = m_bCanWrite = true;
				}
				return true;
			}
			return false;
		}

		bool seek_set(uint64_t offset) {
			if (m_fp && (_fseeki64(m_fp, offset, SEEK_SET) == 0)) {
				if (m_bOpenedWrite) {
					m_bCanRead = m_bCanWrite = true;
				}
				return true;
			}
			return false;
		}

		bool seek_end(uint64_t offset = 0) {
			if (m_fp && (_fseeki64(m_fp, offset, SEEK_END) == 0)) {
				if (m_bOpenedWrite) {
					m_bCanRead = m_bCanWrite = true;
				}
				return true;
			}

			return false;
		}

		bool seek_frame(uint32_t frameIdx, uint64_t* pFrameSizeOut) {
			if (frameIdx >= m_frameLocations.size()) {
				return false;
			}

			const FrameLocation loc = m_frameLocations[frameIdx];

			if (pFrameSizeOut) {
				*pFrameSizeOut = loc.m_frameSize;
			}

			return seek_set(loc.m_frameOffset);
		}

		template <typename T> bool read(T* item, uint32_t count = 1) {
			if (!m_fp) {
				return false;
			}

			if (!m_bCanRead) {
				seek_cur();
			}

			m_bCanWrite = false;

			if (fread(item, sizeof(*item), count, m_fp) == count) {
				return true;
			}

			return false;
		}

		template <typename T>
		bool write(const T* item, uint32_t count = 1, uint64_t* pOffset = nullptr) {
			if (!m_fp || !m_bOpenedWrite) {
				return false;
			}

			if (!m_bCanWrite) {
				seek_cur();
			}

			if (pOffset) {
				*pOffset = _ftelli64(m_fp);
			}

			m_bCanRead = false;

			if (fwrite(item, sizeof(*item), count, m_fp) == count) {
				return true;
			}

			return false;
		}

		bool initEncoder(FrameDimensions dims) {
			assert(!m_bEncoderInitialized);
			m_bEncoderInitialized = m_encoder.CompressBegin(dims);
			if (m_bEncoderInitialized) {
				const size_t tempBufSize = (size_t)(dims.GetSizeBytes() * 1.1);

				if (m_endecodeTempBuf.empty()) {
					m_endecodeTempBuf.resize(tempBufSize);
				}

				assert((m_endecodeTempBuf.size() == tempBufSize) &&
				       "encoder/decoder set up for different sizes!");
			}
			return m_bEncoderInitialized;
		}

		bool initDecoder(FrameDimensions dims) {
			assert(!m_bDecoderInitialized);
			m_bDecoderInitialized = m_decoder.CompressBegin(dims);
			if (m_bDecoderInitialized) {
				const size_t tempBufSize = (size_t)(dims.GetSizeBytes() * 1.1);

				if (m_endecodeTempBuf.empty()) {
					m_endecodeTempBuf.resize(tempBufSize);
				}

				assert((m_endecodeTempBuf.size() == tempBufSize) &&
				       "encoder/decoder set up for different sizes!");
			}
			return m_bDecoderInitialized;
		}

		FILE*                      m_fp           = nullptr;
		bool                       m_bCanWrite    = false;
		bool                       m_bCanRead     = false;
		bool                       m_bOpenedWrite = false;
		std::vector<FrameLocation> m_frameLocations;
		AVIHChunkData              m_avihData;
		STRHChunkData              m_strhData;
		STRFChunkData              m_strfData;
		ListHeader                 m_moviList;
		std::vector<uint8_t>       m_endecodeTempBuf;

		uint64_t m_avihOffset = 0;
		uint64_t m_strhOffset = 0;
		uint64_t m_moviOffset = 0;

		Codec m_decoder;
		Codec m_encoder;

		bool m_bDecoderInitialized = false;
		bool m_bEncoderInitialized = false;
	};

#if USE_TEMP_IMPL_FORMAT
	struct TempImplOffsets {
		FrameLocation m_offsets[1];
	};

	enum : uint32_t { kTempImplMagic = 0x1a651a65 };

	struct TempImplFileHeader {
		uint32_t        m_magic;
		FrameDimensions m_frameDims;
		uint32_t        m_frameCount           = 0;
		uint64_t        m_frameLocationsOffset = 0;
	};
#endif // USE_TEMP_IMPL_FORMAT
}

using namespace Impl;

LagsFile::LagsFile() : m_state(new LagsFileState()) {}

LagsFile::~LagsFile() {
	Close();
}

bool LagsFile::OpenRead(const std::string& path) {
	Close();

	if (!m_state->openRead(path.c_str())) {
		return false;
	}

	bool result = false;

#if USE_TEMP_IMPL_FORMAT
	TempImplFileHeader hdr;

	result = m_state->read(&hdr);

	result = result && (hdr.m_magic == kTempImplMagic);

	if (result) {
		m_frameDims  = hdr.m_frameDims;
		m_frameCount = hdr.m_frameCount;
		m_state->m_frameLocations.resize(hdr.m_frameCount);
	}

	result = result && m_state->seek_set(hdr.m_frameLocationsOffset);
	result = result && m_state->read(m_state->m_frameLocations.data(), m_frameCount);
#else

	ListHeader  riffAVI;
	ListHeader  listHDRL;
	ChunkHeader chunkAVIH;
	ListHeader  listSTRL;
	ChunkHeader chunkSTRH;
	ChunkHeader chunkSTRF;
	ChunkHeader chunkIndex;

	result = m_state->read(&riffAVI) && riffAVI.m_listCC == k4CC_RIFF && riffAVI.m_fourCC == k4CC_AVI;
	result = result && m_state->read(&listHDRL) && listHDRL.m_listCC == k4CC_LIST &&
	         listHDRL.m_fourCC == k4CC_hdrl;
	result = result && m_state->read(&chunkAVIH) && chunkAVIH.m_fourCC == k4CC_avih &&
	         chunkAVIH.m_sizeBytes == sizeof(AVIHChunkData);
	result =
	  result && m_state->read(&(m_state->m_avihData)) && m_state->m_avihData.m_flags == AVIF_HASINDEX;
	result = result && m_state->read(&listSTRL) && listSTRL.m_listCC == k4CC_LIST &&
	         listSTRL.m_fourCC == k4CC_strl;
	result = result && m_state->read(&chunkSTRH) && chunkSTRH.m_fourCC == k4CC_strh &&
	         chunkSTRH.m_sizeBytes == sizeof(STRHChunkData);
	result = result && m_state->read(&(m_state->m_strhData)) &&
	         m_state->m_strhData.m_fccHandler == k4CC_LAGS;
	result = result && m_state->read(&chunkSTRF) && chunkSTRF.m_fourCC == k4CC_strf &&
	         chunkSTRF.m_sizeBytes == sizeof(STRFChunkData);
	result = result && m_state->read(&(m_state->m_strfData)) &&
	         m_state->m_strfData.m_compression == 0 &&
	         (m_state->m_strfData.m_bitCount == 24 || m_state->m_strfData.m_bitCount == 32);
	result = result && m_state->read(&(m_state->m_moviList));

	if (result) {
		m_frameCount          = m_state->m_strhData.m_length;
		m_frameDims.bpp       = (BitsPerPixel)m_state->m_strfData.m_bitCount;
		m_frameDims.w         = m_state->m_strfData.m_width;
		m_frameDims.h         = m_state->m_strfData.m_height;
		m_state->m_moviOffset = _ftelli64(m_state->m_fp) - sizeof(uint32_t);
	}

	result = result && m_state->seek_cur(m_state->m_moviList.m_sizeBytes - sizeof(uint32_t));

	const uint32_t expectedIndexSize = sizeof(AVIIndexEntry) * m_frameCount;

	result = result && m_state->read(&chunkIndex) && chunkIndex.m_fourCC == k4CC_idx1 &&
	         chunkIndex.m_sizeBytes == expectedIndexSize;

	{
		std::vector<AVIIndexEntry> index;

		if (result) {
			index.resize(m_frameCount);
			result = result && m_state->read(index.data(), m_frameCount);

			// offsets are relative to 'movi' 4cc in movi list.
			// if the first one isn't 4 it's corrupted or using absolute location.
			if (index[0].m_chunkOffset != sizeof(uint32_t)) {
				result = false;
			}
		}

		if (result) {
			m_state->m_frameLocations.resize(m_frameCount);

			for (uint32_t f = 0; f < m_frameCount; f++) {
				FrameLocation&       fl = m_state->m_frameLocations[f];
				const AVIIndexEntry& ie = index[f];

				fl.m_frameOffset = m_state->m_moviOffset + ie.m_chunkOffset + sizeof(ChunkHeader);
				fl.m_frameSize   = ie.m_chunkLength;
			}
		}
	}

#endif // USE_TEMP_IMPL_FORMAT

	result = result && m_state->initDecoder(m_frameDims);

	if (!result) {
		Close();
	}

	return result;
}

bool LagsFile::OpenWrite(const std::string& path, const FrameDimensions& frameDims) {
	Close();

	if (!m_state->openWrite(path.c_str())) {
		return false;
	}

	bool result = true;

	m_frameDims = frameDims;

#if USE_TEMP_IMPL_FORMAT

	TempImplFileHeader hdr = {kTempImplMagic, m_frameDims, m_frameCount, 0};

	result = m_state->write(&hdr);

#else // USE_TEMP_IMPL_FORMAT

	ListHeader  riffAVI(k4CC_AVI);
	ListHeader  listHDRL(k4CC_hdrl);
	ChunkHeader chunkAVIH(k4CC_avih);

	m_state->m_avihData.m_width  = frameDims.w;
	m_state->m_avihData.m_height = frameDims.h;
	// fill in later? leave as zero?
	m_state->m_avihData.m_maxBytesPerSec = 0;

	ListHeader  listSTRL(k4CC_strl);
	ChunkHeader chunkSTRH(k4CC_strh);

	//m_state->m_strhData.m_frameRect.right  = frameDims.w;
	//m_state->m_strhData.m_frameRect.bottom = frameDims.h;
	//m_state->m_strhData.m_sampleSize       = frameDims.GetBytesPerPixel();

	ChunkHeader chunkSTRF(k4CC_strf);

	m_state->m_strfData.m_bitCount  = (int16_t)frameDims.bpp;
	m_state->m_strfData.m_width     = frameDims.w;
	m_state->m_strfData.m_height    = frameDims.h;
	m_state->m_strfData.m_sizeImage = frameDims.GetSizeBytes();

	m_state->m_moviList             = ListHeader(k4CC_movi);
	m_state->m_moviList.m_sizeBytes = sizeof(uint32_t);

	result = m_state->write(&riffAVI);
	result = result && m_state->write(&listHDRL);
	result = result && m_state->write(&chunkAVIH);
	result = result && m_state->write(&(m_state->m_avihData), 1, &(m_state->m_avihOffset));
	result = result && m_state->write(&listSTRL);
	result = result && m_state->write(&chunkSTRH);
	result = result && m_state->write(&(m_state->m_strhData), 1, &(m_state->m_strhOffset));
	result = result && m_state->write(&chunkSTRF);
	result = result && m_state->write(&(m_state->m_strfData));
	result = result && m_state->write(&(m_state->m_moviList), 1, &(m_state->m_moviOffset));

#endif // USE_TEMP_IMPL_FORMAT

	result = result && m_state->initEncoder(frameDims);

	if (!result) {
		Close();
		remove(path.c_str());
		return false;
	}

	return true;
}

bool LagsFile::ReadFrame(uint32_t frameIdx, uint8_t* pDstRaster) {
	assert(m_state->m_frameLocations.size() == m_frameCount && "Internal frame accounting error!");

	if (frameIdx >= m_frameCount || !m_state->m_fp) {
		return false;
	}

	if (!m_state->m_bDecoderInitialized) {
		if (!m_state->initDecoder(m_frameDims)) {
			return false;
		}
	}

	uint64_t compressedSize = 0;
	uint8_t* pTempBuf       = m_state->m_endecodeTempBuf.data();
	bool     result         = true;

	result = result && m_state->seek_frame(frameIdx, &compressedSize);
	result = result && m_state->read(pTempBuf, compressedSize);
	result = result && m_state->m_decoder.Decompress(pTempBuf, compressedSize, pDstRaster);

	return result;
}

bool LagsFile::WriteFrame(const uint8_t* pSrcRaster) {
	FrameLocation loc = {};

	uint32_t compressedSize = 0;
	uint8_t* pTempBuf       = m_state->m_endecodeTempBuf.data();
	bool     result         = true;

	result = result && m_state->seek_end();
	result = result && m_state->m_encoder.Compress(pSrcRaster, pTempBuf, &compressedSize);

#if USE_TEMP_IMPL_FORMAT
	result = result && m_state->write(pTempBuf, compressedSize, &loc.m_frameOffset);
#else

	ChunkHeader chunkVFrame(k4CC_vframe);
	chunkVFrame.m_sizeBytes = compressedSize;

	const uint32_t paddedSize = chunkVFrame.getPaddedDataSize();

	for (uint32_t p = compressedSize; p < paddedSize; p++) {
		pTempBuf[p] = 0;
	}

	result = result && m_state->write(&chunkVFrame);
	result = result && m_state->write(pTempBuf, paddedSize, &loc.m_frameOffset);

	m_state->m_moviList.m_sizeBytes += (sizeof(chunkVFrame) + paddedSize);

#endif // USE_TEMP_IMPL_FORMAT

	if (result) {
		loc.m_frameSize = compressedSize;
		m_state->m_frameLocations.push_back(loc);
		m_frameCount++;

		assert(m_state->m_frameLocations.size() == m_frameCount && "Internal frame accounting error!");
	}

	return result;
}

bool LagsFile::Close() {
	if (!m_state->m_fp) {
		return false;
	}

	bool result = true;

	if (m_state->m_bOpenedWrite) {
#if USE_TEMP_IMPL_FORMAT
		TempImplFileHeader hdr = {kTempImplMagic, m_frameDims, m_frameCount, 0};

		assert(m_state->m_frameLocations.size() == m_frameCount && "Internal frame accounting error!");

		result = m_state->seek_end();
		result = result && m_state->write(m_state->m_frameLocations.data(), m_frameCount,
		                                  &(hdr.m_frameLocationsOffset));
		result = result && m_state->seek_set(0);
		result = result && m_state->write(&hdr);
#else
		uint32_t maxFrameSize = 0;

		result = result && m_state->seek_end();

		// for re-writing main file list header now that we can know final size.
		ListHeader riffAVI(k4CC_AVI);

		// dump index list as last chunk after movi list
		{
			// offset of 'movi' 4cc
			const uint32_t moviStart = m_state->m_moviOffset + kListHeaderSize;

			std::vector<AVIIndexEntry> index;
			index.resize(m_frameCount);

			uint32_t f = 0;
			for (const FrameLocation& fl : m_state->m_frameLocations) {
				AVIIndexEntry& ie = index[f];

				// offsets are to the beginning of chunk header (not chunk data) and are relative to start of 'movi' 4cc
				ie.m_chunkOffset = (uint32_t)((fl.m_frameOffset - sizeof(ChunkHeader)) - moviStart);
				ie.m_chunkLength = (uint32_t)(fl.m_frameSize);

				// track max chunk size for estimating max data rate and recommending buffer size
				if (ie.m_chunkLength > maxFrameSize) {
					maxFrameSize = ie.m_chunkLength;
				}

				f++;
			}

			ChunkHeader chunkIndex(k4CC_idx1);
			chunkIndex.m_sizeBytes = (uint32_t)index.size() * sizeof(AVIIndexEntry);

			uint64_t indexOffset = 0;

			result = result && m_state->write(&chunkIndex);
			result = result && m_state->write(index.data(), (uint32_t)index.size(), &indexOffset);

			riffAVI.m_sizeBytes = (uint32_t)((indexOffset + chunkIndex.m_sizeBytes) - kListHeaderSize);
		}

		m_state->m_avihData.m_totalFrames         = m_frameCount;
		m_state->m_strhData.m_suggestedBufferSize = sizeof(ChunkHeader) + maxFrameSize;
		m_state->m_avihData.m_maxBytesPerSec      = maxFrameSize * kDefaultFPS;
		m_state->m_strhData.m_length              = m_frameCount;

		result = result && m_state->seek_set(0);
		result = result && m_state->write(&riffAVI);

		result = result && m_state->seek_set(m_state->m_avihOffset);
		result = result && m_state->write(&(m_state->m_avihData));

		result = result && m_state->seek_set(m_state->m_strhOffset);
		result = result && m_state->write(&(m_state->m_strhData));

		result = result && m_state->seek_set(m_state->m_moviOffset);
		result = result && m_state->write(&(m_state->m_moviList));

#endif // USE_TEMP_IMPL_FORMAT
	}

	m_frameDims  = FrameDimensions();
	m_frameCount = 0;

	m_state->~LagsFileState();
	new (m_state.get()) LagsFileState();

	return result;
}

bool LagsFile::IsWriteMode() const {
	return m_state->m_bCanWrite;
}

} // namespace Lagarith
