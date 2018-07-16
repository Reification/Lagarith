//This file contains functions that perform and undo median predition.
#pragma once

// Used to compare and select between SSE and SSE2 versions of functions
struct Performance {
	int              count;
	bool             prefere_a;
	unsigned __int64 time_a;
	unsigned __int64 time_b;
};

void Block_Predict(const unsigned char* source, unsigned char* dest,
                   const unsigned int width, const unsigned int length, const bool rgbmode);
void Block_Predict_YUV16(const unsigned char* source, unsigned char* dest,
                         const unsigned int width, const unsigned int length, const bool is_y);

void Decorrilate_And_Split_RGB24(const unsigned char* in, unsigned char* rdst,
                                 unsigned char* gdst, unsigned char* bdst,
                                 const unsigned int width, const unsigned int height,
                                 Performance* performance);
void Decorrilate_And_Split_RGB32(const unsigned char* in, unsigned char* rdst,
                                 unsigned char* gdst, unsigned char* bdst,
                                 const unsigned int width, const unsigned int height,
                                 Performance* performance);
void Decorrilate_And_Split_RGBA(const unsigned char* in, unsigned char* rdst,
                                unsigned char* gdst, unsigned char* bdst,
                                unsigned char* adst, const unsigned int width,
                                const unsigned int height, Performance* performance);
void Split_YUY2(const unsigned char* src, unsigned char* ydst,
                unsigned char* udst, unsigned char* vdst,
                const unsigned int width, const unsigned int height, Performance* performance);
void Split_UYVY(const unsigned char* src, unsigned char* ydst,
                unsigned char* udst, unsigned char* vdst,
                const unsigned int width, const unsigned int height, Performance* performance);

void Interleave_And_Restore_RGB24(unsigned char* out,
                                  const unsigned char* rsrc,
                                  const unsigned char* gsrc,
                                  const unsigned char* bsrc, const unsigned int width,
                                  const unsigned int height, Performance* performance);
void Interleave_And_Restore_RGB32(unsigned char* out,
                                  const unsigned char* rsrc,
                                  const unsigned char* gsrc,
                                  const unsigned char* bsrc, const unsigned int width,
                                  const unsigned int height, Performance* performance);
void Interleave_And_Restore_RGBA(unsigned char* out,
                                 const unsigned char* rsrc,
                                 const unsigned char* gsrc,
                                 const unsigned char* bsrc,
                                 const unsigned char* asrc, const unsigned int width,
                                 const unsigned int height, Performance* performance);
void Interleave_And_Restore_YUY2(unsigned char* out,
                                 const unsigned char* ysrc,
                                 const unsigned char* usrc,
                                 const unsigned char* vsrc, const unsigned int width,
                                 const unsigned int height, Performance* performance);
void Restore_YV12(unsigned char* ysrc, unsigned char* usrc,
                  unsigned char* vsrc, const unsigned int width,
                  const unsigned int height, Performance* performance);

void Interleave_And_Restore_Old_Unaligned(unsigned char* bsrc, unsigned char* gsrc,
                                          unsigned char* rsrc, unsigned char* dst,
                                          unsigned char* buffer, bool rgb24, unsigned int width,
                                          unsigned int height);
void Double_Resolution(const unsigned char* src, unsigned char* dst, unsigned char* buffer,
                       const unsigned int width, const unsigned int height);
