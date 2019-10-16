#include <Core/DeviceManager.hpp>
#include <Util/Util.hpp>

using namespace std;

DeviceManager::DeviceManager()
	: mInstance(VK_NULL_HANDLE), mGLFWInitialized(false), mMaxFramesInFlight(0) {}
DeviceManager::~DeviceManager() {
	if (mGLFWInitialized) glfwTerminate();

	for (auto& w : mWindows)
		safe_delete(w);

	for (auto& d : mDevices) {
		d->FlushCommandBuffers();
		safe_delete(d);
	}

	vkDestroyInstance(mInstance, nullptr);
}

VkPhysicalDevice DeviceManager::GetPhysicalDevice(uint32_t index, const vector<const char*>& extensions) const {
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

void DeviceManager::CreateInstance() {
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

	vector<const char*> instanceExtensions {
		#ifdef ENABLE_DEBUG_LAYERS
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
		#endif
	};
	
	// request GLFW extensions
	uint32_t glfwExtensionCount = 0;
	printf("Requesting glfw extensions... ");
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	printf("Done.\n");
	for (uint32_t i = 0; i < glfwExtensionCount; i++)
		instanceExtensions.push_back(glfwExtensions[i]);

	vector<const char*> validationLayers {
		#ifdef ENABLE_DEBUG_LAYERS
		"VK_LAYER_KHRONOS_validation",
		"VK_LAYER_LUNARG_standard_validation",
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

void DeviceManager::Initialize(const vector<DisplayCreateInfo>& displays) {
	vector<const char*> deviceExtensions {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME, // needed to obtain a swapchain
	};

	vector<const char*> validationLayers {
		#ifdef ENABLE_DEBUG_LAYERS
		"VK_LAYER_KHRONOS_validation",
		"VK_LAYER_RENDERDOC_Capture",
		#endif
	};

	uint32_t minImageCount = ~0;

	MouseKeyboardInput* windowInput = new MouseKeyboardInput();

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

		auto w = new Window(mInstance, "VkCave " + to_string(mWindows.size()), windowInput, it.mWindowPosition, it.mMonitor);
		if (!mDevices[deviceIndex]) mDevices[deviceIndex] = new Device(mInstance, deviceExtensions, validationLayers, w->Surface(), physicalDevice, deviceIndex);
		w->CreateSwapchain(mDevices[deviceIndex]);
		minImageCount = min(minImageCount, w->mImageCount);
		mWindows.push_back(w);
	}

	for (const auto& device : mDevices)
		device->mMaxFramesInFlight = minImageCount;
}

bool DeviceManager::PollEvents() const {
	for (const auto& w : mWindows)
		if (w->ShouldClose())
			return false;
	glfwPollEvents();
	return true;
}

void DeviceManager::PresentWindows(const vector<shared_ptr<Fence>>& fences){
	for (Window* w : mWindows)
		w->Present(fences);
}