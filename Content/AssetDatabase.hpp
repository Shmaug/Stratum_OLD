#pragma once

#include <unordered_map>

#include <Util/Util.hpp>
#include <Content/Asset.hpp>

class Font;
class Mesh;
class Shader;
class Texture;
class DeviceManager;

class AssetDatabase {
public:
	ENGINE_EXPORT ~AssetDatabase();

	ENGINE_EXPORT Shader*	LoadShader	(const std::string& filename);
	ENGINE_EXPORT Texture*	LoadTexture	(const std::string& filename, bool srgb = true);
	ENGINE_EXPORT Mesh*		LoadMesh	(const std::string& filename, float scale = 1.f);
	ENGINE_EXPORT Font*		LoadFont	(const std::string& filename, float pixelSize, float scale);

private:
	friend class DeviceManager;
	ENGINE_EXPORT AssetDatabase(DeviceManager* deviceManager);

	DeviceManager* mDeviceManager;
	std::unordered_map<std::string, Asset*> mAssets;
};