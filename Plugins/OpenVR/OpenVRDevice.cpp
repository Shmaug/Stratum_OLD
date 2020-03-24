#include "OpenVRDevice.hpp"

// Converts to float4x4 format and flips from right-handed to left-handed
float4x4 OpenVRDevice::ConvertMat34(vr::HmdMatrix34_t mat34) {
	return float4x4(
		mat34.m[0][0], mat34.m[0][1], mat34.m[0][2], mat34.m[0][3],
		mat34.m[1][0], mat34.m[1][1], mat34.m[1][2], mat34.m[1][3],
		mat34.m[2][0], mat34.m[2][1], mat34.m[2][2], mat34.m[2][3],
		0.f, 0.f, 0.f, 1.f
	);
}
float4x4 OpenVRDevice::ConvertMat44(vr::HmdMatrix44_t mat44) {
	return float4x4(
		mat44.m[0][0], mat44.m[0][1], mat44.m[0][2], mat44.m[0][3],
		mat44.m[1][0], mat44.m[1][1], mat44.m[1][2], mat44.m[1][3],
		mat44.m[2][0], mat44.m[2][1], mat44.m[2][2], mat44.m[2][3],
		mat44.m[3][0], mat44.m[3][1], mat44.m[3][2], mat44.m[3][3]
	);
}

float4x4 OpenVRDevice::RHtoLH(float4x4 RH) {
	return float4x4(
		RH[0][0], RH[1][0], -RH[2][0], RH[3][0],
		RH[0][1], RH[1][1], -RH[2][1], RH[3][1],
		-RH[0][2], -RH[1][2], RH[2][2], -RH[3][2],
		RH[0][3], RH[1][3], -RH[2][3], RH[3][3]
	);
}

OpenVRDevice::OpenVRDevice(float near, float far)
	: mNearClip(near), mFarClip(far), mSystem(nullptr), mPosition(float3()), mRotation(quaternion()) {
	Init();
}

OpenVRDevice::~OpenVRDevice() {
	vr::VR_Shutdown();
}

void OpenVRDevice::Init() {
	vr::EVRInitError eError = vr::VRInitError_None;
	mSystem = vr::VR_Init(&eError, vr::VRApplication_Scene);

	if (eError != vr::VRInitError_None)
	{
		mSystem = nullptr;
		fprintf_color(COLOR_RED, stderr, 
			"Error: Unable to initialize the OpenVR library.\nReason: %s\n", 
			vr::VR_GetVRInitErrorAsEnglishDescription(eError));
		throw "OPENVR_FAILURE";
		return;
	}

	if (!vr::VRCompositor())
	{
		mSystem = nullptr;
		vr::VR_Shutdown();
		fprintf_color(COLOR_RED, stderr,
			"Error: Compositor initialization failed");
		throw "OPENVR_FAILURE";
		return;
	}

	mRenderModels = (vr::IVRRenderModels*)vr::VR_GetGenericInterface(vr::IVRRenderModels_Version, &eError);
	if (mRenderModels == nullptr)
	{
		mSystem = nullptr;
		vr::VR_Shutdown();
		fprintf_color(COLOR_RED, stderr,
			"Error: Unable to get render model interface!\nReason: %s", 
			vr::VR_GetVRInitErrorAsEnglishDescription(eError));
		throw "OPENVR_FAILURE";
		return;
	}

	//InitializeActions();

	std::string driverName = GetDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String);
	std::string deviceSerialNumber = GetDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String);

	fprintf_color(COLOR_GREEN, stdout, "OpenVR HMD Driver Initialized\Driver name: %s\nDriver serial#: %s\n",
		driverName, deviceSerialNumber);

}


// Adapted from https://github.com/Omnifinity/OpenVR-Tracking-Example
void OpenVRDevice::InitializeActions() {

	// Prepare manifest file
	const char* manifestPath = "C:/Users/pt/Documents/Visual Studio 2013/Projects/HTC Lighthouse Tracking Example/Release/win32/vive_debugger_actions.json";
	
	vr::EVRInputError inputError = vr::VRInput()->SetActionManifestPath(manifestPath);
	if (inputError != vr::VRInputError_None) {
		printf_color(COLOR_RED, "Error: Unable to set manifest path: %d\n", inputError);
		throw "OPENVR_FAILURE";
	}

	// Handles for the new IVRInput
	inputError = vr::VRInput()->GetActionSetHandle(actionSetPath, &mActionSet);
	if (inputError != vr::VRInputError_None) {
		printf_color(COLOR_RED, "Error: Unable to get action set handle: %d\n", inputError);
	}

	// handle for left controller pose
	inputError = vr::VRInput()->GetActionHandle(actionHandLeftPath, &mActionHandLeft);
	if (inputError != vr::VRInputError_None) {
		printf_color(COLOR_RED, "Error: Unable to get action handle: %d\n", inputError);
	}

	// handle for right controller pose
	inputError = vr::VRInput()->GetActionHandle(actionHandRightPath, &mActionHandRight);
	if (inputError != vr::VRInputError_None) {
		printf_color(COLOR_RED, "Error: Unable to get action handle: %d\n", inputError);
	}

	// handle for analog trackpad action
	inputError = vr::VRInput()->GetActionHandle(actionDemoAnalogInputPath, &mActionAnalogInput);
	if (inputError != vr::VRInputError_None) {
		printf_color(COLOR_RED, "Error: Unable to get action handle: %d\n", inputError);
	}

	// handle for a touch action
	inputError = vr::VRInput()->GetActionHandle(actionDemoTouchPath, &mActionTouch);
	if (inputError != vr::VRInputError_None) {
		printf_color(COLOR_RED, "Error: Unable to get action handle: %d\n", inputError);
	}

	// handle for a click action
	inputError = vr::VRInput()->GetActionHandle(actionDemoClickPath, &mActionClick);
	if (inputError != vr::VRInputError_None) {
		printf_color(COLOR_RED, "Error: Unable to get action handle: %d\n", inputError);
	}

	// handle for controller pose source - not used atm
	inputError = vr::VRInput()->GetInputSourceHandle(inputHandLeftPath, &mInputHandLeftPath);
	if (inputError != vr::VRInputError_None) {
		printf_color(COLOR_RED, "Error: Unable to get input handle: %d\n", inputError);
	}

	inputError = vr::VRInput()->GetInputSourceHandle(inputHandRightPath, &mInputHandRightPath);
	if (inputError != vr::VRInputError_None) {
		printf_color(COLOR_RED, "Error: Unable to get input handle: %d\n", inputError);
	}

}

std::string OpenVRDevice::GetDeviceProperty(vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop, vr::TrackedPropertyError* peError)
{
	uint32_t bufferLen = mSystem->GetStringTrackedDeviceProperty(unDevice, prop, NULL, 0, peError);
	if (bufferLen == 0)
	{
		return "";
	}

	char* buffer = new char[bufferLen];
	bufferLen = mSystem->GetStringTrackedDeviceProperty(unDevice, prop, buffer, bufferLen, peError);
	std::string result = buffer;
	delete[] buffer;
	return result;
}

void OpenVRDevice::Shutdown() {

}

void OpenVRDevice::Update() {
	/*
	vr::VRActiveActionSet_t actionSet = { 0 };
	actionSet.ulActionSet = m_actionsetDemo;
	vr::VRInput()->UpdateActionState(&actionSet, sizeof(actionSet), 1);

	for (EHand eHand = Left; eHand <= Right; ((int&)eHand)++)
	{
		vr::InputPoseActionData_t poseData;
		if (vr::VRInput()->GetPoseActionDataForNextFrame(m_rHand[eHand].m_actionPose, vr::TrackingUniverseStanding, &poseData, sizeof(poseData), vr::k_ulInvalidInputValueHandle) != vr::VRInputError_None
			|| !poseData.bActive || !poseData.pose.bPoseIsValid)
		{
			m_rHand[eHand].m_bShowController = false;
		}
		else
		{
			m_rHand[eHand].m_rmat4Pose = ConvertSteamVRMatrixToMatrix4(poseData.pose.mDeviceToAbsoluteTracking);

			vr::InputOriginInfo_t originInfo;
			if (vr::VRInput()->GetOriginTrackedDeviceInfo(poseData.activeOrigin, &originInfo, sizeof(originInfo)) == vr::VRInputError_None
				&& originInfo.trackedDeviceIndex != vr::k_unTrackedDeviceIndexInvalid)
			{
				std::string sRenderModelName = GetTrackedDeviceString(originInfo.trackedDeviceIndex, vr::Prop_RenderModelName_String);
				if (sRenderModelName != m_rHand[eHand].m_sRenderModelName)
				{
					m_rHand[eHand].m_pRenderModel = FindOrLoadRenderModel(sRenderModelName.c_str());
					m_rHand[eHand].m_sRenderModelName = sRenderModelName;
				}
			}
		}
	}
	*/
	vr::VRCompositor()->WaitGetPoses(mTrackedDevicePoses, vr::k_unMaxTrackedDeviceCount, NULL, 0);
	if (mTrackedDevicePoses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
	{
		mHeadMatrix = ConvertMat34(mTrackedDevicePoses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking);
		//mHeadMatrix = RHtoLH(mHeadMatrix);
		mHeadMatrix.Decompose(&mPosition, &mRotation, nullptr);
		//mPosition.z = -mPosition.z;
		//mRotation.x = -mRotation.x;
		//mRotation.y = -mRotation.y;
		//printf_color(COLOR_MAGENTA, "Head position: %f, %f, %f\nHead rotation: %f, %f, %f, %f\n\n", mPosition.x, mPosition.y, mPosition.z, mRotation.x, mRotation.y, mRotation.z, mRotation.w);
	}
}

void OpenVRDevice::ProcessEvent(vr::VREvent_t event) {

}

void OpenVRDevice::UpdateTracking() {
	vr::EVRInputError inputError;

	vr::VRActiveActionSet_t actionSet = { 0 };
	actionSet.ulActionSet = mActionSet;
	vr::VRInput()->UpdateActionState(&actionSet, sizeof(actionSet), 1);

	/*
	vr::InputAnalogActionData_t analogData;
	inputError = vr::VRInput()->GetAnalogActionData(mActionAnalogInput, &analogData, sizeof(analogData), vr::k_ulInvalidInputValueHandle);
	if (inputError == vr::VRInputError_None && analogData.bActive)
	{
		float m_vAnalogValue0 = analogData.x;
		float m_vAnalogValue1 = analogData.y;

		// check from which device the action came
		vr::InputOriginInfo_t originInfo;
		if (vr::VRInputError_None == vr::VRInput()->GetOriginTrackedDeviceInfo(analogData.activeOrigin, &originInfo, sizeof(originInfo)))
		{
			if (originInfo.devicePath == mInputHandLeftPath) {
			}
			else if (originInfo.devicePath == mInputHandRightPath) {
			}

		}
	}

	// Get digital data of a "Touch Action"
	vr::InputDigitalActionData_t digitalDataTouch;
	inputError = vr::VRInput()->GetDigitalActionData(mActionTouch, &digitalDataTouch, sizeof(digitalDataTouch), vr::k_ulInvalidInputValueHandle);
	if (inputError == vr::VRInputError_None)
	{
		bool val = digitalDataTouch.bState;

		// check from which device the action came
		vr::InputOriginInfo_t originInfo;
		if (vr::VRInputError_None == vr::VRInput()->GetOriginTrackedDeviceInfo(digitalDataTouch.activeOrigin, &originInfo, sizeof(originInfo)))
		{
			if (originInfo.devicePath == mInputHandLeftPath) {
			}
			else if (originInfo.devicePath == mInputHandRightPath) {
			}
		}
	}

	// Get digital data of a "Click Action"
	vr::InputDigitalActionData_t digitalDataClick;
	inputError = vr::VRInput()->GetDigitalActionData(mActionClick, &digitalDataClick, sizeof(digitalDataClick), vr::k_ulInvalidInputValueHandle);
	if (inputError == vr::VRInputError_None && digitalDataClick.bActive)
	{
		bool val = digitalDataClick.bState;

		// check from which device the action came
		vr::InputOriginInfo_t originInfo;
		if (vr::VRInputError_None == vr::VRInput()->GetOriginTrackedDeviceInfo(digitalDataClick.activeOrigin, &originInfo, sizeof(originInfo)))
		{
			if (originInfo.devicePath == mInputHandLeftPath) {
			}
			else if (originInfo.devicePath == mInputHandRightPath) {
			}
		}
	}
	*/

	// get pose data for each controller
	vr::InputPoseActionData_t poseData;
	inputError = vr::VRInput()->GetPoseActionDataForNextFrame(mActionHandLeft, vr::TrackingUniverseStanding, &poseData, sizeof(poseData), vr::k_ulInvalidInputValueHandle);
	if (inputError == vr::VRInputError_None && poseData.bActive && poseData.pose.bPoseIsValid && poseData.pose.bDeviceIsConnected) {

		float4x4 pose = ConvertMat34(poseData.pose.mDeviceToAbsoluteTracking);
		pose.Decompose(&mControllers[0].position, &mControllers[0].rotation, nullptr);

	}
}

void OpenVRDevice::CalculateEyeAdjustment() {
	vr::HmdMatrix34_t mat;

	mat = mSystem->GetEyeToHeadTransform(vr::Eye_Left);
	mLeftEyeTransform = (ConvertMat34(mat));
	mat = mSystem->GetEyeToHeadTransform(vr::Eye_Right);
	mRightEyeTransform = (ConvertMat34(mat));
}

void OpenVRDevice::CalculateProjectionMatrices() {
	vr::HmdMatrix44_t mat;
	float l, r, t, b;

	mSystem->GetProjectionRaw(vr::Eye_Left, &l, &r, &t, &b);
	l *= mNearClip;
	r *= mNearClip;
	t *= mNearClip;
	b *= mNearClip;
	//mLeftProjection = float4x4::Perspective(l, r, t, b, mNearClip, mFarClip);
	//Need to invert near/far. dont ask me why this works
	mLeftProjection = ConvertMat44(mSystem->GetProjectionMatrix(vr::Eye_Right, -mNearClip, -mFarClip));
	mLeftProjection[1][1] = -mLeftProjection[1][1];
	mLeftProjection[2][2] = -mLeftProjection[2][2];
	mLeftProjection[3][2] = -mLeftProjection[3][2];
	mLeftProjection[2][3] = -mLeftProjection[2][3];

	mSystem->GetProjectionRaw(vr::Eye_Right, &l, &r, &t, &b);
	l *= mNearClip;
	r *= mNearClip;
	t *= mNearClip;
	b *= mNearClip;
	//mRightProjection = float4x4::Perspective(l, r, t, b, mNearClip, mFarClip);
	mRightProjection = ConvertMat44(mSystem->GetProjectionMatrix(vr::Eye_Right, -mNearClip, -mFarClip));
	mRightProjection[1][1] = -mRightProjection[1][1];
	mRightProjection[2][2] = -mRightProjection[2][2];
	mRightProjection[3][2] = -mRightProjection[3][2];
	mRightProjection[2][3] = -mRightProjection[2][3];

}


//Extension getters taken from https://github.com/ValveSoftware/openvr/blob/master/samples/hellovr_vulkan/hellovr_vulkan_main.cpp
#pragma region Extensions

bool OpenVRDevice::GetVulkanInstanceExtensionsRequired(std::vector< std::string >& outInstanceExtensionList)
{
	if (!vr::VRCompositor())
	{
		return false;
	}

	outInstanceExtensionList.clear();
	uint32_t nBufferSize = vr::VRCompositor()->GetVulkanInstanceExtensionsRequired(nullptr, 0);
	if (nBufferSize > 0)
	{
		// Allocate memory for the space separated list and query for it
		char* pExtensionStr = new char[nBufferSize];
		pExtensionStr[0] = 0;
		vr::VRCompositor()->GetVulkanInstanceExtensionsRequired(pExtensionStr, nBufferSize);

		// Break up the space separated list into entries on the CUtlStringList
		std::string curExtStr;
		uint32_t nIndex = 0;
		while (pExtensionStr[nIndex] != 0 && (nIndex < nBufferSize))
		{
			if (pExtensionStr[nIndex] == ' ')
			{
				outInstanceExtensionList.push_back(curExtStr);
				curExtStr.clear();
			}
			else
			{
				curExtStr += pExtensionStr[nIndex];
			}
			nIndex++;
		}
		if (curExtStr.size() > 0)
		{
			outInstanceExtensionList.push_back(curExtStr);
		}

		delete[] pExtensionStr;
	}

	return true;
}

bool OpenVRDevice::GetVulkanDeviceExtensionsRequired(VkPhysicalDevice pPhysicalDevice, std::vector< std::string >& outDeviceExtensionList)
{
	if (!vr::VRCompositor())
	{
		return false;
	}

	outDeviceExtensionList.clear();
	uint32_t nBufferSize = vr::VRCompositor()->GetVulkanDeviceExtensionsRequired((VkPhysicalDevice_T*)pPhysicalDevice, nullptr, 0);
	if (nBufferSize > 0)
	{
		// Allocate memory for the space separated list and query for it
		char* pExtensionStr = new char[nBufferSize];
		pExtensionStr[0] = 0;
		vr::VRCompositor()->GetVulkanDeviceExtensionsRequired((VkPhysicalDevice_T*)pPhysicalDevice, pExtensionStr, nBufferSize);

		// Break up the space separated list into entries on the CUtlStringList
		std::string curExtStr;
		uint32_t nIndex = 0;
		while (pExtensionStr[nIndex] != 0 && (nIndex < nBufferSize))
		{
			if (pExtensionStr[nIndex] == ' ')
			{
				outDeviceExtensionList.push_back(curExtStr);
				curExtStr.clear();
			}
			else
			{
				curExtStr += pExtensionStr[nIndex];
			}
			nIndex++;
		}
		if (curExtStr.size() > 0)
		{
			outDeviceExtensionList.push_back(curExtStr);
		}

		delete[] pExtensionStr;
	}

	return true;
}

#pragma endregion