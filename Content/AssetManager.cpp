#include <Content/AssetManager.hpp>
#include <Content/Font.hpp>
#include <Content/Mesh.hpp>
#include <Content/Texture.hpp>
#include <Content/Shader.hpp>

using namespace std;

AssetManager::AssetManager(DeviceManager* deviceManager) : mDeviceManager(deviceManager) {}
AssetManager::~AssetManager() {
	for (auto& asset : mAssets)
		delete asset.second;
}

Shader* AssetManager::LoadShader(const std::string& filename) {
	Asset*& asset = mAssets[filename];
	if (!asset) asset = new Shader(filename, mDeviceManager, filename);
	return (Shader*)asset;
}
Texture* AssetManager::LoadTexture(const std::string& filename, bool srgb) {
	Asset*& asset = mAssets[filename];
	if (!asset) asset = new Texture(filename, mDeviceManager, filename, srgb);
	return (Texture*)asset;
}
Mesh* AssetManager::LoadMesh(const std::string& filename, float scale) {
	Asset*& asset = mAssets[filename];
	if (!asset) asset = new Mesh(filename, mDeviceManager, filename, scale);
	return (Mesh*)asset;
}
Font* AssetManager::LoadFont(const std::string& filename, uint32_t pixelHeight) {
	Asset*& asset = mAssets[filename + to_string(pixelHeight)];
	if (!asset) asset = new Font(filename, mDeviceManager, filename, (float)pixelHeight, 1.f / pixelHeight);
	return (Font*)asset;
}