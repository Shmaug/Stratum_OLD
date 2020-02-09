#pragma once

#include <Content/Texture.hpp>

class Dicom {
public:
	PLUGIN_EXPORT static Texture* LoadDicomStack(const std::string& folder, Device* device, float3* size);
};