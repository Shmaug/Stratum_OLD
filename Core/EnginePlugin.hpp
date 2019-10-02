#include <memory>

#include <Core/CommandBuffer.hpp>
#include <Core/DeviceManager.hpp>
#include <Scene/Scene.hpp>
#include <Util/Util.hpp>

#ifdef WINDOWS
class __declspec(dllexport) EnginePlugin {
#else
static_assert(false, "Not implemented!");
#endif
public:
	inline virtual ~EnginePlugin() {}

	inline virtual bool Init(Scene* scene, DeviceManager* deviceManager) { return true; }

	inline virtual void PreUpdate (const FrameTime& frameTime) {}
	inline virtual void Update	  (const FrameTime& frameTime) {}
	inline virtual void PostUpdate(const FrameTime& frameTime) {}

	inline virtual void PreRender (const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex) {}
	inline virtual void PostRender(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex) {}

	inline virtual int Priority() { return 50; }
};

#define ENGINE_PLUGIN(plugin) extern "C" { PLUGIN_EXPORT EnginePlugin* CreatePlugin() { return new plugin(); } }