#pragma once

#include <Content/Texture.hpp>

Texture* LoadDicomStack(const std::string& folder, Device* device, float3* size);