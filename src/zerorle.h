#pragma once

int          deRLE(const unsigned char* in, unsigned char* out, const int length, const int level);
unsigned int TestAndRLE(unsigned char* const in, unsigned char* const out1,
                        unsigned char* const out3, const unsigned int length, int* level);
