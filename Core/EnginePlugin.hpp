#pragma once

#include <Core/Device.hpp>
#include <Core/Instance.hpp>
#include <Util/Util.hpp>

class Camera;
class Instance;
class Scene;

class EnginePlugin {
public:
	bool mEnabled;

	inline virtual ~EnginePlugin() {}
	
	inline virtual bool Init(Scene* scene) { return true; }
	
	inline virtual void PreUpdate () {}
	inline virtual void Update	  () {}
	inline virtual void PostUpdate() {}
	
	inline virtual void PreRender (CommandBuffer* commandBuffer, Camera* camera) {}
	inline virtual void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {}
	inline virtual void PostRender(CommandBuffer* commandBuffer, Camera* camera) {}
	
	// Higher priority plugins get called first
	inline virtual int Priority() { return 50; }
};

#define ENGINE_PLUGIN(plugin) extern "C" { PLUGIN_EXPORT EnginePlugin* CreatePlugin() { return new plugin(); } }