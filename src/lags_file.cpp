#include "lagarith_internal.h"
#include "lagarith.h"

namespace Lagarith {

bool LagsFile::OpenRead( const std::string& path )
{
	(void)path;
	return false;
}

bool LagsFile::ReadFrame( uint32_t frameIdx, uint8_t* pDstRaster )
{
	(void)frameIdx;
	(void)pDstRaster;
	return false;
}

bool LagsFile::OpenWrite( const std::string& path, const FrameDimensions& frameDims )
{
	(void)path;

	m_frameDims = frameDims;
	(void)m_frameCount;
	(void)m_fp;
	(void)m_writeMode;

	return false;
}

bool LagsFile::WriteFrame( const uint8_t* pSrcRaster )
{
	(void)pSrcRaster;
	return false;
}

void LagsFile::Close() {
}

} // namespace Lagarith
