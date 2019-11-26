#pragma once

#ifdef WINDOWS
#include <winsock2.h>
#include <Windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>
#undef near
#undef far
#undef free
#endif

#include <stdexcept>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <cstring>
#include <mutex>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <queue>
#include <set>
#include <variant>
#include <optional>

#include <vulkan/vulkan.h>

#include <Math/Geometry.hpp>

#ifdef WINDOWS
#ifdef ENGINE_CORE
#define ENGINE_EXPORT __declspec(dllexport)
#define PLUGIN_EXPORT
#else
#define ENGINE_EXPORT __declspec(dllimport)
#define PLUGIN_EXPORT __declspec(dllexport)
#endif
#else
#ifdef ENGINE_CORE
#define ENGINE_EXPORT
#define PLUGIN_EXPORT
#else
#define ENGINE_EXPORT
#define PLUGIN_EXPORT
#endif
#endif

#define safe_delete(x) if (x != nullptr) { delete x; x = nullptr; }
#define safe_delete_array(x) if (x != nullptr) { delete[] x; x = nullptr; }

enum BlendMode {
	Opaque = 0,
	Alpha = 1,
	Additive = 2,
	Multiply = 3,
	BLEND_MODE_MAX_ENUM = 0x7FFFFFFF
};

enum ConsoleColor {
	Red,
	Green,
	Blue,
	Yellow,
	Cyan,
	Magenta,

	BoldRed,
	BoldGreen,
	BoldBlue,
	BoldYellow,
	BoldCyan,
	BoldMagenta
};
enum PassType {
	Depth,
	Main
};

template<typename... Args>
inline void printf_color(ConsoleColor color, Args&&... a) {
	#ifdef WINDOWS
	int c = 0;
	switch(color) {
		case Red:
		case BoldRed:
		c = FOREGROUND_RED;
		break;
		case Green:
		case BoldGreen:
		c = FOREGROUND_GREEN;
		break;
		case Blue:
		case BoldBlue:
		c = FOREGROUND_BLUE;
		break;
		case Yellow:
		case BoldYellow:
		c = FOREGROUND_RED | FOREGROUND_GREEN;
		break;
		case Cyan:
		case BoldCyan:
		c = FOREGROUND_BLUE | FOREGROUND_GREEN;
		break;
		case Magenta:
		case BoldMagenta:
		c = FOREGROUND_RED | FOREGROUND_BLUE;
		break;
	}
	if (color >= 6) c |= FOREGROUND_INTENSITY;
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), c);
	#else
	switch(color) {
		case Red:
		printf("\x1B[0;31m");
		break;
		case Green:
		printf("\x1B[0;32m");
		break;
		case Blue:
		printf("\x1B[0;34m");
		break;
		case Yellow:
		printf("\x1B[0;33m");
		break;
		case Cyan:
		printf("\x1B[0;36m");
		break;
		case Magenta:
		printf("\x1B[0;35m");
		break;

		case BoldRed:
		printf("\x1B[1;31m");
		break;
		case BoldGreen:
		printf("\x1B[1;32m");
		break;
		case BoldBlue:
		printf("\x1B[1;34m");
		break;
		case BoldYellow:
		printf("\x1B[1;33m");
		break;
		case BoldCyan:
		printf("\x1B[1;36m");
		break;
		case BoldMagenta:
		printf("\x1B[1;35m");
		break;
	}
	#endif

	printf(std::forward<Args>(a)...);

	#ifdef WINDOWS
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	#else
	printf("\x1B[0m");
	#endif
}
template<typename... Args>
#ifdef WINDOWS
inline void fprintf_color(ConsoleColor color, FILE* str, Args&&... a) {
#else
inline void fprintf_color(ConsoleColor color, _IO_FILE* str, Args&&... a) {
#endif
	#ifdef WINDOWS
	int c = 0;
	switch(color) {
		case Red:
		case BoldRed:
		c = FOREGROUND_RED;
		break;
		case Green:
		case BoldGreen:
		c = FOREGROUND_GREEN;
		break;
		case Blue:
		case BoldBlue:
		c = FOREGROUND_BLUE;
		break;
		case Yellow:
		case BoldYellow:
		c = FOREGROUND_RED | FOREGROUND_GREEN;
		break;
		case Cyan:
		case BoldCyan:
		c = FOREGROUND_BLUE | FOREGROUND_GREEN;
		break;
		case Magenta:
		case BoldMagenta:
		c = FOREGROUND_RED | FOREGROUND_BLUE;
		break;
	}
	if (color >= 6) c |= FOREGROUND_INTENSITY;
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), c);
	#else
	switch(color) {
		case Red:
		fprintf(str, "\x1B[0;31m");
		break;
		case Green:
		fprintf(str, "\x1B[0;32m");
		break;
		case Blue:
		fprintf(str, "\x1B[0;34m");
		break;
		case Yellow:
		fprintf(str, "\x1B[0;33m");
		break;
		case Cyan:
		fprintf(str, "\x1B[0;36m");
		break;
		case Magenta:
		fprintf(str, "\x1B[0;35m");
		break;

		case BoldRed:
		fprintf(str, "\x1B[1;31m");
		break;
		case BoldGreen:
		fprintf(str, "\x1B[1;32m");
		break;
		case BoldBlue:
		fprintf(str, "\x1B[1;34m");
		break;
		case BoldYellow:
		fprintf(str, "\x1B[1;33m");
		break;
		case BoldCyan:
		fprintf(str, "\x1B[1;36m");
		break;
		case BoldMagenta:
		fprintf(str, "\x1B[1;35m");
		break;
	}
	#endif

	fprintf(str, std::forward<Args>(a)...);

	#ifdef WINDOWS
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	#else
	fprintf(str, "\x1B[0m");
	#endif
}

template <typename T>
inline T AlignUpWithMask(T value, size_t mask) {
	return (T)(((size_t)value + mask) & ~mask);
}
template <typename T>
inline T AlignDownWithMask(T value, size_t mask) {
	return (T)((size_t)value & ~mask);
}
template <typename T>
inline T AlignUp(T value, size_t alignment) {
	return AlignUpWithMask(value, alignment - 1);
}
template <typename T>
inline T AlignDown(T value, size_t alignment) {
	return AlignDownWithMask(value, alignment - 1);
}

template <typename T>
inline T DivideByMultiple(T value, size_t alignment) {
	return (T)((value + alignment - 1) / alignment);
}

template <typename T>
inline bool IsPowerOfTwo(T value) {
	return 0 == (value & (value - 1));
}


// Defines a vertex input. Hashes itself once at creation, then remains immutable.
// Note: Meshes store a pointer to one of these, but do not handle creation/deletion.
struct VertexInput {
public:
	const VkVertexInputBindingDescription mBinding;
	// Note: In order to hash and compare correctly, attributes must appear in order of location.
	const std::vector<VkVertexInputAttributeDescription> mAttributes;

	VertexInput(const VkVertexInputBindingDescription& binding, const std::vector<VkVertexInputAttributeDescription>& attribs)
		: mBinding(binding), mAttributes(attribs) {
		std::size_t h = 0;
		hash_combine(h, mBinding.binding);
		hash_combine(h, mBinding.inputRate);
		hash_combine(h, mBinding.stride);
		for (const auto& a : mAttributes) {
			hash_combine(h, a.binding);
			hash_combine(h, a.format);
			hash_combine(h, a.location);
			hash_combine(h, a.offset);
		}
		mHash = h;
	};

	inline bool operator==(const VertexInput& rhs) const {
		/*
		if (mBinding.binding != rhs.mBinding.binding ||
			mBinding.inputRate != rhs.mBinding.inputRate ||
			mBinding.stride != rhs.mBinding.stride ||
			mAttributes.size() != rhs.mAttributes.size()) return false;
		for (uint32_t i = 0; i < mAttributes.size(); i++)
			if (mAttributes[i].binding != rhs.mAttributes[i].binding ||
				mAttributes[i].format != rhs.mAttributes[i].format ||
				mAttributes[i].location != rhs.mAttributes[i].location ||
				mAttributes[i].offset != rhs.mAttributes[i].offset) return false;
		return true;
		*/
		return mHash == rhs.mHash;
	}

private:
	friend struct std::hash<VertexInput>;
	size_t mHash;
};

namespace std {
	template<>
	struct hash<VertexInput> {
		inline std::size_t operator()(const  VertexInput& v) const {
			return v.mHash;
		}
	};
}

inline bool ReadFile(const std::string& filename, std::string& dest) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		std::cerr << "Failed to open file " << filename.c_str() << std::endl;
		return false;
	}

	size_t fileSize = (size_t)file.tellg();
	dest.resize(fileSize);

	file.seekg(0);
	file.read(const_cast<char*>(dest.data()), fileSize);

	file.close();

	return true;
}

inline bool ReadFile(const std::string& filename, std::vector<uint8_t>& dest) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		std::cerr << "Failed to open file " << filename.c_str() << std::endl;
		return false;
	}

	size_t fileSize = (size_t)file.tellg();
	dest.resize(fileSize);

	file.seekg(0);
	file.read(reinterpret_cast<char*>(dest.data()), fileSize);

	file.close();

	return true;
}

inline std::string GetDirectory(const std::string& file) {
	size_t k = file.rfind('\\');
	if (k == std::string::npos)
		k = file.rfind('/');
	if (k == std::string::npos)
		return "";
	return file.substr(0, (int)k);
}


inline static bool HasStencilComponent(VkFormat format) {
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

inline void ThrowIfFailed(VkResult result, const std::string& message){
	if (result != VK_SUCCESS){
		const char* code = "<unknown>";
		switch (result) {
			case VK_NOT_READY: code = "VK_NOT_READY"; break;
			case VK_TIMEOUT: code = "VK_TIMEOUT"; break;
			case VK_EVENT_SET: code = "VK_EVENT_SET"; break;
			case VK_EVENT_RESET: code = "VK_EVENT_RESET"; break;
			case VK_INCOMPLETE: code = "VK_INCOMPLETE"; break;
			case VK_ERROR_OUT_OF_HOST_MEMORY: code = "VK_ERROR_OUT_OF_HOST_MEMORY"; break;
			case VK_ERROR_OUT_OF_DEVICE_MEMORY: code = "VK_ERROR_OUT_OF_DEVICE_MEMORY"; break;
			case VK_ERROR_INITIALIZATION_FAILED: code = "VK_ERROR_INITIALIZATION_FAILED"; break;
			case VK_ERROR_DEVICE_LOST: code = "VK_ERROR_DEVICE_LOST"; break;
			case VK_ERROR_MEMORY_MAP_FAILED: code = "VK_ERROR_MEMORY_MAP_FAILED"; break;
			case VK_ERROR_LAYER_NOT_PRESENT: code = "VK_ERROR_LAYER_NOT_PRESENT"; break;
			case VK_ERROR_EXTENSION_NOT_PRESENT: code = "VK_ERROR_EXTENSION_NOT_PRESENT"; break;
			case VK_ERROR_FEATURE_NOT_PRESENT: code = "VK_ERROR_FEATURE_NOT_PRESENT"; break;
			case VK_ERROR_INCOMPATIBLE_DRIVER: code = "VK_ERROR_INCOMPATIBLE_DRIVER"; break;
			case VK_ERROR_TOO_MANY_OBJECTS : code = "VK_ERROR_TOO_MANY_OBJECTS "; break;
			case VK_ERROR_FORMAT_NOT_SUPPORTED : code = "VK_ERROR_FORMAT_NOT_SUPPORTED "; break;
			case VK_ERROR_FRAGMENTED_POOL : code = "VK_ERROR_FRAGMENTED_POOL "; break;
			case VK_ERROR_OUT_OF_POOL_MEMORY: code = "VK_ERROR_OUT_OF_POOL_MEMORY"; break;
			case VK_ERROR_INVALID_EXTERNAL_HANDLE: code = "VK_ERROR_INVALID_EXTERNAL_HANDLE"; break;
			case VK_ERROR_SURFACE_LOST_KHR: code = "VK_ERROR_SURFACE_LOST_KHR"; break;
			case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: code = "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR"; break;
			case VK_ERROR_OUT_OF_DATE_KHR: code = "VK_ERROR_OUT_OF_DATE_KHR"; break;
			case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: code = "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR"; break;
			case VK_ERROR_VALIDATION_FAILED_EXT: code = "VK_ERROR_VALIDATION_FAILED_EXT"; break;
			case VK_ERROR_INVALID_SHADER_NV: code = "VK_ERROR_INVALID_SHADER_NV"; break;
			case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: code = "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT"; break;
			case VK_ERROR_FRAGMENTATION_EXT: code = "VK_ERROR_FRAGMENTATION_EXT"; break;
			case VK_ERROR_NOT_PERMITTED_EXT: code = "VK_ERROR_NOT_PERMITTED_EXT"; break;
		}
		fprintf_color(Red, stderr, "%s: %s\n", message.c_str(), code);
		throw;
	}
}

inline const char* FormatToString(VkFormat format) {
	switch (format) {
	case VK_FORMAT_UNDEFINED: return "VK_FORMAT_UNDEFINED";
	case VK_FORMAT_R4G4_UNORM_PACK8: return "VK_FORMAT_R4G4_UNORM_PACK8";
	case VK_FORMAT_R4G4B4A4_UNORM_PACK16: return "VK_FORMAT_R4G4B4A4_UNORM_PACK16";
	case VK_FORMAT_B4G4R4A4_UNORM_PACK16: return "VK_FORMAT_B4G4R4A4_UNORM_PACK16";
	case VK_FORMAT_R5G6B5_UNORM_PACK16: return "VK_FORMAT_R5G6B5_UNORM_PACK16";
	case VK_FORMAT_B5G6R5_UNORM_PACK16: return "VK_FORMAT_B5G6R5_UNORM_PACK16";
	case VK_FORMAT_R5G5B5A1_UNORM_PACK16: return "VK_FORMAT_R5G5B5A1_UNORM_PACK16";
	case VK_FORMAT_B5G5R5A1_UNORM_PACK16: return "VK_FORMAT_B5G5R5A1_UNORM_PACK16";
	case VK_FORMAT_A1R5G5B5_UNORM_PACK16: return "VK_FORMAT_A1R5G5B5_UNORM_PACK16";
	case VK_FORMAT_R8_UNORM: return "VK_FORMAT_R8_UNORM";
	case VK_FORMAT_R8_SNORM: return "VK_FORMAT_R8_SNORM";
	case VK_FORMAT_R8_USCALED: return "VK_FORMAT_R8_USCALED";
	case VK_FORMAT_R8_SSCALED: return "VK_FORMAT_R8_SSCALED";
	case VK_FORMAT_R8_UINT: return "VK_FORMAT_R8_UINT";
	case VK_FORMAT_R8_SINT: return "VK_FORMAT_R8_SINT";
	case VK_FORMAT_R8_SRGB: return "VK_FORMAT_R8_SRGB";
	case VK_FORMAT_R8G8_UNORM: return "VK_FORMAT_R8G8_UNORM";
	case VK_FORMAT_R8G8_SNORM: return "VK_FORMAT_R8G8_SNORM";
	case VK_FORMAT_R8G8_USCALED: return "VK_FORMAT_R8G8_USCALED";
	case VK_FORMAT_R8G8_SSCALED: return "VK_FORMAT_R8G8_SSCALED";
	case VK_FORMAT_R8G8_UINT: return "VK_FORMAT_R8G8_UINT";
	case VK_FORMAT_R8G8_SINT: return "VK_FORMAT_R8G8_SINT";
	case VK_FORMAT_R8G8_SRGB: return "VK_FORMAT_R8G8_SRGB";
	case VK_FORMAT_R8G8B8_UNORM: return "VK_FORMAT_R8G8B8_UNORM";
	case VK_FORMAT_R8G8B8_SNORM: return "VK_FORMAT_R8G8B8_SNORM";
	case VK_FORMAT_R8G8B8_USCALED: return "VK_FORMAT_R8G8B8_USCALED";
	case VK_FORMAT_R8G8B8_SSCALED: return "VK_FORMAT_R8G8B8_SSCALED";
	case VK_FORMAT_R8G8B8_UINT: return "VK_FORMAT_R8G8B8_UINT";
	case VK_FORMAT_R8G8B8_SINT: return "VK_FORMAT_R8G8B8_SINT";
	case VK_FORMAT_R8G8B8_SRGB: return "VK_FORMAT_R8G8B8_SRGB";
	case VK_FORMAT_B8G8R8_UNORM: return "VK_FORMAT_B8G8R8_UNORM";
	case VK_FORMAT_B8G8R8_SNORM: return "VK_FORMAT_B8G8R8_SNORM";
	case VK_FORMAT_B8G8R8_USCALED: return "VK_FORMAT_B8G8R8_USCALED";
	case VK_FORMAT_B8G8R8_SSCALED: return "VK_FORMAT_B8G8R8_SSCALED";
	case VK_FORMAT_B8G8R8_UINT: return "VK_FORMAT_B8G8R8_UINT";
	case VK_FORMAT_B8G8R8_SINT: return "VK_FORMAT_B8G8R8_SINT";
	case VK_FORMAT_B8G8R8_SRGB: return "VK_FORMAT_B8G8R8_SRGB";
	case VK_FORMAT_R8G8B8A8_UNORM: return "VK_FORMAT_R8G8B8A8_UNORM";
	case VK_FORMAT_R8G8B8A8_SNORM: return "VK_FORMAT_R8G8B8A8_SNORM";
	case VK_FORMAT_R8G8B8A8_USCALED: return "VK_FORMAT_R8G8B8A8_USCALED";
	case VK_FORMAT_R8G8B8A8_SSCALED: return "VK_FORMAT_R8G8B8A8_SSCALED";
	case VK_FORMAT_R8G8B8A8_UINT: return "VK_FORMAT_R8G8B8A8_UINT";
	case VK_FORMAT_R8G8B8A8_SINT: return "VK_FORMAT_R8G8B8A8_SINT";
	case VK_FORMAT_R8G8B8A8_SRGB: return "VK_FORMAT_R8G8B8A8_SRGB";
	case VK_FORMAT_B8G8R8A8_UNORM: return "VK_FORMAT_B8G8R8A8_UNORM";
	case VK_FORMAT_B8G8R8A8_SNORM: return "VK_FORMAT_B8G8R8A8_SNORM";
	case VK_FORMAT_B8G8R8A8_USCALED: return "VK_FORMAT_B8G8R8A8_USCALED";
	case VK_FORMAT_B8G8R8A8_SSCALED: return "VK_FORMAT_B8G8R8A8_SSCALED";
	case VK_FORMAT_B8G8R8A8_UINT: return "VK_FORMAT_B8G8R8A8_UINT";
	case VK_FORMAT_B8G8R8A8_SINT: return "VK_FORMAT_B8G8R8A8_SINT";
	case VK_FORMAT_B8G8R8A8_SRGB: return "VK_FORMAT_B8G8R8A8_SRGB";
	case VK_FORMAT_A8B8G8R8_UNORM_PACK32: return "VK_FORMAT_A8B8G8R8_UNORM_PACK32";
	case VK_FORMAT_A8B8G8R8_SNORM_PACK32: return "VK_FORMAT_A8B8G8R8_SNORM_PACK32";
	case VK_FORMAT_A8B8G8R8_USCALED_PACK32: return "VK_FORMAT_A8B8G8R8_USCALED_PACK32";
	case VK_FORMAT_A8B8G8R8_SSCALED_PACK32: return "VK_FORMAT_A8B8G8R8_SSCALED_PACK32";
	case VK_FORMAT_A8B8G8R8_UINT_PACK32: return "VK_FORMAT_A8B8G8R8_UINT_PACK32";
	case VK_FORMAT_A8B8G8R8_SINT_PACK32: return "VK_FORMAT_A8B8G8R8_SINT_PACK32";
	case VK_FORMAT_A8B8G8R8_SRGB_PACK32: return "VK_FORMAT_A8B8G8R8_SRGB_PACK32";
	case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return "VK_FORMAT_A2R10G10B10_UNORM_PACK32";
	case VK_FORMAT_A2R10G10B10_SNORM_PACK32: return "VK_FORMAT_A2R10G10B10_SNORM_PACK32";
	case VK_FORMAT_A2R10G10B10_USCALED_PACK32: return "VK_FORMAT_A2R10G10B10_USCALED_PACK32";
	case VK_FORMAT_A2R10G10B10_SSCALED_PACK32: return "VK_FORMAT_A2R10G10B10_SSCALED_PACK32";
	case VK_FORMAT_A2R10G10B10_UINT_PACK32: return "VK_FORMAT_A2R10G10B10_UINT_PACK32";
	case VK_FORMAT_A2R10G10B10_SINT_PACK32: return "VK_FORMAT_A2R10G10B10_SINT_PACK32";
	case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return "VK_FORMAT_A2B10G10R10_UNORM_PACK32";
	case VK_FORMAT_A2B10G10R10_SNORM_PACK32: return "VK_FORMAT_A2B10G10R10_SNORM_PACK32";
	case VK_FORMAT_A2B10G10R10_USCALED_PACK32: return "VK_FORMAT_A2B10G10R10_USCALED_PACK32";
	case VK_FORMAT_A2B10G10R10_SSCALED_PACK32: return "VK_FORMAT_A2B10G10R10_SSCALED_PACK32";
	case VK_FORMAT_A2B10G10R10_UINT_PACK32: return "VK_FORMAT_A2B10G10R10_UINT_PACK32";
	case VK_FORMAT_A2B10G10R10_SINT_PACK32: return "VK_FORMAT_A2B10G10R10_SINT_PACK32";
	case VK_FORMAT_R16_UNORM: return "VK_FORMAT_R16_UNORM";
	case VK_FORMAT_R16_SNORM: return "VK_FORMAT_R16_SNORM";
	case VK_FORMAT_R16_USCALED: return "VK_FORMAT_R16_USCALED";
	case VK_FORMAT_R16_SSCALED: return "VK_FORMAT_R16_SSCALED";
	case VK_FORMAT_R16_UINT: return "VK_FORMAT_R16_UINT";
	case VK_FORMAT_R16_SINT: return "VK_FORMAT_R16_SINT";
	case VK_FORMAT_R16_SFLOAT: return "VK_FORMAT_R16_SFLOAT";
	case VK_FORMAT_R16G16_UNORM: return "VK_FORMAT_R16G16_UNORM";
	case VK_FORMAT_R16G16_SNORM: return "VK_FORMAT_R16G16_SNORM";
	case VK_FORMAT_R16G16_USCALED: return "VK_FORMAT_R16G16_USCALED";
	case VK_FORMAT_R16G16_SSCALED: return "VK_FORMAT_R16G16_SSCALED";
	case VK_FORMAT_R16G16_UINT: return "VK_FORMAT_R16G16_UINT";
	case VK_FORMAT_R16G16_SINT: return "VK_FORMAT_R16G16_SINT";
	case VK_FORMAT_R16G16_SFLOAT: return "VK_FORMAT_R16G16_SFLOAT";
	case VK_FORMAT_R16G16B16_UNORM: return "VK_FORMAT_R16G16B16_UNORM";
	case VK_FORMAT_R16G16B16_SNORM: return "VK_FORMAT_R16G16B16_SNORM";
	case VK_FORMAT_R16G16B16_USCALED: return "VK_FORMAT_R16G16B16_USCALED";
	case VK_FORMAT_R16G16B16_SSCALED: return "VK_FORMAT_R16G16B16_SSCALED";
	case VK_FORMAT_R16G16B16_UINT: return "VK_FORMAT_R16G16B16_UINT";
	case VK_FORMAT_R16G16B16_SINT: return "VK_FORMAT_R16G16B16_SINT";
	case VK_FORMAT_R16G16B16_SFLOAT: return "VK_FORMAT_R16G16B16_SFLOAT";
	case VK_FORMAT_R16G16B16A16_UNORM: return "VK_FORMAT_R16G16B16A16_UNORM";
	case VK_FORMAT_R16G16B16A16_SNORM: return "VK_FORMAT_R16G16B16A16_SNORM";
	case VK_FORMAT_R16G16B16A16_USCALED: return "VK_FORMAT_R16G16B16A16_USCALED";
	case VK_FORMAT_R16G16B16A16_SSCALED: return "VK_FORMAT_R16G16B16A16_SSCALED";
	case VK_FORMAT_R16G16B16A16_UINT: return "VK_FORMAT_R16G16B16A16_UINT";
	case VK_FORMAT_R16G16B16A16_SINT: return "VK_FORMAT_R16G16B16A16_SINT";
	case VK_FORMAT_R16G16B16A16_SFLOAT: return "VK_FORMAT_R16G16B16A16_SFLOAT";
	case VK_FORMAT_R32_UINT: return "VK_FORMAT_R32_UINT";
	case VK_FORMAT_R32_SINT: return "VK_FORMAT_R32_SINT";
	case VK_FORMAT_R32_SFLOAT: return "VK_FORMAT_R32_SFLOAT";
	case VK_FORMAT_R32G32_UINT: return "VK_FORMAT_R32G32_UINT";
	case VK_FORMAT_R32G32_SINT: return "VK_FORMAT_R32G32_SINT";
	case VK_FORMAT_R32G32_SFLOAT: return "VK_FORMAT_R32G32_SFLOAT";
	case VK_FORMAT_R32G32B32_UINT: return "VK_FORMAT_R32G32B32_UINT";
	case VK_FORMAT_R32G32B32_SINT: return "VK_FORMAT_R32G32B32_SINT";
	case VK_FORMAT_R32G32B32_SFLOAT: return "VK_FORMAT_R32G32B32_SFLOAT";
	case VK_FORMAT_R32G32B32A32_UINT: return "VK_FORMAT_R32G32B32A32_UINT";
	case VK_FORMAT_R32G32B32A32_SINT: return "VK_FORMAT_R32G32B32A32_SINT";
	case VK_FORMAT_R32G32B32A32_SFLOAT: return "VK_FORMAT_R32G32B32A32_SFLOAT";
	case VK_FORMAT_R64_UINT: return "VK_FORMAT_R64_UINT";
	case VK_FORMAT_R64_SINT: return "VK_FORMAT_R64_SINT";
	case VK_FORMAT_R64_SFLOAT: return "VK_FORMAT_R64_SFLOAT";
	case VK_FORMAT_R64G64_UINT: return "VK_FORMAT_R64G64_UINT";
	case VK_FORMAT_R64G64_SINT: return "VK_FORMAT_R64G64_SINT";
	case VK_FORMAT_R64G64_SFLOAT: return "VK_FORMAT_R64G64_SFLOAT";
	case VK_FORMAT_R64G64B64_UINT: return "VK_FORMAT_R64G64B64_UINT";
	case VK_FORMAT_R64G64B64_SINT: return "VK_FORMAT_R64G64B64_SINT";
	case VK_FORMAT_R64G64B64_SFLOAT: return "VK_FORMAT_R64G64B64_SFLOAT";
	case VK_FORMAT_R64G64B64A64_UINT: return "VK_FORMAT_R64G64B64A64_UINT";
	case VK_FORMAT_R64G64B64A64_SINT: return "VK_FORMAT_R64G64B64A64_SINT";
	case VK_FORMAT_R64G64B64A64_SFLOAT: return "VK_FORMAT_R64G64B64A64_SFLOAT";
	case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return "VK_FORMAT_B10G11R11_UFLOAT_PACK32";
	case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: return "VK_FORMAT_E5B9G9R9_UFLOAT_PACK32";
	case VK_FORMAT_D16_UNORM: return "VK_FORMAT_D16_UNORM";
	case VK_FORMAT_X8_D24_UNORM_PACK32: return "VK_FORMAT_X8_D24_UNORM_PACK32";
	case VK_FORMAT_D32_SFLOAT: return "VK_FORMAT_D32_SFLOAT";
	case VK_FORMAT_S8_UINT: return "VK_FORMAT_S8_UINT";
	case VK_FORMAT_D16_UNORM_S8_UINT: return "VK_FORMAT_D16_UNORM_S8_UINT";
	case VK_FORMAT_D24_UNORM_S8_UINT: return "VK_FORMAT_D24_UNORM_S8_UINT";
	case VK_FORMAT_D32_SFLOAT_S8_UINT: return "VK_FORMAT_D32_SFLOAT_S8_UINT";
	case VK_FORMAT_BC1_RGB_UNORM_BLOCK: return "VK_FORMAT_BC1_RGB_UNORM_BLOCK";
	case VK_FORMAT_BC1_RGB_SRGB_BLOCK: return "VK_FORMAT_BC1_RGB_SRGB_BLOCK";
	case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return "VK_FORMAT_BC1_RGBA_UNORM_BLOCK";
	case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return "VK_FORMAT_BC1_RGBA_SRGB_BLOCK";
	case VK_FORMAT_BC2_UNORM_BLOCK: return "VK_FORMAT_BC2_UNORM_BLOCK";
	case VK_FORMAT_BC2_SRGB_BLOCK: return "VK_FORMAT_BC2_SRGB_BLOCK";
	case VK_FORMAT_BC3_UNORM_BLOCK: return "VK_FORMAT_BC3_UNORM_BLOCK";
	case VK_FORMAT_BC3_SRGB_BLOCK: return "VK_FORMAT_BC3_SRGB_BLOCK";
	case VK_FORMAT_BC4_UNORM_BLOCK: return "VK_FORMAT_BC4_UNORM_BLOCK";
	case VK_FORMAT_BC4_SNORM_BLOCK: return "VK_FORMAT_BC4_SNORM_BLOCK";
	case VK_FORMAT_BC5_UNORM_BLOCK: return "VK_FORMAT_BC5_UNORM_BLOCK";
	case VK_FORMAT_BC5_SNORM_BLOCK: return "VK_FORMAT_BC5_SNORM_BLOCK";
	case VK_FORMAT_BC6H_UFLOAT_BLOCK: return "VK_FORMAT_BC6H_UFLOAT_BLOCK";
	case VK_FORMAT_BC6H_SFLOAT_BLOCK: return "VK_FORMAT_BC6H_SFLOAT_BLOCK";
	case VK_FORMAT_BC7_UNORM_BLOCK: return "VK_FORMAT_BC7_UNORM_BLOCK";
	case VK_FORMAT_BC7_SRGB_BLOCK: return "VK_FORMAT_BC7_SRGB_BLOCK";
	case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK: return "VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK";
	case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK: return "VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK";
	case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK: return "VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK";
	case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK: return "VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK";
	case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK: return "VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK";
	case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK: return "VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK";
	case VK_FORMAT_EAC_R11_UNORM_BLOCK: return "VK_FORMAT_EAC_R11_UNORM_BLOCK";
	case VK_FORMAT_EAC_R11_SNORM_BLOCK: return "VK_FORMAT_EAC_R11_SNORM_BLOCK";
	case VK_FORMAT_EAC_R11G11_UNORM_BLOCK: return "VK_FORMAT_EAC_R11G11_UNORM_BLOCK";
	case VK_FORMAT_EAC_R11G11_SNORM_BLOCK: return "VK_FORMAT_EAC_R11G11_SNORM_BLOCK";
	case VK_FORMAT_ASTC_4x4_UNORM_BLOCK: return "VK_FORMAT_ASTC_4x4_UNORM_BLOCK";
	case VK_FORMAT_ASTC_4x4_SRGB_BLOCK: return "VK_FORMAT_ASTC_4x4_SRGB_BLOCK";
	case VK_FORMAT_ASTC_5x4_UNORM_BLOCK: return "VK_FORMAT_ASTC_5x4_UNORM_BLOCK";
	case VK_FORMAT_ASTC_5x4_SRGB_BLOCK: return "VK_FORMAT_ASTC_5x4_SRGB_BLOCK";
	case VK_FORMAT_ASTC_5x5_UNORM_BLOCK: return "VK_FORMAT_ASTC_5x5_UNORM_BLOCK";
	case VK_FORMAT_ASTC_5x5_SRGB_BLOCK: return "VK_FORMAT_ASTC_5x5_SRGB_BLOCK";
	case VK_FORMAT_ASTC_6x5_UNORM_BLOCK: return "VK_FORMAT_ASTC_6x5_UNORM_BLOCK";
	case VK_FORMAT_ASTC_6x5_SRGB_BLOCK: return "VK_FORMAT_ASTC_6x5_SRGB_BLOCK";
	case VK_FORMAT_ASTC_6x6_UNORM_BLOCK: return "VK_FORMAT_ASTC_6x6_UNORM_BLOCK";
	case VK_FORMAT_ASTC_6x6_SRGB_BLOCK: return "VK_FORMAT_ASTC_6x6_SRGB_BLOCK";
	case VK_FORMAT_ASTC_8x5_UNORM_BLOCK: return "VK_FORMAT_ASTC_8x5_UNORM_BLOCK";
	case VK_FORMAT_ASTC_8x5_SRGB_BLOCK: return "VK_FORMAT_ASTC_8x5_SRGB_BLOCK";
	case VK_FORMAT_ASTC_8x6_UNORM_BLOCK: return "VK_FORMAT_ASTC_8x6_UNORM_BLOCK";
	case VK_FORMAT_ASTC_8x6_SRGB_BLOCK: return "VK_FORMAT_ASTC_8x6_SRGB_BLOCK";
	case VK_FORMAT_ASTC_8x8_UNORM_BLOCK: return "VK_FORMAT_ASTC_8x8_UNORM_BLOCK";
	case VK_FORMAT_ASTC_8x8_SRGB_BLOCK: return "VK_FORMAT_ASTC_8x8_SRGB_BLOCK";
	case VK_FORMAT_ASTC_10x5_UNORM_BLOCK: return "VK_FORMAT_ASTC_10x5_UNORM_BLOCK";
	case VK_FORMAT_ASTC_10x5_SRGB_BLOCK: return "VK_FORMAT_ASTC_10x5_SRGB_BLOCK";
	case VK_FORMAT_ASTC_10x6_UNORM_BLOCK: return "VK_FORMAT_ASTC_10x6_UNORM_BLOCK";
	case VK_FORMAT_ASTC_10x6_SRGB_BLOCK: return "VK_FORMAT_ASTC_10x6_SRGB_BLOCK";
	case VK_FORMAT_ASTC_10x8_UNORM_BLOCK: return "VK_FORMAT_ASTC_10x8_UNORM_BLOCK";
	case VK_FORMAT_ASTC_10x8_SRGB_BLOCK: return "VK_FORMAT_ASTC_10x8_SRGB_BLOCK";
	case VK_FORMAT_ASTC_10x10_UNORM_BLOCK: return "VK_FORMAT_ASTC_10x10_UNORM_BLOCK";
	case VK_FORMAT_ASTC_10x10_SRGB_BLOCK: return "VK_FORMAT_ASTC_10x10_SRGB_BLOCK";
	case VK_FORMAT_ASTC_12x10_UNORM_BLOCK: return "VK_FORMAT_ASTC_12x10_UNORM_BLOCK";
	case VK_FORMAT_ASTC_12x10_SRGB_BLOCK: return "VK_FORMAT_ASTC_12x10_SRGB_BLOCK";
	case VK_FORMAT_ASTC_12x12_UNORM_BLOCK: return "VK_FORMAT_ASTC_12x12_UNORM_BLOCK";
	case VK_FORMAT_ASTC_12x12_SRGB_BLOCK: return "VK_FORMAT_ASTC_12x12_SRGB_BLOCK";
	case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG: return "VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG";
	case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG: return "VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG";
	case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG: return "VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG";
	case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG: return "VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG";
	case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG: return "VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG";
	case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG: return "VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG";
	case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG: return "VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG";
	case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG: return "VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG";
	}
	return "";
}
inline const char* TopologyToString(VkPrimitiveTopology topology) {
	switch (topology) {
    case VK_PRIMITIVE_TOPOLOGY_POINT_LIST: return "VK_PRIMITIVE_TOPOLOGY_POINT_LIST";                        
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST: return "VK_PRIMITIVE_TOPOLOGY_LINE_LIST";
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP: return "VK_PRIMITIVE_TOPOLOGY_LINE_STRIP";
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: return "VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST";
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: return "VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP";
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN: return "VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN";
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY: return "VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY";
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY: return "VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY";
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY: return "VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY";
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY: return "VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY";
    case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST: return "VK_PRIMITIVE_TOPOLOGY_PATCH_LIST";
	}
	return "";
}