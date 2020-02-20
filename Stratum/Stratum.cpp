﻿#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>
#include <unordered_map>

#include <Core/Instance.hpp>
#include <Core/PluginManager.hpp>
#include <Input/InputManager.hpp>
#include <Scene/GUI.hpp>
#include <Scene/Scene.hpp>
#include <ThirdParty/json11.h>
#include <Util/Util.hpp>
#include <Util/Profiler.hpp>

#include <Util/Util.hpp>

using namespace std;

class Stratum {
private:
	Instance* mInstance;
	InputManager* mInputManager;
	PluginManager* mPluginManager;
	AssetManager* mAssetManager;
	Scene* mScene;

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
	Stratum(int argc, char** argv) : mScene(nullptr), mInstance(nullptr), mInputManager(nullptr)
	{
		printf("Initializing...\n");
		mPluginManager = new PluginManager();
		mInstance = new Instance(argc, argv, mPluginManager);
		mInputManager = new InputManager();
		mAssetManager = new AssetManager(mInstance->Device());
		printf("Initialized.\n");

		mScene = new Scene(mInstance, mAssetManager, mInputManager, mPluginManager);
		Gizmos::Initialize(mInstance->Device(), mAssetManager);
		GUI::Initialize(mInstance->Device(), mAssetManager);
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
		GUI::Destroy(mInstance->Device());
		Gizmos::Destroy(mInstance->Device());
		safe_delete(mScene);

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

		// create, run, and delete the engine all in one line :)
		delete (new Stratum(argc, argv))->Loop();

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