//This file contains functions that perform and undo median predition.
#pragma once

void Block_Predict(const unsigned char* source, unsigned char* dest, unsigned int width,
                   unsigned int length, bool rgbmode);

void Decorrelate_And_Split_RGB24(const unsigned char* in, unsigned char* rdst, unsigned char* gdst,
                                 unsigned char* bdst, unsigned int width,
                                 unsigned int height);
void Decorrelate_And_Split_RGB32(const unsigned char* in, unsigned char* rdst, unsigned char* gdst,
                                 unsigned char* bdst, unsigned int width,
                                 unsigned int height);
void Interleave_And_Restore_RGB24(unsigned char* out, const unsigned char* rsrc,
                                  const unsigned char* gsrc, const unsigned char* bsrc,
                                  unsigned int width, unsigned int height);
void Interleave_And_Restore_RGB32(unsigned char* out, const unsigned char* rsrc,
                                  const unsigned char* gsrc, const unsigned char* bsrc,
                                  unsigned int width, unsigned int height);