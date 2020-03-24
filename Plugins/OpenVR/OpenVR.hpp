#pragma once

#include <Core/EnginePlugin.hpp>
#include <Scene/Scene.hpp>
#include <Input/MouseKeyboardInput.hpp>
#include <Util/Profiler.hpp>

#include "OpenVRDevice.hpp"

class OpenVR : public EnginePlugin {
private:
	Scene* mScene;
	Camera* mCamera;
	Camera* mCameraRight;
	Object* mBodyBase;
	Object* mHead;
	OpenVRDevice* mVRDevice;

	std::vector<Object*> mObjects;
	uint64_t mFrameNum;

	//Texture* mLeftEye;
	//Texture* mRightEye;
	Texture* mMirror;

public:
	PLUGIN_EXPORT OpenVR();
	PLUGIN_EXPORT ~OpenVR();

	PLUGIN_EXPORT void PreInstanceInit(Instance* instance) override;
	PLUGIN_EXPORT void PreDeviceInit(Instance* instance, VkPhysicalDevice device) override;
	PLUGIN_EXPORT bool Init(Scene* scene) override;
	PLUGIN_EXPORT void Update(CommandBuffer* commandBuffer) override;
	//PLUGIN_EXPORT void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) override;
	PLUGIN_EXPORT void PreRender(CommandBuffer* commandBuffer, Camera* camera, PassType pass) override;
	//PLUGIN_EXPORT void PostRenderScene(CommandBuffer* commandBuffer, Camera* camera, PassType pass) override;
	PLUGIN_EXPORT void PostProcess(CommandBuffer* commandBuffer, Camera* camera) override;
	PLUGIN_EXPORT void PrePresent() override;

	inline int Priority() override { return 1000; }
};