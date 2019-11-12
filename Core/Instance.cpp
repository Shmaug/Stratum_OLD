#include <Core/Instance.hpp>
#include <Core/Device.hpp>
#include <Core/Window.hpp>
#include <Util/Profiler.hpp>

using namespace std;

Instance::Instance() : mInstance(VK_NULL_HANDLE), mGLFWInitialized(false), mFrameCount(0), mMaxFramesInFlight(0), mTotalTime(0), mDeltaTime(0) {}
Instance::~Instance() {
	if (mGLFWInitialized) glfwTerminate();

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

void Instance::CreateInstance() {
	if (mInstance != VK_NULL_HANDLE)
		vkDestroyInstance(mInstance, nullptr);

	if (!mGLFWInitialized) {
		if (glfwInit() == GLFW_FALSE) {
			const char* msg;
			glfwGetError(&msg);
			printf("Failed to initialize GLFW! %s\n", msg);
			throw runtime_error("Failed to initialize GLFW!");
		}
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		mGLFWInitialized = true;
		printf("Initialized glfw.\n");
	}

	vector<const char*> instanceExtensions{
		#ifdef ENABLE_DEBUG_LAYERS
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
		#endif
		//VK_KHR_SURFACE_EXTENSION_NAME,
		//VK_KHR_DISPLAY_EXTENSION_NAME,
	};

	// request GLFW extensions
	uint32_t glfwExtensionCount = 0;
	printf("Requesting glfw extensions... ");
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	printf("Done.\n");
	for (uint32_t i = 0; i < glfwExtensionCount; i++)
		instanceExtensions.push_back(glfwExtensions[i]);

	vector<const char*> validationLayers{
		#ifdef ENABLE_DEBUG_LAYERS
		"VK_LAYER_KHRONOS_validation",
		"VK_LAYER_LUNARG_core_validation",
		"VK_LAYER_LUNARG_standard_validation",
		//"VK_LAYER_RENDERDOC_Capture"
		#endif
	};

#ifdef ENABLE_DEBUG_LAYERS
	printf("Initializing with validation layers\n");
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
	appInfo.pApplicationName = "VkCAVE";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_1;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
	createInfo.ppEnabledExtensionNames = instanceExtensions.data();
	createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
	createInfo.ppEnabledLayerNames = validationLayers.data();
	printf("Creating vulkan instance... ");
	ThrowIfFailed(vkCreateInstance(&createInfo, nullptr, &mInstance), "vkCreateInstance failed");
	printf("Done.\n");
}
void Instance::CreateDevicesAndWindows(const vector<DisplayCreateInfo>& displays) {
	vector<const char*> deviceExtensions{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME, // needed to obtain a swapchain
	};
	vector<const char*> validationLayers{
		#ifdef ENABLE_DEBUG_LAYERS
		"VK_LAYER_KHRONOS_validation",
		"VK_LAYER_RENDERDOC_Capture",
		#endif
	};

	MouseKeyboardInput* windowInput = new MouseKeyboardInput();

	mMaxFramesInFlight = 0xFFFF;

	// create windows
	for (const auto& it : displays) {
		uint32_t deviceIndex = it.mDevice;
		VkPhysicalDevice physicalDevice = GetPhysicalDevice(deviceIndex, deviceExtensions);
		while (physicalDevice == VK_NULL_HANDLE) {
			// requested device index does not exist or is not suitable, fallback to previous device if it exists
			if (deviceIndex == 0) throw runtime_error("Failed to find suitable device!");
			deviceIndex--;
			physicalDevice = GetPhysicalDevice(deviceIndex, deviceExtensions);
		}
		if (physicalDevice == VK_NULL_HANDLE) continue; // could not find any suitable devices...

		if ((uint32_t)mDevices.size() <= deviceIndex) mDevices.resize((size_t)deviceIndex + 1);

		auto w = new Window(this, "VkCave " + to_string(mWindows.size()), windowInput, it.mWindowPosition, it.mMonitor);
		if (!mDevices[deviceIndex]) {
			uint32_t gq, pq;
			Device::FindQueueFamilies(physicalDevice, w->Surface(), gq, pq);
			mDevices[deviceIndex] = new Device(this, physicalDevice, deviceIndex, gq, pq, deviceExtensions, validationLayers);
		}
		
		w->CreateSwapchain(mDevices[deviceIndex]);
		mWindows.push_back(w);
		mMaxFramesInFlight = min(mMaxFramesInFlight, w->mImageCount);
	}

	for (Device* d : mDevices)
		d->mFrameContexts.resize(mMaxFramesInFlight);

	mStartTime = mClock.now();
	mLastFrame = mClock.now();
}

bool Instance::PollEvents() {
	chrono::time_point<chrono::steady_clock> t1 = mClock.now();
	mDeltaTime = (t1 - mLastFrame).count() * 1e-9f;
	mTotalTime = (t1 - mStartTime).count() * 1e-9f;
	mLastFrame = t1;

	for (const auto& w : mWindows)
		if (w->ShouldClose())
			return false;
	glfwPollEvents();
	return true;
}

void Instance::AdvanceFrame() {

	for (Window* w : mWindows)
		// will wait on the semaphores signalled by the frame mMaxFramesInFlight ago
		w->Present();

	mFrameCount++;
	PROFILER_BEGIN("Wait for GPU");
	for (uint32_t i = 0; i < mDevices.size(); i++) {
		mDevices[i]->mFrameContextIndex = mFrameCount % mMaxFramesInFlight;
		mDevices[i]->CurrentFrameContext()->Reset();
	}
	PROFILER_END;
}