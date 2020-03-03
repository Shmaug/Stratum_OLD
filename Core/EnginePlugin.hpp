#pragma once

#include <Core/CommandBuffer.hpp>
#include <Util/Util.hpp>

class Scene;
class Camera;
class Instance;

class EnginePlugin {
public:
	bool mEnabled;

	inline virtual ~EnginePlugin() {}
	
	inline virtual void PreInstanceInit(Instance* instance) {};
	inline virtual void PreDeviceInit(Instance* instance, VkPhysicalDevice physicalDevice) {};
	
	inline virtual bool Init(Scene* scene) { return true; }

	inline virtual void PreUpdate(CommandBuffer* commandBuffer) {}
	inline virtual void FixedUpdate(CommandBuffer* commandBuffer) {}
	inline virtual void Update(CommandBuffer* commandBuffer) {}
	inline virtual void PostUpdate(CommandBuffer* commandBuffer) {}
	
	/// Called before a camera starts rendering, before BeginRenderPass
	inline virtual void PreRender(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {}
	/// Called before a camera starts rendering the scene, after BeginRenderPass
	inline virtual void PreRenderScene(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {}
	/// Called after a camera finishes rendering the scene, before EndRenderPass
	inline virtual void PostRenderScene(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {}
	/// Called before a camera presents to a window, but after the camera resolves to Camera::ResolveBuffer. Good for post-processing
	inline virtual void PostProcess(CommandBuffer* commandBuffer, Camera* camera) {}

	inline virtual void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {}

	inline virtual void PrePresent() {}
	
	/// Higher priority plugins get called first
	inline virtual int Priority() { return 50; }
};

#define ENGINE_PLUGIN(plugin) extern "C" { PLUGIN_EXPORT EnginePlugin* CreatePlugin() { return new plugin(); } }