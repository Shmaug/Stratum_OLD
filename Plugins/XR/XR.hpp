#pragma once

#include <Core/EnginePlugin.hpp>
#include <Scene/Scene.hpp>
#include <Input/MouseKeyboardInput.hpp>
#include <Util/Profiler.hpp>

#include <openxr/openxr.h>

class XR : public EnginePlugin {
private:
	Scene* mScene;
	std::vector<Object*> mObjects;

	XrInstance mInstance;

public:
	PLUGIN_EXPORT XR();
	PLUGIN_EXPORT ~XR();

	PLUGIN_EXPORT bool Init(Scene* scene) override;
	PLUGIN_EXPORT void Update(CommandBuffer* commandBuffer) override;
	PLUGIN_EXPORT void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) override;

	inline int Priority() override { return 1000; }
};