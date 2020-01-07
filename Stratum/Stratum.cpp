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

#include <Util/Util.hpp>

using namespace std;

// Debug messenger functions
#ifdef ENABLE_DEBUG_LAYERS
VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		fprintf_color(BoldRed, stderr, "%s: %s\n", pCallbackData->pMessageIdName, pCallbackData->pMessage);
		throw;
	} else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		if (strcmp("UNASSIGNED-CoreValidation-Shader-OutputNotConsumed", pCallbackData->pMessageIdName) == 0) return VK_FALSE;
		if (strcmp("UNASSIGNED-CoreValidation-DrawState-ClearCmdBeforeDraw", pCallbackData->pMessageIdName) == 0) return VK_FALSE;
		fprintf_color(BoldYellow, stderr, "%s: %s\n", pCallbackData->pMessageIdName, pCallbackData->pMessage);
	} else
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
	vector<Instance::DisplayCreateInfo> mDisplays;
	bool mDebugMessenger;
};

class Stratum {
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
		PROFILER_BEGIN("Sort Cameras");
		// sort cameras so that RenderDepth of 0 is last
		sort(mScene->mCameras.begin(), mScene->mCameras.end(), [](const auto& a, const auto& b) {
			return a->RenderPriority() > b->RenderPriority();
		});
		PROFILER_END;

		PROFILER_BEGIN("Get CommandBuffers");
		std::unordered_map<Device*, shared_ptr<CommandBuffer>> commandBuffers;
		for (uint32_t i = 0; i < mScene->Instance()->DeviceCount(); i++) {
			shared_ptr<CommandBuffer> commandBuffer = mScene->Instance()->GetDevice(i)->GetCommandBuffer();
			commandBuffers.emplace(commandBuffer->Device(), commandBuffer);
		}
		PROFILER_END;

		PROFILER_BEGIN("Scene PreFrame");
		for (auto& d : commandBuffers)
			mScene->PreFrame(d.second.get());
		PROFILER_END;

		PROFILER_BEGIN("Render Cameras");
		for (const auto& camera : mScene->Cameras())
			if (camera->EnabledHierarchy()){
				CommandBuffer* cb = commandBuffers.at(camera->Device()).get();
				mScene->Render(cb, camera, camera->Framebuffer(), PASS_MAIN);
				camera->ResolveWindow(cb);
			}
		PROFILER_END;

		PROFILER_BEGIN("Execute CommandBuffers");
		for (auto& d : commandBuffers)
			d.first->Execute(d.second);
		PROFILER_END;
	}

public:
	Stratum(const Configuration* config) : mScene(nullptr), mInstance(nullptr), mInputManager(nullptr)
#ifdef ENABLE_DEBUG_LAYERS
		, mDebugMessenger(VK_NULL_HANDLE)
#endif
	{
		bool useglfw = false;
		bool directMode = false;
		for (auto& d : config->mDisplays)
			if (d.mDirectDisplay >= 0)
				directMode = true;
			else
				useglfw = true;

		printf("Initializing...\n");
		mInstance = new Instance(directMode, useglfw);
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
		printf("Creating devices and displays... ");
		mInstance->CreateDevicesAndWindows(config->mDisplays);
		printf("Done.\n");

		mScene = new Scene(mInstance, mAssetManager, mInputManager, mPluginManager);
		for (uint32_t i = 0; i < mInstance->WindowCount(); i++)
			mInputManager->RegisterInputDevice(mInstance->GetWindow(i)->mInput);
	}

	Stratum* Loop() {
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
			for (uint32_t i = 0; i < mInstance->WindowCount(); i++)
				mInstance->GetWindow(i)->AcquireNextImage();
			PROFILER_END;

			mScene->Update();
			Render();

			// present windows
			PROFILER_BEGIN("Present");
			mInstance->AdvanceFrame();
			PROFILER_END;

			// for testing: disable asynchronous frames
			for (uint32_t i = 0; i < mInstance->DeviceCount(); i++)
				mInstance->GetDevice(i)->FlushFrames();

			#ifdef PROFILER_ENABLE
			Profiler::FrameEnd();
			#endif
		}

		for (uint32_t i = 0; i < mInstance->DeviceCount(); i++)
			mInstance->GetDevice(i)->FlushFrames();

		mPluginManager->UnloadPlugins();

		return this;
	}

	~Stratum() {
		safe_delete(mScene);

		#ifdef ENABLE_DEBUG_LAYERS
		if (mDebugMessenger != VK_NULL_HANDLE) DestroyDebugUtilsMessengerEXT(*mInstance, mDebugMessenger, nullptr);
		#endif

		safe_delete(mAssetManager);
		safe_delete(mInputManager);
		safe_delete(mInstance);
	}
};

bool ReadConfig(const string& file, Configuration& config) {
	ifstream stream(file);
	if (!stream.is_open()) {
		printf("Failed to open %s\n", file.c_str());
		return false;
	}
	stringstream buffer;
	buffer << stream.rdbuf();

	string err;
	auto json = json11::Json::parse(buffer.str(), err);

	if (json.is_null()) {
		printf("Failed to read %s: %s\n", file.c_str(), err.c_str());
		return false;
	}

	auto& displays = json["displays"];
	if (displays.is_array()) {
		for (const auto& d : displays.array_items()) {
			auto& device = d["device"];
			auto& rect = d["window_rect"];
			auto& direct = d["direct_display"];

			Instance::DisplayCreateInfo info = {};
			info.mDevice = device.is_number() ? device.int_value() : -1;
			info.mDirectDisplay = direct.is_number() ? direct.int_value() : -1;
			if (rect.is_array() && rect.array_items().size() == 4 &&
				rect.array_items()[0].is_number() &&
				rect.array_items()[1].is_number() &&
				rect.array_items()[2].is_number() &&
				rect.array_items()[3].is_number()) {
				info.mWindowPosition.offset.x = (int)rect.array_items()[0].number_value();
				info.mWindowPosition.offset.y = (int)rect.array_items()[1].number_value();
				info.mWindowPosition.extent.width = (int)rect.array_items()[2].number_value();
				info.mWindowPosition.extent.height = (int)rect.array_items()[3].number_value();
			} else
				info.mWindowPosition = { { 160, 90 }, { 1600, 900 } };

			config.mDisplays.push_back(info);
		}
	}
	return true;
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
		if (!ReadConfig(file, config)){
			config.mDisplays.push_back({});
			config.mDisplays[0].mDevice = 0;
			config.mDisplays[0].mWindowPosition = { { 160, 90 }, { 1600, 900 } };
			config.mDisplays[0].mDirectDisplay = -1;
		}
		if (argc > 2)
			config.mDebugMessenger = strcmp(argv[2], "-nodebug") != 0;
		else
			config.mDebugMessenger = true;

		// create, run, and delete the engine all in one line :)
		delete (new Stratum(&config))->Loop();

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