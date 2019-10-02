#include <Content/AssetDatabase.hpp>
#include <Content/Font.hpp>
#include <Content/Mesh.hpp>
#include <Content/Texture.hpp>
#include <Content/Shader.hpp>

using namespace std;

AssetDatabase::AssetDatabase(DeviceManager* deviceManager) : mDeviceManager(deviceManager) {}
AssetDatabase::~AssetDatabase() {
	for (auto& asset : mAssets)
		delete asset.second;
}

Shader* AssetDatabase::LoadShader(const std::string& filename) {
	Asset*& asset = mAssets[filename];
	if (!asset) asset = new Shader(filename, mDeviceManager, filename);
	return (Shader*)asset;
}
Texture* AssetDatabase::LoadTexture(const std::string& filename, bool srgb) {
	Asset*& asset = mAssets[filename];
	if (!asset) asset = new Texture(filename, mDeviceManager, filename, srgb);
	return (Texture*)asset;
}
Mesh* AssetDatabase::LoadMesh(const std::string& filename, float scale) {
	Asset*& asset = mAssets[filename];
	if (!asset) asset = new Mesh(filename, mDeviceManager, filename, scale);
	return (Mesh*)asset;
}
Font* AssetDatabase::LoadFont(const std::string& filename, float pixelSize, float scale) {
	Asset*& asset = mAssets[filename];
	if (!asset) asset = new Font(filename, mDeviceManager, filename, pixelSize, scale);
	return (Font*)asset;
}