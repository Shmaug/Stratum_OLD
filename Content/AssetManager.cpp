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
	mMutex.lock();
	Asset*& asset = mAssets[filename];
	if (!asset) asset = new Shader(filename, mDevice, filename);
	mMutex.unlock();
	return (Shader*)asset;
}
Texture* AssetManager::LoadTexture(const string& filename, bool srgb) {
	mMutex.lock();
	Asset*& asset = mAssets[filename];
	if (!asset) asset = new Texture(filename, mDevice, filename, srgb);
	mMutex.unlock();
	return (Texture*)asset;
}
Texture* AssetManager::LoadCubemap(const string& posx, const string& negx, const string& posy, const string& negy, const string& posz, const string& negz, bool srgb) {
	mMutex.lock();
	Asset*& asset = mAssets[negx + posx + negy + posy + negz + posz];
	if (!asset) asset = new Texture(negx + " Cube", mDevice, posx, negx, posy, negy, posz, negz, srgb);
	mMutex.unlock();
	return (Texture*)asset;
}
Mesh* AssetManager::LoadMesh(const string& filename, float scale) {
	mMutex.lock();
	Asset*& asset = mAssets[filename];
	if (!asset) asset = new Mesh(filename, mDevice, filename, scale);
	mMutex.unlock();
	return (Mesh*)asset;
}
Font* AssetManager::LoadFont(const string& filename, uint32_t pixelHeight) {
	mMutex.lock();
	Asset*& asset = mAssets[filename + to_string(pixelHeight)];
	if (!asset) asset = new Font(filename, mDevice, filename, (float)pixelHeight, 1.f / pixelHeight);
	mMutex.unlock();
	return (Font*)asset;
}