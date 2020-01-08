#include <Core/Instance.hpp>
#include <Core/Device.hpp>
#include <Core/Window.hpp>
#include <Util/Profiler.hpp>

using namespace std;

Instance::Instance() : mInstance(VK_NULL_HANDLE), mFrameCount(0), mMaxFramesInFlight(0), mTotalTime(0), mDeltaTime(0) {
	set<string> instanceExtensions;
	#ifdef ENABLE_DEBUG_LAYERS
	instanceExtensions.insert(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	#endif

	instanceExtensions.insert(VK_KHR_SURFACE_EXTENSION_NAME);
	instanceExtensions.insert(VK_KHR_DISPLAY_EXTENSION_NAME);
	#ifdef __linux
	instanceExtensions.insert(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
	#endif

	vector<const char*> validationLayers;
	#ifdef ENABLE_DEBUG_LAYERS
	validationLayers.push_back("VK_LAYER_KHRONOS_validation");
	validationLayers.push_back("VK_LAYER_LUNARG_core_validation");
	validationLayers.push_back("VK_LAYER_LUNARG_standard_validation");
	//validationLayers.push_back("VK_LAYER_RENDERDOC_Capture");
	#endif
	if (validationLayers.size()) {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		set<string> availableLayerSet;
		for (const VkLayerProperties& layer : availableLayers)
			availableLayerSet.insert(layer.layerName);

		for (auto it = validationLayers.begin(); it != validationLayers.end();) {
			if (availableLayerSet.count(*it))
				it++;
			else {
				printf("Removing unsupported layer: %s\n", *it);
				it = validationLayers.erase(it);
			}
		}
	}

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Stratum";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Stratum";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_1;

	vector<const char*> exts;
	for (const string& s : instanceExtensions)
		exts.push_back(s.c_str());

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = (uint32_t)exts.size();
	createInfo.ppEnabledExtensionNames = exts.data();
	createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
	createInfo.ppEnabledLayerNames = validationLayers.data();
	printf("Creating vulkan instance... ");
	ThrowIfFailed(vkCreateInstance(&createInfo, nullptr, &mInstance), "vkCreateInstance failed");
	printf("Done.\n");
}
Instance::~Instance() {
	#ifdef __linux
	for (auto& c : mXCBConnections)
		xcb_disconnect(c.second);
	#endif

	for (Window* w : mWindows)
		safe_delete(w);

	for (Device* d : mDevices)
		safe_delete(d);

	vkDestroyInstance(mInstance, nullptr);
}

VkPhysicalDevice Instance::GetPhysicalDevice(uint32_t index, const vector<const char*>& extensions) const {
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
	if (deviceCount == 0) return VK_NULL_HANDLE;
	vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());

	if (index >= deviceCount) return VK_NULL_HANDLE;

	// 'index' represents the index of the SUITABLE devices, count the suitable devices
	uint32_t i = 0;
	for (const auto& device : devices) {
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
		vector<VkExtensionProperties> availableExtensions(extensionCount);
		if (extensionCount == 0) continue;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
		set<string> availableExtensionSet;
		for (const VkExtensionProperties& e : availableExtensions)
			availableExtensionSet.insert(e.extensionName);
		bool hasExtensions = true;
		for (auto& e : extensions)
			if (availableExtensionSet.count(e) == 0) {
				hasExtensions = false;
				break;
			}
		if (hasExtensions) {
			if (i == index) return device;
			i++;
		}
	}
	return VK_NULL_HANDLE;
}

void Instance::CreateDevicesAndWindows(const vector<DisplayCreateInfo>& displays) {
	vector<const char*> deviceExtensions {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};
	vector<const char*> validationLayers;
	#ifdef ENABLE_DEBUG_LAYERS
	validationLayers.push_back("VK_LAYER_KHRONOS_validation");
	validationLayers.push_back("VK_LAYER_LUNARG_core_validation");
	validationLayers.push_back("VK_LAYER_LUNARG_standard_validation");
	validationLayers.push_back("VK_LAYER_RENDERDOC_Capture");
	#endif

	MouseKeyboardInput* windowInput = new MouseKeyboardInput();

	mMaxFramesInFlight = 0xFFFF;

	// create windows
	for (auto& it : displays) {
		uint32_t deviceIndex = it.mDevice;
		VkPhysicalDevice physicalDevice = GetPhysicalDevice(deviceIndex, deviceExtensions);
		while (physicalDevice == VK_NULL_HANDLE) {
			// requested device index does not exist or is not suitable, fallback to previous device if it exists
			if (deviceIndex == 0) {
				fprintf_color(BoldRed, stderr, "Failed to find suitable device!\n");
				throw;
			}
			deviceIndex--;
			physicalDevice = GetPhysicalDevice(deviceIndex, deviceExtensions);
		}
		if (physicalDevice == VK_NULL_HANDLE) continue; // could not find any suitable devices...

		if ((uint32_t)mDevices.size() <= deviceIndex) mDevices.resize((size_t)deviceIndex + 1);

		#ifdef __linux
		int screenp = 0;
		xcb_connection_t* conn = nullptr;
		if (!mXCBConnections.count(it.mXDisplay)) {
			conn = xcb_connect(it.mXDisplay.data(), &screenp);
			mXCBConnections.emplace(it.mXDisplay, conn);
			if (int err = xcb_connection_has_error(conn)) {
				mXCBConnections.erase(it.mXDisplay);
				fprintf_color(Red, stderr, "xcb_connect failed: %d\n", err);
				throw;
			}
		}
		Window* w = new Window(this, "Stratum " + to_string(mWindows.size()), windowInput, it.mWindowPosition, conn, it.mXScreen >= 0 ? it.mXScreen : screenp);
		#else
		static_assert(false, "Not implemented!");
		#endif

		if (!mDevices[deviceIndex]) {
			uint32_t gq, pq;
			Device::FindQueueFamilies(physicalDevice, w->Surface(), gq, pq);
			mDevices[deviceIndex] = new Device(this, physicalDevice, deviceIndex, gq, pq, deviceExtensions, validationLayers);
		}
		mDevices[deviceIndex]->mWindowCount++;
		
		w->CreateSwapchain(mDevices[deviceIndex]);
		mWindows.push_back(w);
		mMaxFramesInFlight = min(mMaxFramesInFlight, w->mImageCount);
	}

	for (Device* d : mDevices){
		d->mFrameContexts.resize(mMaxFramesInFlight);
		for (uint32_t i = 0; i < mMaxFramesInFlight; i++)
			d->mFrameContexts[i].mSemaphores.resize(d->mWindowCount);
	}

	mStartTime = mClock.now();
	mLastFrame = mClock.now();
}

bool Instance::PollEvents() {
    auto t1 = mClock.now();
	mDeltaTime = (t1 - mLastFrame).count() * 1e-9f;
	mTotalTime = (t1 - mStartTime).count() * 1e-9f;
	mLastFrame = t1;

	#if __linux
	for (auto& c : mXCBConnections) {
		xcb_generic_event_t* event = xcb_poll_for_event(c.second);
		if (!event) return true;

		xcb_client_message_event_t* cm = (xcb_client_message_event_t*)event;

		switch (event->response_type & ~0x80) {
		case XCB_CLIENT_MESSAGE: {
			for (const auto& w : mWindows)
				if (w->mXCBConnection == c.second && cm->data.data32[0] == w->mXCBDeleteWin)
					return false;
			break;
		}
		}

		free(event);
	}
	#else
	static_assert(false, "Not implemented!");
	#endif

	return true;
}

void Instance::AdvanceFrame() {
	uint32_t wi = 0;
	for (Window* w : mWindows) {
		vector<VkSemaphore> waitSemaphores;
		for (const shared_ptr<Semaphore>& s : w->Device()->CurrentFrameContext()->mSemaphores[wi])
			waitSemaphores.push_back(*s);
		// will wait on the semaphores signalled by the frame mMaxFramesInFlight ago
		w->Present(waitSemaphores);
		wi++;
	}

	mFrameCount++;

	PROFILER_BEGIN("Wait for GPU");
	for (uint32_t i = 0; i < mDevices.size(); i++) {
		mDevices[i]->mFrameContextIndex = mFrameCount % mMaxFramesInFlight;
		mDevices[i]->CurrentFrameContext()->Reset();
	}
	PROFILER_END;
}