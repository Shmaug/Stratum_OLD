#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>
#include <unordered_map>

#ifdef __GNUC__
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif

#include <Core/Instance.hpp>
#include <Core/PluginManager.hpp>
#include <Input/InputManager.hpp>
#include <Scene/Scene.hpp>
#include <ThirdParty/json11.h>
#include <Util/Util.hpp>
#include <Util/Profiler.hpp>

using namespace std;

// Debug messenger functions
#ifdef ENABLE_DEBUG_LAYERS
VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT * pCallbackData, void* pUserData) {
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT){
		printf_color(BoldRed, "%s: %s\n", pCallbackData->pMessageIdName, pCallbackData->pMessage);
		throw runtime_error("");
	} else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		printf_color(BoldYellow, "%s: %s\n", pCallbackData->pMessageIdName, pCallbackData->pMessage);
	else
		printf("%s: %s\n", pCallbackData->pMessageIdName, pCallbackData->pMessage);

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
	struct CameraInfo {
		uint32_t mTargetWindowIndex;
		float mFieldOfView;
		float mNear;
		float mFar;
		float3 mPosition;
		float3 mRotation;
	};

	vector<Instance::DisplayCreateInfo> mDisplays;
	vector<CameraInfo> mCameras;

	bool mDebugMessenger;
};

class VkCAVE {
private:
	Instance* mInstance;
	InputManager* mInputManager;
	PluginManager* mPluginManager;
	AssetManager* mAssetManager;
	Scene* mScene;

	#ifdef ENABLE_DEBUG_LAYERS
	VkDebugUtilsMessengerEXT mDebugMessenger;
	#endif

	void Render() {
		PROFILER_BEGIN("Get CommandBuffers");
		std::unordered_map<Device*, shared_ptr<CommandBuffer>> commandBuffers;
		for (uint32_t i = 0; i < mScene->Instance()->DeviceCount(); i++) {
			shared_ptr<CommandBuffer> commandBuffer = mScene->Instance()->GetDevice(i)->GetCommandBuffer();
			commandBuffers.emplace(commandBuffer->Device(), commandBuffer);
		}
		PROFILER_END;

		PROFILER_BEGIN("Sort Cameras");
		// sort cameras so that RenderDepth of 0 is last
		sort(mScene->mCameras.begin(), mScene->mCameras.end(), [](const auto& a, const auto& b) {
			return a->RenderPriority() < b->RenderPriority();
		});
		PROFILER_END;

		PROFILER_BEGIN("Scene Pre Frame");
		for (auto& d : commandBuffers)
			mScene->PreFrame(d.second.get());
		PROFILER_END;

		PROFILER_BEGIN("Render Cameras");
		for (const auto& camera : mScene->Cameras()) {
			if (!camera->EnabledHierarchy()) continue;
			mScene->Render(camera, commandBuffers.at(camera->Device()).get(), nullptr);
		}
		PROFILER_END;

		PROFILER_BEGIN("Execute CommandBuffers");
		for (auto& d : commandBuffers)
			d.first->Execute(d.second);
		PROFILER_END;
	}

public:
	VkCAVE(const Configuration* config) : mScene(nullptr), mInstance(nullptr), mInputManager(nullptr)
#ifdef ENABLE_DEBUG_LAYERS
		, mDebugMessenger(VK_NULL_HANDLE)
#endif
	{
		printf("Initializing...\n");
		mInstance = new Instance();
		mInstance->CreateInstance();
		mInputManager = new InputManager();
		mPluginManager = new PluginManager();
		mAssetManager = new AssetManager(mInstance);
		
		#ifdef ENABLE_DEBUG_LAYERS
		if (config->mDebugMessenger) {
			VkDebugUtilsMessengerCreateInfoEXT msgr = {};
			msgr.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			msgr.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			msgr.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			msgr.pfnUserCallback = DebugCallback;
			printf("Creating debug messenger... ");
			VkResult result = CreateDebugUtilsMessengerEXT(*mInstance, &msgr, nullptr, &mDebugMessenger);
			if (result == VK_SUCCESS)
				printf("Success.\n");
			else {
				printf("Failed.\n");
				mDebugMessenger = VK_NULL_HANDLE;
			}
		}
		#endif
		
		// initialize device, create windows and logical devices
		
		vector<Instance::DisplayCreateInfo> displays;

		if (config && config->mDisplays.size())
			displays = config->mDisplays;
		 else
			displays = { {{ {0, 0}, {0, 0} }, -1, 0 } };

		printf("Creating devices and displays... ");
		mInstance->CreateDevicesAndWindows(displays);
		printf("Done.\n");

		mScene = new Scene(mInstance, mAssetManager, mInputManager, mPluginManager);

		printf("Creating %d window cameras... ", (int)displays.size());
		for (uint32_t i = 0; i < config->mCameras.size(); i++) {
			auto w = mInstance->GetWindow(config->mCameras[i].mTargetWindowIndex);
			auto camera = make_shared<Camera>("Camera", w);
			camera->LocalPosition(config->mCameras[i].mPosition);
			camera->LocalRotation(quaternion(radians(config->mCameras[i].mRotation)));
			camera->FieldOfView(radians(config->mCameras[i].mFieldOfView));
			camera->Near(config->mCameras[i].mNear);
			camera->Far(config->mCameras[i].mFar);
			mScene->AddObject(camera);
		}
		for (uint32_t i = 0; i < mInstance->WindowCount(); i++)
			mInputManager->RegisterInputDevice(mInstance->GetWindow(i)->mInput);
		printf("Done.\n");
	}

	VkCAVE* Loop() {
		mPluginManager->LoadPlugins(mScene);

		while (true) {
			#ifdef PROFILER_ENABLE
			Profiler::FrameStart();
			#endif

			PROFILER_BEGIN("Poll Events");
			for (InputDevice* d : mInputManager->mInputDevices)
				d->NextFrame();
			if (!mInstance->PollEvents()) {
				PROFILER_END;
				break;
			}
			PROFILER_END;

			PROFILER_BEGIN("Acquire Swapchain Images");
			bool failed = false;
			// advance window swapchains
			for (uint32_t i = 0; i < mInstance->WindowCount(); i++) {
				Window* w = mInstance->GetWindow(i);
				if (w->AcquireNextImage() == VK_NULL_HANDLE)
					failed = true;
			}
			if (failed) {
				// flush all command buffers to make re-initialization cleaner
				for (uint32_t i = 0; i < mInstance->DeviceCount(); i++)
					mInstance->GetDevice(i)->FlushFrames();
				continue;
			}
			PROFILER_END;

			mScene->Update();
			Render();

			// present windows
			PROFILER_BEGIN("Present");
			mInstance->AdvanceFrame();
			PROFILER_END;

			#ifdef PROFILER_ENABLE
			Profiler::FrameEnd();
			#endif
		}

		for (uint32_t i = 0; i < mInstance->DeviceCount(); i++)
			mInstance->GetDevice(i)->FlushFrames();

		mPluginManager->UnloadPlugins();

		return this;
	}

	~VkCAVE() {
		safe_delete(mScene);

		#ifdef ENABLE_DEBUG_LAYERS
		if (mDebugMessenger != VK_NULL_HANDLE) DestroyDebugUtilsMessengerEXT(*mInstance, mDebugMessenger, nullptr);
		#endif

		safe_delete(mAssetManager);
		safe_delete(mInputManager);
		safe_delete(mInstance);
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
			Instance::DisplayCreateInfo info = {};
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

			config.mDisplays.push_back(info);
		}
	}
	auto& cameras = json["cameras"];
	if (cameras.is_array()) {
		for (const auto& camera : cameras.array_items()) {
			auto& pos = camera["position"];
			auto& rot = camera["rotation"];
			auto& fov = camera["fov"];
			auto& near = camera["near"];
			auto& far = camera["far"];
			auto& idx = camera["display_index"];

			Configuration::CameraInfo info = {};

			if (pos.is_array() && pos.array_items().size() == 3) {
				info.mPosition.x = (float)atof(pos.array_items()[0].string_value().c_str());
				info.mPosition.y = (float)atof(pos.array_items()[1].string_value().c_str());
				info.mPosition.z = (float)atof(pos.array_items()[2].string_value().c_str());
			}
			if (rot.is_array() && rot.array_items().size() == 3) {
				info.mRotation.x = (float)atof(rot.array_items()[0].string_value().c_str());
				info.mRotation.y = (float)atof(rot.array_items()[1].string_value().c_str());
				info.mRotation.z = (float)atof(rot.array_items()[2].string_value().c_str());
			}
			if (fov.is_string()) info.mFieldOfView = (float)atof(fov.string_value().c_str());
			if (near.is_string()) info.mNear = (float)atof(near.string_value().c_str());
			if (far.is_string()) info.mFar = (float)atof(far.string_value().c_str());
			if (idx.is_number()) info.mTargetWindowIndex = (int)idx.number_value();

			config.mCameras.push_back(info);
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

		const char* file = "config.json";

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

	#if defined(WINDOWS)
	OutputDebugString("Dumping Memory Leaks...\n");
	_CrtDumpMemoryLeaks();
	OutputDebugString("Done\n");
	#endif
	return EXIT_SUCCESS;
}