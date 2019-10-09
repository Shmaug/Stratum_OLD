#ifdef WINDOWS
#include <winsock2.h>
#include <stdlib.h>
#include <Windows.h>
#undef near
#undef far
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif
#endif

#include <chrono>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <thread>
#include <unordered_map>

#include <Core/DeviceManager.hpp>
#include <Core/PluginManager.hpp>
#include <Input/InputManager.hpp>
#include <Scene/Scene.hpp>
#include <ThirdParty/json11.h>
#include <Util/Util.hpp>
#include <Util/Profiler.hpp>

using namespace std;

// Debug messenger functions
#ifdef _DEBUG
VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT * pCallbackData, void* pUserData) {
	#ifdef WINDOWS
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED);
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN);
	#endif

	printf("%s: %s\n", pCallbackData->pMessageIdName, pCallbackData->pMessage);
	
	if (messageSeverity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)) {
		#ifdef WINDOWS
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
		#endif
		throw runtime_error(pCallbackData->pMessage);
	}
	return VK_FALSE;
}
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	else
		return VK_ERROR_EXTENSION_NOT_PRESENT;
}
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
		func(instance, debugMessenger, pAllocator);
}
#endif

struct Configuration {
	vector<DeviceManager::DisplayCreateInfo> mDisplays;
	bool mDebugMessenger;
};

class VkCAVE {
private:
	DeviceManager* mDeviceManager;
	InputManager* mInputManager;
	PluginManager* mPluginManager;
	AssetManager* mAssetManager;
	Scene* mScene;

	#ifdef _DEBUG
	VkDebugUtilsMessengerEXT mDebugMessenger;
	#endif

	void Render(FrameTime* frameTime, uint32_t backBufferIndex, vector<shared_ptr<Fence>>& fences) {
		PROFILER_BEGIN("Sort Cameras");
		// sort cameras so that RenderDepth of 0 is last
		sort(mScene->mCameras.begin(), mScene->mCameras.end(), [](const auto& a, const auto& b) {
			return a->RenderPriority() < b->RenderPriority();
		});
		PROFILER_END;

		PROFILER_BEGIN("Render Cameras");
		std::unordered_map<Device*, shared_ptr<CommandBuffer>> commandBuffers;
		for (const auto& camera : mScene->Cameras()) {
			PROFILER_BEGIN("Get CommandBuffer");
			shared_ptr<CommandBuffer> commandBuffer;
			if (commandBuffers.count(camera->Device()))
				commandBuffer = commandBuffers.at(camera->Device());
			else {
				commandBuffer = camera->Device()->GetCommandBuffer();
				commandBuffers.emplace(camera->Device(), commandBuffer);
			}
			PROFILER_END;

			mScene->Render(*frameTime, camera, commandBuffer.get(), backBufferIndex);
		}
		PROFILER_END;

		PROFILER_BEGIN("Execute CommandBuffers");
		for (auto& d : commandBuffers)
			fences.push_back(d.first->Execute(d.second));
		PROFILER_END;
	}

public:
	VkCAVE(const Configuration* config) : mScene(nullptr), mDeviceManager(nullptr), mInputManager(nullptr)
#ifdef _DEBUG
		, mDebugMessenger(VK_NULL_HANDLE)
#endif
	{
		printf("Initializing...\n");
		mDeviceManager = new DeviceManager();
		mDeviceManager->CreateInstance();

		mInputManager = new InputManager();
		mPluginManager = new PluginManager();
		mAssetManager = new AssetManager(mDeviceManager);
		
		#ifdef _DEBUG
		if (config->mDebugMessenger) {
			VkDebugUtilsMessengerCreateInfoEXT msgr = {};
			msgr.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			msgr.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			msgr.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			msgr.pfnUserCallback = DebugCallback;
			printf("Creating debug messenger... ");
			VkResult result = CreateDebugUtilsMessengerEXT(mDeviceManager->Instance(), &msgr, nullptr, &mDebugMessenger);
			if (result == VK_SUCCESS)
				printf("Success.\n");
			else {
				printf("Failed.\n");
				mDebugMessenger = VK_NULL_HANDLE;
			}
		}
		#endif
		
		// initialize device, create windows and logical devices
		
		vector<DeviceManager::DisplayCreateInfo> displays;

		if (config && config->mDisplays.size())
			displays = config->mDisplays;
		 else
			displays = { {{ {0, 0}, {0, 0} }, -1, 0 } };

		printf("Creating devices and displays... ");
		mDeviceManager->Initialize(displays);
		printf("Done.\n");

		mScene = new Scene(mDeviceManager, mAssetManager, mInputManager, mPluginManager);

		printf("Creating %d window cameras... ", (int)displays.size());
		for (uint32_t i = 0; i < displays.size(); i++) {
			auto w = mDeviceManager->GetWindow(i);
			auto camera = make_shared<Camera>("Camera", w);
			camera->LocalPosition(displays[i].mCameraPos);
			camera->LocalRotation(quat(radians(displays[i].mCameraRot)));
			camera->FieldOfView(radians(displays[i].mCameraFov));
			camera->Near(displays[i].mCameraNear);
			camera->Far(displays[i].mCameraFar);
			mScene->AddObject(camera);
		}
		for (uint32_t i = 0; i < mDeviceManager->WindowCount(); i++)
			mInputManager->RegisterInputDevice(mDeviceManager->GetWindow(i)->Input());
		printf("Done.\n");
	}

	VkCAVE* Loop() {
		mPluginManager->LoadPlugins(mScene);

		chrono::high_resolution_clock clock;
		auto start = clock.now();
		auto t0 = clock.now();
		auto t1 = clock.now();

		vector<shared_ptr<Fence>> fences;

		FrameTime frameTime = {};
		frameTime.mFrameNumber = 0;
		while (true) {
			#ifdef PROFILER_ENABLE
			Profiler::FrameStart();
			#endif

			PROFILER_BEGIN("Poll Events");
			if (!mDeviceManager->PollEvents()) {
				PROFILER_END;
				break;
			}
			t1 = clock.now();
			frameTime.mDeltaTime = (float)((t1 - t0).count() * 1e-9);
			frameTime.mTotalTime = (float)((t1 - start).count() * 1e-9);
			t0 = t1;
			PROFILER_END;

			PROFILER_BEGIN("Acquire Swapchain Images");
			bool failed = false;
			// advance window swapchains
			for (uint32_t i = 0; i < mDeviceManager->WindowCount(); i++)
				if (mDeviceManager->GetWindow(i)->AcquireNextImage() == VK_NULL_HANDLE)
					failed = true;
			if (failed) {
				// flush all command buffers to 'reset' the scene
				for (uint32_t i = 0; i < mDeviceManager->DeviceCount(); i++)
					mDeviceManager->GetDevice(i)->FlushCommandBuffers();
				continue;
			}
			PROFILER_END;

			mScene->Update(frameTime);

			// render cameras
			fences.clear();
			Render(&frameTime, mDeviceManager->GetWindow(0)->CurrentBackBufferIndex(), fences); // TODO: uses backBufferIndex of the first window (assumes they are in sync!)

			// present windows
			PROFILER_BEGIN("Present");
			mDeviceManager->PresentWindows(fences);
			PROFILER_END;
			
			for (InputDevice* d : mInputManager->mInputDevices)
				d->NextFrame();

			#ifdef PROFILER_ENABLE
			Profiler::FrameEnd();
			#endif
			frameTime.mFrameNumber++;
		}

		for (uint32_t i = 0; i < mDeviceManager->DeviceCount(); i++)
			mDeviceManager->GetDevice(i)->FlushCommandBuffers();

		mPluginManager->UnloadPlugins();

		return this;
	}

	~VkCAVE() {
		safe_delete(mScene);

		#ifdef _DEBUG
		if (mDebugMessenger != VK_NULL_HANDLE) DestroyDebugUtilsMessengerEXT(mDeviceManager->Instance(), mDebugMessenger, nullptr);
		#endif

		safe_delete(mAssetManager);
		safe_delete(mInputManager);
		safe_delete(mDeviceManager);
	}
};

void ReadConfig(const string& file, Configuration& config) {
	ifstream stream(file);
	if (!stream.is_open()) {
		printf("Failed to open %s\n", file.c_str());
		return;
	}
	stringstream buffer;
	buffer << stream.rdbuf();

	string err;
	auto json = json11::Json::parse(buffer.str(), err);

	if (json.is_null()) {
		printf("Failed to read %s: %s\n", file.c_str(), err.c_str());
		return;
	}

	auto& displays = json["displays"];
	if (displays.is_array()) {
		for (const auto& d : displays.array_items()) {
			DeviceManager::DisplayCreateInfo info = {};
			auto& rect = d["window_rect"];
			auto& monitor = d["monitor"];
			auto& device = d["device"];
			auto& camera = d["camera"];

			if (rect.is_array() && rect.array_items().size() == 4 &&
				rect.array_items()[0].is_number() &&
				rect.array_items()[1].is_number() &&
				rect.array_items()[2].is_number() &&
				rect.array_items()[3].is_number()) {
				info.mWindowPosition.offset.x = (int)rect.array_items()[0].number_value();
				info.mWindowPosition.offset.y = (int)rect.array_items()[1].number_value();
				info.mWindowPosition.extent.width = (int)rect.array_items()[2].number_value();
				info.mWindowPosition.extent.height = (int)rect.array_items()[3].number_value();
			}

			if (monitor.is_number()) info.mMonitor = (int)monitor.number_value();
			if (device.is_number()) info.mDevice = (int)device.number_value();

			if (camera.is_object()) {
				auto& pos = camera["position"];
				auto& rot = camera["rotation"];
				auto& fov = camera["fov"];
				auto& near = camera["near"];
				auto& far = camera["far"];

				if (pos.is_array() && pos.array_items().size() == 3) {
					info.mCameraPos.x = (float)atof(pos.array_items()[0].string_value().c_str());
					info.mCameraPos.y = (float)atof(pos.array_items()[1].string_value().c_str());
					info.mCameraPos.z = (float)atof(pos.array_items()[2].string_value().c_str());
				}
				if (rot.is_array() && rot.array_items().size() == 3) {
					info.mCameraRot.x = (float)atof(rot.array_items()[0].string_value().c_str());
					info.mCameraRot.y = (float)atof(rot.array_items()[1].string_value().c_str());
					info.mCameraRot.z = (float)atof(rot.array_items()[2].string_value().c_str());
				}
				if (fov.is_string()) info.mCameraFov = (float)atof(fov.string_value().c_str());
				if (near.is_string()) info.mCameraNear = (float)atof(near.string_value().c_str());
				if (far.is_string()) info.mCameraFar = (float)atof(far.string_value().c_str());
			}

			config.mDisplays.push_back(info);
		}
	}

	printf("Read %s\n", file.c_str());
}

int main(int argc, char* argv[]) {
	{ 
		#ifdef WINDOWS
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
			cerr << "WSAStartup failed" << endl;
		#endif

		char* file = "config.json";

		if (argc > 1) file = argv[1];

		Configuration config = {};
		ReadConfig(file, config);
		if (argc > 2)
			config.mDebugMessenger = strcmp(argv[2], "-nodebug") != 0;
		else
			config.mDebugMessenger = true;

		// create, run, and delete the engine all in one line :)
		delete (new VkCAVE(&config))->Loop();

		#ifdef WINDOWS
		WSACleanup();
		#endif
	}

	#if defined(WINDOWS) && defined(_DEBUG)
	_CrtDumpMemoryLeaks();
	#endif
	return EXIT_SUCCESS;
}