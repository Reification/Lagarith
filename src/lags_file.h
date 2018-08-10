#pragma once

#include "lagarith.h"
#include "lagarith_internal.h"

class LagsFile {
public:
	using FrameDimensions = Lagarith::FrameDimensions;

public:
	LagsFile() {}

	bool OpenRead(const std::string& path);
	bool ReadFrame(uint8_t* pDstRaster);

	bool OpenWrite(const std::string& path, const FrameDimensions& frameDims);
	bool WriteFrame(const uint8_t* pSrcRaster);

	bool Close();

	const FrameDimensions& GetFrameDimensions() const { return m_frameDims; }
	uint32_t               GetFrameCount() const { return m_frameCount; }

private:
	FrameDimensions m_frameDims;
	uint32_t        m_frameCount = 0;
	FILE*           m_fp         = nullptr;
	bool            m_writeMode  = false;
};
