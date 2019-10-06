#pragma once

#include <memory>

#include <Content/Material.hpp>
#include <Core/EnginePlugin.hpp>
#include <Core/DeviceManager.hpp>
#include <Input/InputManager.hpp>
#include <Scene/Camera.hpp>
#include <Scene/Object.hpp>
#include <Scene/Renderer.hpp>
#include <Util/Util.hpp>

class VkCAVE;
class EnginePlugin;

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

	template<class T>
	inline T* GetPlugin() {
		for (EnginePlugin* p : mPlugins)
			if (T* t = dynamic_cast<T*>(p))
				return t;
		return nullptr;
	}

private:
	friend class VkCAVE;
	ENGINE_EXPORT Scene(::DeviceManager* deviceManager, ::InputManager* iputManager);


	::InputManager* mInputManager;
	::DeviceManager* mDeviceManager;
	std::vector<std::shared_ptr<Object>> mObjects;
	std::vector<Camera*> mCameras;
	std::vector<Renderer*> mRenderers;
	std::vector<EnginePlugin*> mPlugins;
};