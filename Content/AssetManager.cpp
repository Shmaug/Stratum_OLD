#include <Content/AssetManager.hpp>
#include <Content/Font.hpp>
#include <Content/Mesh.hpp>
#include <Content/Texture.hpp>
#include <Content/Shader.hpp>

using namespace std;

AssetManager::AssetManager(Device* device) : mDevice(device) {}
AssetManager::~AssetManager() {
	for (auto& asset : mAssets)
		delete asset.second;
}

Shader* AssetManager::LoadShader(const string& filename) {
	Asset*& asset = mAssets[filename];
	if (!asset) asset = new Shader(filename, mDevice, filename);
	return (Shader*)asset;
}
Texture* AssetManager::LoadTexture(const string& filename, bool srgb) {
	Asset*& asset = mAssets[filename];
	if (!asset) asset = new Texture(filename, mDevice, filename, srgb);
	return (Texture*)asset;
}
Texture* AssetManager::LoadCubemap(const string& posx, const string& negx, const string& posy, const string& negy, const string& posz, const string& negz, bool srgb) {
	Asset*& asset = mAssets[negx + posx + negy + posy + negz + posz];
	if (!asset) asset = new Texture(negx + " Cube", mDevice, posx, negx, posy, negy, posz, negz, srgb);
	return (Texture*)asset;
}
Mesh* AssetManager::LoadMesh(const string& filename, float scale) {
	Asset*& asset = mAssets[filename];
	if (!asset) asset = new Mesh(filename, mDevice, filename, scale);
	return (Mesh*)asset;
}
Font* AssetManager::LoadFont(const string& filename, uint32_t pixelHeight) {
	Asset*& asset = mAssets[filename + to_string(pixelHeight)];
	if (!asset) asset = new Font(filename, mDevice, filename, (float)pixelHeight, 1.f / pixelHeight);
	return (Font*)asset;
}