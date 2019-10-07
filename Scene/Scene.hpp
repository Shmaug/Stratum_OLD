#pragma once

#include <memory>

#include <Content/Material.hpp>
#include <Core/DeviceManager.hpp>
#include <Core/PluginManager.hpp>
#include <Input/InputManager.hpp>
#include <Scene/Camera.hpp>
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

	inline const std::vector<Camera*>& Cameras() const { return mCameras; }

	inline ::DeviceManager* DeviceManager() const { return mDeviceManager; }
	inline ::InputManager* InputManager() const { return mInputManager; }
	inline ::PluginManager* PluginManager() const { return mPluginManager; }

private:
	friend class VkCAVE;
	ENGINE_EXPORT Scene(::DeviceManager* deviceManager, ::InputManager* iputManager, ::PluginManager* pluginManager);

	::InputManager* mInputManager;
	::DeviceManager* mDeviceManager;
	::PluginManager* mPluginManager;
	std::vector<std::shared_ptr<Object>> mObjects;
	std::vector<Camera*> mCameras;
	std::vector<Renderer*> mRenderers;
};