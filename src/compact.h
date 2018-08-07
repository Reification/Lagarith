#pragma once

namespace Lagarith {
class CompressClass {
public:
	uint32_t prob_ranges[257] = {}; // Byte probablity range table
	uint32_t scale            = 0;  // Used to replace some multiply/divides with binary shifts,
	                                // (1<<scale) is equal to the cumulative probabilty of all bytes

	uint8_t* buffer = nullptr; // buffer to perform RLE when encoding

	CompressClass();
	~CompressClass();

	bool InitCompressBuffers(const uint32_t length);
	void FreeCompressBuffers();

	uint32_t Compact(uint8_t* in, uint8_t* out, const uint32_t length);

	void Uncompact(const uint8_t* in, uint8_t* out, const uint32_t length);

	uint32_t Calcprob(const uint8_t* in, uint32_t length, uint8_t* out = 0);
	void     Scaleprob(const uint32_t length);
	uint32_t Readprob(const uint8_t* in);

	uint32_t Encode(const uint8_t* in, uint8_t* out, const uint32_t length);

	void Decode_And_DeRLE(const uint8_t* in, uint8_t* out, const uint32_t length, uint32_t level);
};
}
