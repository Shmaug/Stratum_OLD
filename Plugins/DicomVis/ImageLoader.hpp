#pragma once

#include <Content/Texture.hpp>

enum ImageStackType {
	IMAGE_STACK_NONE,
	IMAGE_STACK_DICOM,
	IMAGE_STACK_RAW,
	IMAGE_STACK_STANDARD,
};

class ImageLoader {
public:
	PLUGIN_EXPORT static ImageStackType FolderStackType(const fs::path& folder);
	// Load a stack of RAW images
	// Load a stack of normal images (png, jpg, tiff, etc..)
	// Items are sorted in order of name
	PLUGIN_EXPORT static Texture* LoadStandardStack(const fs::path& folder, Device* device, float3* size);
	// Load a stack of RAW images
	// PLUGIN_EXPORT static Texture* LoadRawStack(const std::string& folder, Device* device, float3* size);
	PLUGIN_EXPORT static Texture* LoadDicomStack(const fs::path& folder, Device* device, float3* size);
	// Load a stack of raw, uncompressed images
	// Items are sorted in order of name
	PLUGIN_EXPORT static Texture* LoadRawStack(const fs::path& folder, Device* device, float3* size);
};