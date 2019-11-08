#pragma once

#include <memory>

#include <Core/CommandBuffer.hpp>
#include <Core/DeviceManager.hpp>
#include <Util/Util.hpp>

class Camera;
class CommandBuffer;
class DeviceManager;
class Scene;

class EnginePlugin {
public:
	bool mEnabled;

	inline virtual ~EnginePlugin() {}
	
	inline virtual bool Init(Scene* scene) { return true; }
	
	inline virtual void PreUpdate (const FrameTime& frameTime) {}
	inline virtual void Update	  (const FrameTime& frameTime) {}
	inline virtual void PostUpdate(const FrameTime& frameTime) {}
	
	inline virtual void PreRender (CommandBuffer* commandBuffer, uint32_t backBufferIndex, Camera* camera) {}
	inline virtual void DrawGizmos(CommandBuffer* commandBuffer, uint32_t backBufferIndex, Camera* camera) {}
	inline virtual void PostRender(CommandBuffer* commandBuffer, uint32_t backBufferIndex, Camera* camera) {}
	
	// Higher priority plugins get called first
	inline virtual int Priority() { return 50; }
};

#define ENGINE_PLUGIN(plugin) extern "C" { PLUGIN_EXPORT EnginePlugin* CreatePlugin() { return new plugin(); } }