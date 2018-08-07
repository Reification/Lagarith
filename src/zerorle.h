#pragma once

int      deRLE(const uint8_t* in, uint8_t* out, int length, int level);
uint32_t TestAndRLE(uint8_t* const in, uint8_t* const out1, uint8_t* const out3, uint32_t length,
                    int* level);
