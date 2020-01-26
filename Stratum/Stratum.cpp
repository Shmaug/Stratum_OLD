#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>
#include <unordered_map>

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
		fprintf_color(COLOR_RED_BOLD, stderr, "%s: %s\n", pCallbackData->pMessageIdName, pCallbackData->pMessage);
		throw;
	} else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		if (strcmp("UNASSIGNED-CoreValidation-Shader-OutputNotConsumed", pCallbackData->pMessageIdName) == 0) return VK_FALSE;
		if (strcmp("UNASSIGNED-CoreValidation-DrawState-ClearCmdBeforeDraw", pCallbackData->pMessageIdName) == 0) return VK_FALSE;
		fprintf_color(COLOR_YELLOW_BOLD, stderr, "%s: %s\n", pCallbackData->pMessageIdName, pCallbackData->pMessage);
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
		PROFILER_BEGIN("Get CommandBuffers");
		shared_ptr<CommandBuffer> commandBuffer = mScene->Instance()->Device()->GetCommandBuffer();
		PROFILER_END;

		mScene->PreFrame(commandBuffer.get());

		PROFILER_BEGIN("Render Cameras");
		for (const auto& camera : mScene->Cameras())
			if (camera->EnabledHierarchy())
				mScene->Render(commandBuffer.get(), camera, camera->Framebuffer(), PASS_MAIN);

		for (const auto& camera : mScene->Cameras())
			if (camera->EnabledHierarchy())
				camera->Resolve(commandBuffer.get());

		for (const auto& camera : mScene->Cameras())
			if (camera->EnabledHierarchy()) {
				PROFILER_BEGIN("Plugin PostProcess");
				for (const auto& p : mPluginManager->Plugins())
					if (p->mEnabled) p->PostProcess(commandBuffer.get(), camera);
				PROFILER_END;
			}
		for (const auto& camera : mScene->Cameras())
			if (camera->TargetWindow() && camera->EnabledHierarchy()) {
				Texture* src = camera->ResolveBuffer();
				src->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, commandBuffer.get());
				Texture::TransitionImageLayout(camera->TargetWindow()->BackBuffer(), camera->TargetWindow()->Format().format, 1, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandBuffer.get());
				VkImageCopy rgn = {};
				rgn.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				rgn.srcSubresource.layerCount = 1;
				rgn.extent = { src->Width(), src->Height(), 1 };
				rgn.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				rgn.dstSubresource.layerCount = 1;
				vkCmdCopyImage(*commandBuffer,
					src->Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					camera->TargetWindow()->BackBuffer(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1, &rgn);
				Texture::TransitionImageLayout(camera->TargetWindow()->BackBuffer(), camera->TargetWindow()->Format().format, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, commandBuffer.get());
				src->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, commandBuffer.get());
			}

		for (const auto& camera : mScene->Cameras())
			if (camera->EnabledHierarchy())
				camera->PostRender(commandBuffer.get());
		PROFILER_END;

		PROFILER_BEGIN("Execute CommandBuffer");
		mInstance->Device()->Execute(commandBuffer);
		PROFILER_END;
	}

public:
	Stratum(const Instance::DisplayCreateInfo& display, bool debugMessenger) : mScene(nullptr), mInstance(nullptr), mInputManager(nullptr)
#ifdef ENABLE_DEBUG_LAYERS
		, mDebugMessenger(VK_NULL_HANDLE)
#endif
	{
		printf("Initializing...\n");
		mInstance = new Instance(display);
		mInputManager = new InputManager();
		mPluginManager = new PluginManager();
		mAssetManager = new AssetManager(mInstance->Device());
		
		#ifdef ENABLE_DEBUG_LAYERS
		if (debugMessenger) {
			VkDebugUtilsMessengerCreateInfoEXT msgr = {};
			msgr.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			msgr.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			msgr.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			msgr.pfnUserCallback = DebugCallback;
			printf("Creating debug messenger... ");
			VkResult result = CreateDebugUtilsMessengerEXT(*mInstance, &msgr, nullptr, &mDebugMessenger);
			if (result == VK_SUCCESS)
				fprintf_color(COLOR_GREEN, stdout, "Success.\n");
			else {
				fprintf_color(COLOR_RED, stderr, "Failed.\n");
				mDebugMessenger = VK_NULL_HANDLE;
			}
		}
		#endif

		printf("Initialized.\n");

		mScene = new Scene(mInstance, mAssetManager, mInputManager, mPluginManager);
		mInputManager->RegisterInputDevice(mInstance->Window()->mInput);
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

			PROFILER_BEGIN("Acquire Image");
			mInstance->Window()->AcquireNextImage();
			PROFILER_END;

			mScene->Update();
			Render();

			mInstance->AdvanceFrame();

			#ifdef PROFILER_ENABLE
			Profiler::FrameEnd();
			#endif
		}

		mInstance->Device()->FlushFrames();

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

int main(int argc, char* argv[]) {
	{ 
		#ifdef WINDOWS
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
			cerr << "WSAStartup failed" << endl;
		#endif

		Instance::DisplayCreateInfo display = {};
		display.mDeviceIndex = 0;
		display.mWindowPosition = { { 160, 90 }, { 1600, 900 }};
		display.mXDisplay = "";
		bool debugMessenger = true;
		
		for (int i = 1; i < argc; i++){
			if (strcmp(argv[i], "-nodebug") == 0) debugMessenger = false;
		}

		// create, run, and delete the engine all in one line :)
		delete (new Stratum(display, debugMessenger))->Loop();

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