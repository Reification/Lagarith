//we only need to read png images
//#define STBI_NO_PNG
#define STBI_NO_TGA
#define STBI_NO_BMP
#define STBI_NO_PNM
#define STBI_NO_JPEG
#define STBI_NO_PSD
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC

// disable windows fopen security warning in stb headers.
#if defined(WIN32)
# define _CRT_SECURE_NO_WARNINGS 1
#endif

#define STB_IMAGE_IMPLEMENTATION 1
//#define STB_IMAGE_WRITE_IMPLEMENTATION 1
#define STB_DEFINE 1

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunused-function"
# pragma clang diagnostic ignored "-Wwritable-strings"
# pragma clang diagnostic ignored "-Wint-conversion"
# pragma clang diagnostic ignored "-Wformat"
# pragma clang diagnostic ignored "-Wmultichar"
# pragma clang diagnostic ignored "-Wabsolute-value"
# pragma clang diagnostic ignored "-Wint-to-pointer-cast"
# pragma clang diagnostic ignored "-Wint-to-void-pointer-cast"
# pragma clang diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif // __clang__

#include "stb/stb.h"
#include "stb/stb_image.h"
//#include "stb/stb_image_write.h"

#if defined(__clang__)
# pragma clang diagnostic pop
#endif // __clang__
