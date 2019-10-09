#pragma once

#include <memory>

#include <Content/AssetManager.hpp>
#include <Content/Material.hpp>
#include <Core/DeviceManager.hpp>
#include <Core/PluginManager.hpp>
#include <Input/InputManager.hpp>
#include <Scene/Camera.hpp>
#include <Scene/Light.hpp>
#include <Scene/Object.hpp>
#include <Scene/Renderer.hpp>
#include <Util/Util.hpp>

class VkCAVE;
class PluginManager;

/*
Holds scene Objects. In general, plugins will add objects during their lifetime,
and remove objects during or at the end of their lifetime.
This makes the shared_ptr destroy when the plugin removes the object, allowing the plugin's module
to free the memory.
*/
class Scene {
public:
	ENGINE_EXPORT ~Scene();
	ENGINE_EXPORT void AddObject(std::shared_ptr<Object> object);
	ENGINE_EXPORT void RemoveObject(Object* object);

	ENGINE_EXPORT void Update(const FrameTime& frameTime);
	ENGINE_EXPORT void Render(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, Material* materialOverride = nullptr);

	inline Buffer* LightBuffer(Device* device, uint32_t backBufferIndex) const { return mLightBuffers.at(device)[backBufferIndex]; }
	inline const std::vector<Light*>& Lights() const { return mLights; }
	inline const std::vector<Camera*>& Cameras() const { return mCameras; }

	inline ::AssetManager* AssetManager() const { return mAssetManager; }
	inline ::DeviceManager* DeviceManager() const { return mDeviceManager; }
	inline ::InputManager* InputManager() const { return mInputManager; }
	inline ::PluginManager* PluginManager() const { return mPluginManager; }

private:
	friend class VkCAVE;
	ENGINE_EXPORT Scene(::DeviceManager* deviceManager, ::AssetManager* assetManager, ::InputManager* iputManager, ::PluginManager* pluginManager);

	std::unordered_map<Device*, Buffer**> mLightBuffers;

	::AssetManager* mAssetManager;
	::InputManager* mInputManager;
	::DeviceManager* mDeviceManager;
	::PluginManager* mPluginManager;
	std::vector<std::shared_ptr<Object>> mObjects;
	std::vector<Light*> mLights;
	std::vector<Camera*> mCameras;
	std::vector<Renderer*> mRenderers;
};