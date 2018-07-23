#pragma once

class CompressClass {
public:
	unsigned int prob_ranges[257] = {}; // Byte probablity range table
	unsigned int scale = 0;            // Used to replace some multiply/divides with binary shifts,
	                               // (1<<scale) is equal to the cumulative probabilty of all bytes

	unsigned char* buffer = nullptr; // buffer to perform RLE when encoding

	CompressClass();
	~CompressClass();

	bool InitCompressBuffers(const unsigned int length);
	void FreeCompressBuffers();

	unsigned int Compact(unsigned char* in, unsigned char* out, const unsigned int length);

	void Uncompact(const unsigned char* in, unsigned char* out,
	               const unsigned int length);

	unsigned int Calcprob(const unsigned char* in, unsigned int length, unsigned char* out = 0);
	void         Scaleprob(const unsigned int length);
	unsigned int Readprob(const unsigned char* in);

	unsigned int Encode(const unsigned char* in, unsigned char* out, const unsigned int length);

	void         Decode_And_DeRLE(const unsigned char* in, unsigned char* out,
	                              const unsigned int length, unsigned int level);
};
