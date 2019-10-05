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
#include <Scene/Scene.hpp>
#include <ThirdParty/json11.h>
#include <Util/Util.hpp>
#include <Util/Profiler.hpp>

#ifdef WINDOWS
typedef EnginePlugin* (__cdecl* CreatePluginProc)(void);
#else
static_assert(false, "Not implemented!");
#endif

using namespace std;

// Debug messenger functions
#ifdef _DEBUG
VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT * pCallbackData, void* pUserData) {
	#ifdef WINDOWS
	OutputDebugString(pCallbackData->pMessageIdName);
	OutputDebugString(": ");
	OutputDebugString(pCallbackData->pMessage);
	OutputDebugString("\n");
	#endif
	cerr << pCallbackData->pMessageIdName << ": " << pCallbackData->pMessage << endl;
	if (strcmp(pCallbackData->pMessage, "Added messenger") != 0 && strcmp(pCallbackData->pMessage, "Destroyed messenger\n") != 0)
		throw runtime_error(pCallbackData->pMessage);
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
	Scene* mScene;

	std::unordered_map<Camera*, Window*> mCameraWindowMap;

	#ifdef _DEBUG
	VkDebugUtilsMessengerEXT mDebugMessenger;
	#endif

	#ifdef WINDOWS
	vector<HMODULE> mPluginModules;
	#else
	static_assert(false, "Not implemented!");
	#endif

	void LoadPlugins() {
		for (const auto& p : filesystem::directory_iterator("Plugins")) {
			#ifdef WINDOWS
			// load plugin DLLs
			if (wcscmp(p.path().extension().c_str(), L".dll") == 0) {
				printf("Loading %S ... ", p.path().c_str());
				HMODULE m = LoadLibraryW(p.path().c_str());
				if (m == NULL) {
					fprintf(stderr, "Failed to load library!\n");
					continue;
				}
				CreatePluginProc proc = (CreatePluginProc)GetProcAddress(m, "CreatePlugin");
				if (proc == NULL) {
					fprintf(stderr, "Failed to find create function!\n");
					if (!FreeLibrary(m)) fprintf(stderr, "Failed to unload %S\n", p.path().c_str());
					continue;
				}
				EnginePlugin* plugin = proc();
				if (plugin == nullptr) {
					fprintf(stderr, "Failed to call CreatePlugin!\n");
					if (!FreeLibrary(m)) fprintf(stderr, "Failed to unload %S\n", p.path().c_str());
					continue;
				}
				mScene->mPlugins.push_back(plugin);
				mPluginModules.push_back(m);
				printf("Done\n");
			}
			#else
			static_assert(false, "Not implemented!");
			#endif
		}

		sort(mScene->mPlugins.begin(), mScene->mPlugins.end(), [](const auto& a, const auto& b) {
			return a->Priority() < b->Priority();
		});

		for (const auto& p : mScene->mPlugins)
			p->Init(mScene);
	}
	void UnloadPlugins() {
		for (auto& p : mScene->mPlugins)
			safe_delete(p);
		mScene->mPlugins.clear();
		#ifdef WINDOWS
		for (const auto& m : mPluginModules)
			if (!FreeLibrary(m))
				cerr << "Failed to unload plugin module" << endl;
		#else
		static_assert(false, "Not implemented!");
		#endif
	}

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
			PROFILER_BEGIN("Get CommandBuffer and TargetWindow");
			shared_ptr<CommandBuffer> commandBuffer;
			if (commandBuffers.count(camera->Device()))
				commandBuffer = commandBuffers.at(camera->Device());
			else {
				commandBuffer = camera->Device()->GetCommandBuffer();
				commandBuffers.emplace(camera->Device(), commandBuffer);
			}

			Window* targetWindow = nullptr;
			if (mCameraWindowMap.count(camera))
				targetWindow = mCameraWindowMap.at(camera);
			PROFILER_END;

			// Pre Render
			PROFILER_BEGIN("Pre Render");
			if (targetWindow && (camera->PixelWidth() != targetWindow->ClientRect().extent.width || camera->PixelHeight() != targetWindow->ClientRect().extent.height)) {
				camera->PixelWidth(targetWindow->BackBufferSize().width);
				camera->PixelHeight(targetWindow->BackBufferSize().height);
			}

			mScene->Render(*frameTime, camera, commandBuffer.get(), backBufferIndex);

			// resolve or copy render target to target window
			if (targetWindow) {
				PROFILER_BEGIN("Resolve/Copy RenderTarget");
				camera->ColorBuffer(backBufferIndex)->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, commandBuffer.get());

				VkImageMemoryBarrier barrier = {};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.image = targetWindow->CurrentBackBuffer();
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.levelCount = 1;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = 1;
				barrier.srcAccessMask = 0;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				vkCmdPipelineBarrier(*commandBuffer,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
					0,
					0, nullptr,
					0, nullptr,
					1, &barrier
				);

				if (camera->SampleCount() == VK_SAMPLE_COUNT_1_BIT) {
					VkImageCopy region = {};
					region.extent = { camera->PixelWidth(), camera->PixelHeight(), 1 };
					region.dstSubresource.layerCount = 1;
					region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					region.srcSubresource.layerCount = 1;
					region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					vkCmdCopyImage(*commandBuffer, camera->ColorBuffer(backBufferIndex)->Image(camera->Device()), camera->ColorBuffer(backBufferIndex)->Layout(commandBuffer->Device()),
						targetWindow->CurrentBackBuffer(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
				} else {
					VkImageResolve region = {};
					region.extent = { camera->PixelWidth(), camera->PixelHeight(), 1 };
					region.dstSubresource.layerCount = 1;
					region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					region.srcSubresource.layerCount = 1;
					region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					vkCmdResolveImage(*commandBuffer, camera->ColorBuffer(backBufferIndex)->Image(camera->Device()), camera->ColorBuffer(backBufferIndex)->Layout(commandBuffer->Device()),
						targetWindow->CurrentBackBuffer(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
				}

				swap(barrier.oldLayout, barrier.newLayout);
				swap(barrier.srcAccessMask, barrier.dstAccessMask);
				vkCmdPipelineBarrier(*commandBuffer,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					0,
					0, nullptr,
					0, nullptr,
					1, &barrier
				);
				PROFILER_END;
			}
		}
		PROFILER_END;

		PROFILER_BEGIN("Execute CommandBuffers");
		for (auto& d : commandBuffers)
			fences.push_back(d.first->Execute(d.second));
		PROFILER_END;
	}

public:
	VkCAVE(const Configuration* config) : mScene(nullptr), mDeviceManager(nullptr)
#ifdef _DEBUG
		, mDebugMessenger(VK_NULL_HANDLE)
#endif
	{
		printf("Initializing...\n");
		mDeviceManager = new DeviceManager();
		mDeviceManager->CreateInstance();
		
		#ifdef _DEBUG
		if (config->mDebugMessenger) {
			VkDebugUtilsMessengerCreateInfoEXT dCreateInfo = {};
			dCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			dCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			dCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			dCreateInfo.pfnUserCallback = DebugCallback;
			printf("Creating debug messenger... ");
			VkResult result = CreateDebugUtilsMessengerEXT(mDeviceManager->Instance(), &dCreateInfo, nullptr, &mDebugMessenger);
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

		Input::Initialize();

		printf("Creating devices and displays... ");
		mDeviceManager->Initialize(displays);
		printf("Done.\n");

		mScene = new Scene(mDeviceManager);

		printf("Creating %d window cameras... ", (int)displays.size());
		for (uint32_t i = 0; i < displays.size(); i++) {
			auto w = mDeviceManager->GetWindow(i);
			auto camera = new Camera("Camera", w->Device(), w->Format().format);
			camera->LocalPosition(displays[i].mCameraPos);
			camera->LocalRotation(quat(radians(displays[i].mCameraRot)));
			camera->FieldOfView(radians(displays[i].mCameraFov));
			camera->Near(displays[i].mCameraNear);
			camera->Far(displays[i].mCameraFar);
			mScene->AddObject(camera);
			mCameraWindowMap.emplace(camera, w);
		}
		printf("Done.\n");
	}

	VkCAVE* Loop() {
		LoadPlugins();

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
			Render(&frameTime, mDeviceManager->GetWindow(0)->CurrentBackBufferIndex(), fences);

			// present windows
			PROFILER_BEGIN("Present");
			for (uint32_t i = 0; i < mDeviceManager->WindowCount(); i++)
				mDeviceManager->GetWindow(i)->Present(fences);
			PROFILER_END;
			
			Input::NextFrame();
			frameTime.mFrameNumber++;

			#ifdef PROFILER_ENABLE
			Profiler::FrameEnd();
			#endif
		}

		for (uint32_t i = 0; i < mDeviceManager->DeviceCount(); i++)
			mDeviceManager->GetDevice(i)->FlushCommandBuffers();

		UnloadPlugins();

		return this;
	}

	~VkCAVE() {
		for (auto& c : mCameraWindowMap)
			delete c.first;

		mScene->Clear();
		safe_delete(mScene);
		Input::Destroy();

		#ifdef _DEBUG
		if (mDebugMessenger != VK_NULL_HANDLE) DestroyDebugUtilsMessengerEXT(mDeviceManager->Instance(), mDebugMessenger, nullptr);
		#endif

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
	//_CrtDumpMemoryLeaks();
	#endif
	return EXIT_SUCCESS;
}