#include <Content/AssetManager.hpp>
#include <Content/Font.hpp>
#include <Content/Mesh.hpp>
#include <Content/Texture.hpp>
#include <Content/Shader.hpp>

using namespace std;

AssetManager::AssetManager(Instance* instance) : mInstance(instance) {}
AssetManager::~AssetManager() {
	for (auto& asset : mAssets)
		delete asset.second;
}

Shader* AssetManager::LoadShader(const string& filename) {
	Asset*& asset = mAssets[filename];
	if (!asset) asset = new Shader(filename, mInstance, filename);
	return (Shader*)asset;
}
Texture* AssetManager::LoadTexture(const string& filename, bool srgb) {
	Asset*& asset = mAssets[filename];
	if (!asset) asset = new Texture(filename, mInstance, filename, srgb);
	return (Texture*)asset;
}
Texture* AssetManager::LoadCubemap(const string& posx, const string& negx, const string& posy, const string& negy, const string& posz, const string& negz, bool srgb) {
	Asset*& asset = mAssets[negx + posx + negy + posy + negz + posz];
	if (!asset) asset = new Texture(negx + " Cube", mInstance, posx, negx, posy, negy, posz, negz, srgb);
	return (Texture*)asset;
}
Mesh* AssetManager::LoadMesh(const string& filename, float scale) {
	Asset*& asset = mAssets[filename];
	if (!asset) asset = new Mesh(filename, mInstance, filename, scale);
	return (Mesh*)asset;
}
Font* AssetManager::LoadFont(const string& filename, uint32_t pixelHeight) {
	Asset*& asset = mAssets[filename + to_string(pixelHeight)];
	if (!asset) asset = new Font(filename, mInstance, filename, (float)pixelHeight, 1.f / pixelHeight);
	return (Font*)asset;
}