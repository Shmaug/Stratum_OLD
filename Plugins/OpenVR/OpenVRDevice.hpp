#pragma once

#include <Util/Profiler.hpp>
#include <Math/Math.hpp>
#include <Content/Texture.hpp>
#include <openvr.h>


class OpenVRDevice {
public:
	PLUGIN_EXPORT OpenVRDevice(float near = .01f, float far = 1024.f);
	PLUGIN_EXPORT ~OpenVRDevice();

	typedef struct _ControllerData
	{
		// Fields to be initialzed by iterateAssignIds() and setHands()
		int deviceID = -1;  // Device ID according to the SteamVR system
		int hand = -1;      // 0=invalid 1=left 2=right
		int idtrigger = -1; // Trigger axis id
		int idpad = -1;     // Touchpad axis id

		// Analog button data to be set in ContollerCoods()
		float padX;
		float padY;
		float trigVal;

		bool menuPressed = false;
		bool gripPressed = false;
		bool padPressed = false;
		bool triggerPressed = false;

		std::string renderModelName = "";

		float3 position;
		quaternion rotation;

		bool isValid;
	} ControllerData;

	typedef struct _TrackerData
	{
		int deviceID = -1; // Device ID according to the SteamVR system
		float3 position;
		quaternion rotation;
		bool isValid;
	} TrackerData;



	void Init();
	void CalculateEyeAdjustment();
	void CalculateProjectionMatrices();
	void Shutdown();
	void Update();


	bool GetVulkanInstanceExtensionsRequired(std::vector< std::string >& outInstanceExtensionList);
	bool GetVulkanDeviceExtensionsRequired(VkPhysicalDevice pPhysicalDevice, std::vector< std::string >& outDeviceExtensionList);
	std::string GetDeviceProperty(vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop, vr::TrackedPropertyError* peError = NULL);

	vr::IVRSystem* System() { return mSystem; }
	float4x4 LeftEyeMatrix() { return mLeftEyeTransform; }
	float4x4 RightEyeMatrix() { return mRightEyeTransform; }
	float4x4 LeftProjection() { return mLeftProjection; }
	float4x4 RightProjection() { return mRightProjection; }

	float3 Position() { return mPosition; }
	quaternion Rotation() { return mRotation; }

protected:
	vr::IVRRenderModels* mRenderModels;

	vr::IVRSystem* mSystem;
	ControllerData mControllers[2];
	TrackerData mTrackers[32];
	vr::TrackedDevicePose_t mTrackedDevicePoses[vr::k_unMaxTrackedDeviceCount];

	float4x4 mLeftEyeTransform, mRightEyeTransform;
	float4x4 mLeftProjection, mRightProjection;
	float mNearClip, mFarClip;

	float4x4 mHeadMatrix;
	float3 mPosition;
	quaternion mRotation;

	void InitializeActions();
	void ProcessEvent(vr::VREvent_t event);
	void UpdateTracking();

	float4x4 ConvertMat34(vr::HmdMatrix34_t);
	float4x4 ConvertMat44(vr::HmdMatrix44_t);
	float4x4 RHtoLH(float4x4 RH);

private:
	// Handles for the new IVRInput
	vr::VRActionSetHandle_t mActionSet = vr::k_ulInvalidActionSetHandle;
	const char* actionSetPath = "/actions/demo";

	vr::VRActionHandle_t mActionAnalogInput = vr::k_ulInvalidActionHandle;
	const char* actionDemoAnalogInputPath = "/actions/demo/in/AnalogInput";

	vr::VRActionHandle_t mActionClick = vr::k_ulInvalidActionHandle;
	const char* actionDemoClickPath = "/actions/demo/in/ClickAction";

	vr::VRActionHandle_t mActionTouch = vr::k_ulInvalidActionHandle;
	const char* actionDemoTouchPath = "/actions/demo/in/TouchAction";

	vr::VRActionHandle_t mActionHandLeft = vr::k_ulInvalidActionHandle;
	const char* actionHandLeftPath = "/actions/demo/in/Hand_Left";

	vr::VRActionHandle_t mActionHandRight = vr::k_ulInvalidActionHandle;
	const char* actionHandRightPath = "/actions/demo/in/Hand_Right";

	vr::VRInputValueHandle_t mInputHandLeftPath = vr::k_ulInvalidInputValueHandle;
	const char* inputHandLeftPath = "/user/hand/left";

	vr::VRInputValueHandle_t mInputHandRightPath = vr::k_ulInvalidInputValueHandle;
	const char* inputHandRightPath = "/user/hand/right";

};