//This file contains functions that perform and undo median predition.
#pragma once

void Block_Predict(const uint8_t* source, uint8_t* dest, uint32_t width,
                   uint32_t length, bool rgbmode);

void Decorrelate_And_Split_RGB24(const uint8_t* in, uint8_t* rdst, uint8_t* gdst,
                                 uint8_t* bdst, uint32_t width,
                                 uint32_t height);
void Decorrelate_And_Split_RGB32(const uint8_t* in, uint8_t* rdst, uint8_t* gdst,
                                 uint8_t* bdst, uint32_t width,
                                 uint32_t height);
void Interleave_And_Restore_RGB24(uint8_t* out, const uint8_t* rsrc,
                                  const uint8_t* gsrc, const uint8_t* bsrc,
                                  uint32_t width, uint32_t height);
void Interleave_And_Restore_RGB32(uint8_t* out, const uint8_t* rsrc,
                                  const uint8_t* gsrc, const uint8_t* bsrc,
                                  uint32_t width, uint32_t height);
