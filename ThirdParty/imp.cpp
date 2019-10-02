#pragma warning(push)
#pragma warning(disable:6011)
#pragma warning(disable:6054)
#pragma warning(disable:6262)
#pragma warning(disable:6308)
#pragma warning(disable:6387)
#pragma warning(disable:26495)
#pragma warning(disable:28182)

#include <stdint.h>

#define STB_IMAGE_IMPLEMENTATION
#include <ThirdParty/stb_image.h>

#define stbtt_uint8 uint8_t
#define stbtt_int8 int8_t
#define stbtt_uint16 uint16_t
#define stbtt_int16 int16_t
#define stbtt_uint32 uint32_t
#define stbtt_int32 int32_t

#define STB_TRUETYPE_IMPLEMENTATION
#include <ThirdParty/stb_truetype.h>

#pragma warning(pop)