#pragma once

#include <unordered_map>

#include <Util/Util.hpp>
#include <Content/Asset.hpp>

class Font;
class Mesh;
class Shader;
class Texture;
class Instance;

class AssetManager {
public:
	ENGINE_EXPORT ~AssetManager();

	ENGINE_EXPORT Shader*	LoadShader	(const std::string& filename);
	ENGINE_EXPORT Texture*	LoadTexture	(const std::string& filename, bool srgb = true);
	ENGINE_EXPORT Mesh*		LoadMesh	(const std::string& filename, float scale = 1.f);
	ENGINE_EXPORT Font*		LoadFont	(const std::string& filename, uint32_t pixelHeight);

private:
	friend class VkCAVE;
	ENGINE_EXPORT AssetManager(Instance* instance);

	Instance* mInstance;
	std::unordered_map<std::string, Asset*> mAssets;
};