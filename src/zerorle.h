#pragma once

int          deRLE(const unsigned char* in, unsigned char* out, int length, int level);
unsigned int TestAndRLE(unsigned char* const in, unsigned char* const out1,
                        unsigned char* const out3, unsigned int length, int* level);
