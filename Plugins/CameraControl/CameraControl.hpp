#pragma once

#include <Core/EnginePlugin.hpp>
#include <Scene/TextRenderer.hpp>
#include <Input/MouseKeyboardInput.hpp>

class CameraControl : public EnginePlugin {
public:
	PLUGIN_EXPORT CameraControl();
	PLUGIN_EXPORT ~CameraControl();

	PLUGIN_EXPORT bool Init(Scene* scene) override;
	PLUGIN_EXPORT void Update(const FrameTime& frameTime) override;

	inline void CameraDistance(float d) { mCameraDistance = d; }
	inline float CameraDistance() const { return mCameraDistance; }
	inline Object* CameraPivot() const { return mCameraPivot.get(); }

private:
	Scene* mScene;
	float mCameraDistance;
	float3 mCameraEuler;

	MouseKeyboardInput* mInput;

	std::shared_ptr<TextRenderer> mFpsText;
	std::shared_ptr<Object> mCameraPivot;

	float mFrameTimeAccum;
	float mFps;
	uint32_t mFrameCount;
};