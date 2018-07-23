//This file contains functions that perform and undo median predition.
#pragma once

void Block_Predict(const unsigned char* source, unsigned char* dest, const unsigned int width,
                   const unsigned int length, bool rgbmode);

void Decorrelate_And_Split_RGB24(const unsigned char* in, unsigned char* rdst, unsigned char* gdst,
                                 unsigned char* bdst, unsigned int width,
                                 unsigned int height);
void Decorrelate_And_Split_RGB32(const unsigned char* in, unsigned char* rdst, unsigned char* gdst,
                                 unsigned char* bdst, unsigned int width,
                                 unsigned int height);
void Decorrelate_And_Split_RGBA(const unsigned char* in, unsigned char* rdst, unsigned char* gdst,
                                unsigned char* bdst, unsigned char* adst, unsigned int width,
                                unsigned int height);
void Interleave_And_Restore_RGB24(unsigned char* out, const unsigned char* rsrc,
                                  const unsigned char* gsrc, const unsigned char* bsrc,
                                  unsigned int width, unsigned int height);
void Interleave_And_Restore_RGB32(unsigned char* out, const unsigned char* rsrc,
                                  const unsigned char* gsrc, const unsigned char* bsrc,
                                  unsigned int width, unsigned int height);
void Interleave_And_Restore_RGBA(unsigned char* out, const unsigned char* rsrc,
                                 const unsigned char* gsrc, const unsigned char* bsrc,
                                 const unsigned char* asrc, unsigned int width, unsigned int height);
