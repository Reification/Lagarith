#include "lagarith_internal.h"
#include "lagarith.h"

namespace Lagarith {

namespace Impl {
	inline static constexpr uint32_t genFourCC(const char* const code) noexcept {
		return (static_cast<uint32_t>(code[0]) << 24) |
		       (static_cast<uint32_t>(code[0] ? code[1] : 0) << 16) |
		       (static_cast<uint32_t>(code[0] && code[1] ? code[2] : 0) << 8) |
		       static_cast<uint32_t>(code[0] && code[1] && code[2] ? code[3] : 0);
	}

	enum : uint32_t {
		k4CC_RIFF = genFourCC("RIFF"),
		k4CC_AVI  = genFourCC("AVI"),
		k4CC_AVIX = genFourCC("AVIX"),
		k4CC_LIST = genFourCC("LIST"),
		k4CC_hdrl = genFourCC("hdrl"),
		k4CC_avih = genFourCC("avih"),
		k4CC_idx1 = genFourCC("idx1"),
		k4CC_strl = genFourCC("strl"),
		k4CC_strh = genFourCC("strh"),
		k4CC_strf = genFourCC("strf"),
		k4CC_movi = genFourCC("movi"),
		k4CC_vids = genFourCC("vids"),

		k4CC_LAGS = genFourCC("LAGS")
	};

	enum : uint32_t {
		// maybe should be 0?
		kDefaultPaddingGranularity = 4,
		kDefaultFPS                = 60
	};

	inline uint32_t calcPaddedDataSize(uint32_t size, uint32_t granularity) {
		if (granularity <= 1) {
			return size;
		}

		return ((size / granularity) + !!(size % granularity)) * granularity;
	}

	struct ChunkHeader {
		uint32_t m_fourCC;
		uint32_t m_sizeBytes;

		ChunkHeader() : m_fourCC(0), m_sizeBytes(0) {}

		explicit ChunkHeader(uint32_t fourCC);

		uint32_t getPaddedDataSize(uint32_t granularity) const {
			return calcPaddedDataSize(m_sizeBytes, granularity);
		}
	};

	struct ListHeader {
		uint32_t m_listCC;    // 'RIFF' or 'LIST'
		uint32_t m_sizeBytes; // size covers m_fourCC (4) plus sizeof data.
		uint32_t m_fourCC;    // 'AVI' or 'AVIX'

		ListHeader() : m_listCC(0), m_sizeBytes(0), m_fourCC(0) {}

		explicit ListHeader(uint32_t fourCC);

		uint32_t getPaddedDataSize(uint32_t granularity) const {
			return calcPaddedDataSize(m_sizeBytes, granularity);
		}
	};

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

		// probably ok to be 0.
		uint32_t m_maxBytesPerSec = 0;

		// pad sizes to multiples of this size;
		uint32_t m_paddingGranularity = kDefaultPaddingGranularity;

		// the ever-present flags - combination of AVIF_ values above - just set AVIF_HASINDEX
		uint32_t m_flags = AVIF_HASINDEX;

		// # frames in RIFF-AVI section (does not count additional RIFF-AVIX sections)
		uint32_t m_totalFrames = 0;

		// set to 0
		uint32_t m_initialFrames = 0;

		// set to 1 (video only)
		uint32_t m_streams = 1;

		// may be ok to set to 0. set to sizeof chunk header + sizeof largest compressed frame in file.
		uint32_t m_suggestedBufferSize = 0;
		uint32_t m_width               = 0; // frame width
		uint32_t m_height              = 0; // frame height
		uint32_t m_reserved[4]         = {};

		uint32_t calcPaddedDataSize(uint32_t size) const {
			return ::Lagarith::Impl::calcPaddedDataSize(size, m_paddingGranularity);
		}
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
		uint32_t m_flags         = 0;
		uint16_t m_priority      = 0;
		uint16_t m_language      = 0;
		uint32_t m_initialFrames = 0;
		uint32_t m_scale         = 1;
		uint32_t m_rate          = kDefaultFPS; /* m_rate / m_scale == samples/second */
		uint32_t m_start         = 0;
		uint32_t m_length; // frame count
		uint32_t m_suggestedBufferSize = 0;
		uint32_t m_quality             = 1;
		uint32_t m_sampleSize          = 0; // bytes per pixel
		Rect     m_frameRect           = {};
	};

	// aka bitmapinfoheader
	struct STRFChunkData {
		uint32_t m_size          = (uint32_t)sizeof(STRFChunkData);
		int32_t  m_width         = 0; // frame width
		int32_t  m_height        = 0; // frame height
		int16_t  m_planes        = 1;
		int16_t  m_bitCount      = 0; // bits per pixel from FrameDimension
		uint32_t m_compression   = 0; // BI_RGB
		uint32_t m_sizeImage     = 0;
		int32_t  m_xPelsPerMeter = 0;
		int32_t  m_yPelsPerMeter = 0;
		uint32_t m_clrUsed       = 0;
		uint32_t m_clrImportant  = 0;
	};

	enum : uint64_t {
		kChunkSize_avih = sizeof(ChunkHeader) + sizeof(AVIHChunkData),
		kChunkSize_strh = sizeof(ChunkHeader) + sizeof(STRHChunkData),
		kChunkSize_strf = sizeof(ChunkHeader) + sizeof(STRFChunkData),

		kListDataSize_strl = kChunkSize_strh + kChunkSize_strf,
		kListDataSize_hdrl = sizeof(kChunkSize_avih) + sizeof(ListHeader) + kListDataSize_strl,

		kOffset_avih = sizeof(ListHeader) + sizeof(ListHeader) + sizeof(ChunkHeader),
		kOffset_strh = kOffset_avih + sizeof(AVIHChunkData) + sizeof(ListHeader) + sizeof(ChunkHeader),
		kOffset_strf = kOffset_strh + sizeof(STRHChunkData) + sizeof(ChunkHeader)
	};

	ChunkHeader::ChunkHeader(uint32_t fourCC) : m_fourCC(fourCC), m_sizeBytes(0) {
		switch (fourCC) {
		case k4CC_avih: m_sizeBytes = sizeof(AVIHChunkData); break;
		case k4CC_strh: m_sizeBytes = sizeof(STRHChunkData); break;
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

	struct LagsFileState {
		~LagsFileState() {
			if (m_fp) {
				fclose(m_fp);
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

		bool seek_frame(uint32_t frameIdx) {
			if (frameIdx >= m_frameOffsets.size()) {
				return false;
			}

			return seek_set(m_frameOffsets[frameIdx]);
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

		FILE*                 m_fp           = nullptr;
		bool                  m_bCanWrite    = false;
		bool                  m_bCanRead     = false;
		bool                  m_bOpenedWrite = false;
		std::vector<uint64_t> m_frameOffsets;
		AVIHChunkData         m_avihData;
		STRHChunkData         m_strhData;
		STRFChunkData         m_strfData;
	};

	struct TempImplOffsets {
		uint64_t m_frameOffsets[1];
	};

	enum : uint32_t { kTempImplMagic = 0x1a651a64 };

	struct TempImplFileHeader {
		uint32_t        m_magic;
		FrameDimensions m_frameDims;
		uint32_t        m_frameCount         = 0;
		uint64_t        m_frameOffsetsOffset = 0;
	};
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

	TempImplFileHeader hdr;

	bool result = m_state->read(&hdr);

	result = result && (hdr.m_magic == kTempImplMagic);

	if (result) {
		m_frameDims  = hdr.m_frameDims;
		m_frameCount = hdr.m_frameCount;
		m_state->m_frameOffsets.resize(hdr.m_frameCount);
	}

	result = result && m_state->seek_set(hdr.m_frameOffsetsOffset);
	result = result && m_state->read(m_state->m_frameOffsets.data(), m_frameCount);

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

#if 0
	ListHeader  riffAVI(k4CC_AVI);
	ListHeader  listHDRL(k4CC_hdrl);
	ChunkHeader chunkAVIH(k4CC_avih);

	m_state->m_avihData.m_width  = frameDims.w;
	m_state->m_avihData.m_height = frameDims.h;
	// fill in later? leave as zero?
	m_state->m_avihData.m_maxBytesPerSec = 0;

	m_state->m_strhData.m_frameRect.right  = frameDims.w;
	m_state->m_strhData.m_frameRect.bottom = frameDims.h;
	m_state->m_strhData.m_sampleSize       = frameDims.GetBytesPerPixel();

	m_state->m_strfData.m_bitCount = (int16_t)frameDims.bpp;
	m_state->m_strfData.m_width    = frameDims.w;
	m_state->m_strfData.m_height   = frameDims.h;
#endif // 0

	TempImplFileHeader hdr = {kTempImplMagic, m_frameDims, m_frameCount, 0};

	if (!m_state->write(&hdr)) {
		Close();
		return false;
	}

	m_frameDims = frameDims;

	return true;
}

bool LagsFile::ReadFrame(uint32_t frameIdx, uint8_t* pDstRaster) {
	assert(m_state->m_frameOffsets.size() == m_frameCount && "Internal frame accounting error!");

	if (frameIdx >= m_frameCount || !m_state->m_fp) {
		return false;
	}

	if (!m_state->seek_frame(frameIdx)) {
		return false;
	}

	if (!m_state->read(pDstRaster, m_frameDims.GetSizeBytes())) {
		return false;
	}

	return true;
}

bool LagsFile::WriteFrame(const uint8_t* pSrcRaster) {
	uint64_t frameOffset = 0;

	if (!m_state->seek_end() ||
	    !m_state->write(pSrcRaster, m_frameDims.GetSizeBytes(), &frameOffset)) {
		return false;
	}

	m_state->m_frameOffsets.push_back(frameOffset);
	m_frameCount++;

	assert(m_state->m_frameOffsets.size() == m_frameCount && "Internal frame accounting error!");

	return true;
}

bool LagsFile::Close() {
	if (!m_state->m_fp) {
		return false;
	}

	bool result = true;

	if (m_state->m_bOpenedWrite) {
		TempImplFileHeader hdr = {kTempImplMagic, m_frameDims, m_frameCount, 0};

		assert(m_state->m_frameOffsets.size() == m_frameCount && "Internal frame accounting error!");

		result = m_state->seek_end();
		result = result && m_state->write(m_state->m_frameOffsets.data(), m_frameCount,
		                                  &(hdr.m_frameOffsetsOffset));
		result = result && m_state->seek_set(0);
		result = result && m_state->write(&hdr);
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
