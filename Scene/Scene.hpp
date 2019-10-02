#pragma once

#include <memory>

#include <Content/Material.hpp>
#include <Scene/Camera.hpp>
#include <Scene/Object.hpp>
#include <Scene/Renderer.hpp>
#include <Util/Util.hpp>

class VkCAVE;
class DeviceManager;

class Scene {
public:
	ENGINE_EXPORT void AddObject(Object* object);
	ENGINE_EXPORT void RemoveObject(Object* object);

	ENGINE_EXPORT void Render(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, Material* materialOverride = nullptr);

	inline const std::vector<Camera*>& Cameras() const { return mCameras; }

	inline ::DeviceManager* DeviceManager() const { return mDeviceManager; }

private:
	friend class VkCAVE;
	ENGINE_EXPORT Scene(::DeviceManager* deviceManager);

	ENGINE_EXPORT void Clear();

	::DeviceManager* mDeviceManager;
	std::vector<Object*> mObjects;
	std::vector<Camera*> mCameras;
	std::vector<Renderer*> mRenderers;
};