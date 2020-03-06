#include <Content/Texture.hpp>
#include <Core/Window.hpp>
#include <Core/Device.hpp>
#include <Core/Instance.hpp>
#include <Scene/Camera.hpp>
#include <Util/Profiler.hpp>
#include <Util/Util.hpp>

using namespace std;

#ifdef __linux
Window::Window(Instance* instance, const string& title, MouseKeyboardInput* input, VkRect2D position, xcb_connection_t* XCBConnection, xcb_screen_t* XCBScreen)
#else
Window::Window(Instance* instance, const string& title, MouseKeyboardInput* input, VkRect2D position, HINSTANCE hInstance)
#endif
	: mInstance(instance), mTargetCamera(nullptr), mDevice(nullptr), mTitle(title), mSwapchainSize({}), mFullscreen(false), mClientRect(position), mInput(input),
	mSwapchain(VK_NULL_HANDLE), mPhysicalDevice(VK_NULL_HANDLE), mImageCount(0), mFormat({}),
	mCurrentBackBufferIndex(0), mImageAvailableSemaphoreIndex(0), mFrameData(nullptr), mDirectDisplay(VK_NULL_HANDLE) {

	printf("Creating window... ");

	#ifdef __linux
	mXCBConnection = XCBConnection;
	mXCBScreen = XCBScreen;
	mXCBWindow = xcb_generate_id(mXCBConnection);

	uint32_t valueList[] {
		XCB_EVENT_MASK_BUTTON_PRESS		| XCB_EVENT_MASK_BUTTON_RELEASE |
		XCB_EVENT_MASK_KEY_PRESS   		| XCB_EVENT_MASK_KEY_RELEASE |
		XCB_EVENT_MASK_POINTER_MOTION	| XCB_EVENT_MASK_BUTTON_MOTION };
	xcb_create_window(
		mXCBConnection,
		XCB_COPY_FROM_PARENT,
		mXCBWindow,
		mXCBScreen->root,
		position.offset.x,
		position.offset.y,
		position.extent.width,
		position.extent.height,
		0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT,
		mXCBScreen->root_visual,
		XCB_CW_EVENT_MASK, valueList);

	xcb_change_property(
		mXCBConnection,
		XCB_PROP_MODE_REPLACE,
		mXCBWindow,
		XCB_ATOM_WM_NAME,
		XCB_ATOM_STRING,
		8,
		title.length(),
		title.c_str());

	xcb_intern_atom_cookie_t wmDeleteCookie = xcb_intern_atom(mXCBConnection, 0, strlen("WM_DELETE_WINDOW"), "WM_DELETE_WINDOW");
	xcb_intern_atom_cookie_t wmProtocolsCookie = xcb_intern_atom(mXCBConnection, 0, strlen("WM_PROTOCOLS"), "WM_PROTOCOLS");
	xcb_intern_atom_reply_t* wmDeleteReply = xcb_intern_atom_reply(mXCBConnection, wmDeleteCookie, NULL);
	xcb_intern_atom_reply_t* wmProtocolsReply = xcb_intern_atom_reply(mXCBConnection, wmProtocolsCookie, NULL);
	mXCBDeleteWin = wmDeleteReply->atom;
	mXCBProtocols = wmProtocolsReply->atom;
	xcb_change_property(mXCBConnection, XCB_PROP_MODE_REPLACE, mXCBWindow, wmProtocolsReply->atom, 4, 32, 1, &wmDeleteReply->atom);

	xcb_map_window(mXCBConnection, mXCBWindow);
	xcb_flush(mXCBConnection);

	VkXcbSurfaceCreateInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	info.connection = mXCBConnection;
	info.window = mXCBWindow;
	ThrowIfFailed(vkCreateXcbSurfaceKHR(*mInstance, &info, nullptr, &mSurface), "vkCreateXcbSurfaceKHR Failed");

	printf("Done\n");

	#else
	mWindowedRect = {};

	mHwnd = CreateWindowExA(
		NULL,
		"Stratum",
		mTitle.c_str(),
		WS_OVERLAPPEDWINDOW,
		position.offset.x,
		position.offset.y,
		position.extent.width,
		position.extent.height,
		NULL,
		NULL,
		hInstance,
		nullptr );
	if (!mHwnd) {
		fprintf_color(COLOR_RED, stderr, "Failed to create window\n");
		throw;
	}

	ShowWindow(mHwnd, SW_SHOW);

	VkWin32SurfaceCreateInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	info.hinstance = hInstance;
	info.hwnd = mHwnd;
	ThrowIfFailed(vkCreateWin32SurfaceKHR(*mInstance, &info, nullptr, &mSurface), "vkCreateWin32SurfaceKHR Failed");

	RECT cr;
	GetClientRect(mHwnd, &cr);
	mClientRect.offset = { (int32_t)cr.top, (int32_t)cr.left };
	mClientRect.extent = { (uint32_t)((int32_t)cr.right - (int32_t)cr.left), (uint32_t)((int32_t)cr.bottom - (int32_t)cr.top) };
	#endif
}
Window::~Window() {
	DestroySwapchain();
	vkDestroySurfaceKHR(*mInstance, mSurface, nullptr);

	if (mDirectDisplay){
		PFN_vkReleaseDisplayEXT vkReleaseDisplay = (PFN_vkReleaseDisplayEXT)vkGetInstanceProcAddr(*mInstance, "vkReleaseDisplayEXT");
		vkReleaseDisplay(mPhysicalDevice, mDirectDisplay);
	}
	
	#ifdef __linux
	if (mXCBConnection && mXCBWindow)
 		xcb_destroy_window(mXCBConnection, mXCBWindow);
 	#else
	DestroyWindow(mHwnd);
	#endif
}

VkImage Window::AcquireNextImage() {
	if (mSwapchain == VK_NULL_HANDLE) return VK_NULL_HANDLE;

	mImageAvailableSemaphoreIndex = (mImageAvailableSemaphoreIndex + 1) % (uint32_t)mImageAvailableSemaphores.size();
	VkResult err = vkAcquireNextImageKHR(*mDevice, mSwapchain, numeric_limits<uint64_t>::max(), *mImageAvailableSemaphores[mImageAvailableSemaphoreIndex], VK_NULL_HANDLE, &mCurrentBackBufferIndex);
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
		CreateSwapchain(mDevice);
		if (mSwapchain == VK_NULL_HANDLE) return VK_NULL_HANDLE;// swapchain failed to create (happens when window is minimized, etc)
		err = vkAcquireNextImageKHR(*mDevice, mSwapchain, numeric_limits<uint64_t>::max(), *mImageAvailableSemaphores[mImageAvailableSemaphoreIndex], VK_NULL_HANDLE, &mCurrentBackBufferIndex);
	}

	if (mFrameData == nullptr) return VK_NULL_HANDLE;
	return mFrameData[mCurrentBackBufferIndex].mSwapchainImage;
}

void Window::Present(vector<VkSemaphore> waitSemaphores) {
	if (mSwapchain == VK_NULL_HANDLE) return;

	waitSemaphores.push_back(*mImageAvailableSemaphores[mImageAvailableSemaphoreIndex]);

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = (uint32_t)waitSemaphores.size();
	presentInfo.pWaitSemaphores = waitSemaphores.data();
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &mSwapchain;
	presentInfo.pImageIndices = &mCurrentBackBufferIndex;
	vkQueuePresentKHR(mDevice->PresentQueue(), &presentInfo);
}

#ifdef __linux
xcb_atom_t getReplyAtomFromCookie(xcb_connection_t* connection, xcb_intern_atom_cookie_t cookie) {
	xcb_generic_error_t * error;
	xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection, cookie, &error);
	if (error) {
		printf("Can't set the screen. Error Code: %s", error->error_code);
		throw;
	}
	return reply->atom;
}
#endif

void Window::Fullscreen(bool fs) {
	#ifdef WINDOWS
	if (fs && !mFullscreen) {
		GetWindowRect(mHwnd, &mWindowedRect);

		UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
		SetWindowLongW(mHwnd, GWL_STYLE, windowStyle);

		HMONITOR hMonitor = MonitorFromWindow(mHwnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFOEX monitorInfo = {};
		monitorInfo.cbSize = sizeof(MONITORINFOEX);
		GetMonitorInfoA(hMonitor, &monitorInfo);

		SetWindowPos(mHwnd, HWND_TOPMOST,
			monitorInfo.rcMonitor.left,
			monitorInfo.rcMonitor.top,
			monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
			monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
			SWP_FRAMECHANGED | SWP_NOACTIVATE);

		ShowWindow(mHwnd, SW_MAXIMIZE);

		mFullscreen = true;
	} else if (!fs && mFullscreen) {
		SetWindowLongA(mHwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
		SetWindowPos(mHwnd, HWND_NOTOPMOST,
			mWindowedRect.left,
			mWindowedRect.top,
			mWindowedRect.right  - mWindowedRect.left,
			mWindowedRect.bottom - mWindowedRect.top,
			SWP_FRAMECHANGED | SWP_NOACTIVATE);
		ShowWindow(mHwnd, SW_NORMAL);

		mFullscreen = false;
	}
	#else

	if (fs == mFullscreen) return;
	mFullscreen = fs;

   	struct {
        unsigned long flags;
        unsigned long functions;
        unsigned long decorations;
        long input_mode;
        unsigned long status;
    } hints = {0};

    //hints.flags = MWM_HINTS_DECORATIONS;
    //hints.decorations = mFullscreen ? 0 : MWM_DECOR_ALL;

	//xcb_intern_atom_cookie_t cookie = xcb_intern_atom(mXCBConnection, 0, 16, "_MOTIF_WM_HINTS");
	//xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(mXCBConnection, cookie, NULL);
	//xcb_change_property(mXCBConnection, XCB_PROP_MODE_REPLACE, mXCBWindow, reply->atom, reply->atom, 32, sizeof(hints), &hints);

	#endif
}

void Window::CreateSwapchain(::Device* device) {
	if (mSwapchain) DestroySwapchain();
	mDevice = device;
	mDevice->SetObjectName(mSurface, mTitle + " Surface", VK_OBJECT_TYPE_SURFACE_KHR);
	if (!mPhysicalDevice) mPhysicalDevice = mDevice->PhysicalDevice();

	#pragma region create swapchain
	// query support
	VkSurfaceCapabilitiesKHR capabilities;
	vector<VkSurfaceFormatKHR> formats;
	vector<VkPresentModeKHR> presentModes;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &capabilities);
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, nullptr);
	if (formatCount != 0) {
		formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(mDevice->PhysicalDevice(), mSurface, &formatCount, formats.data());
	}
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &presentModeCount, nullptr);
	if (presentModeCount != 0) {
		presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(mDevice->PhysicalDevice(), mSurface, &presentModeCount, presentModes.data());
	}
	uint32_t graphicsFamily, presentFamily;
	if (!Device::FindQueueFamilies(mPhysicalDevice, mSurface, graphicsFamily, presentFamily)) {
		fprintf_color(COLOR_RED, stderr, "Failed to find queue families\n");
		throw;
	}
	uint32_t queueFamilyIndices[] = { graphicsFamily, presentFamily };

	// get the size of the swapchain
	mSwapchainSize = capabilities.currentExtent;
	if (mSwapchainSize.width == numeric_limits<uint32_t>::max() || mSwapchainSize.height == numeric_limits<uint32_t>::max() || mSwapchainSize.width == 0 || mSwapchainSize.height == 0)
		return;

	// find a preferrable surface format 
	mFormat = formats[0];
	for (const auto& availableFormat : formats)
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM) {
			mFormat = availableFormat;
			break;
		}

	// find the best present mode
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (const auto& availablePresentMode : presentModes)
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			presentMode = availablePresentMode;
			break;
		} else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
			presentMode = availablePresentMode;

	// find the preferrable number of back buffers
	mImageCount = capabilities.minImageCount + 1;

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = mSurface;
	createInfo.minImageCount = mImageCount;
	createInfo.imageFormat = mFormat.format;
	createInfo.imageColorSpace = mFormat.colorSpace;
	createInfo.imageExtent = mSwapchainSize;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if (graphicsFamily != presentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	} else
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.preTransform = capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
	};
	for (auto& compositeAlphaFlag : compositeAlphaFlags) {
		if (capabilities.supportedCompositeAlpha & compositeAlphaFlag) {
			createInfo.compositeAlpha = compositeAlphaFlag;
			break;
		};
	}
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	
	VkBool32 sfcSupport;
	vkGetPhysicalDeviceSurfaceSupportKHR(mPhysicalDevice, presentFamily, mSurface, &sfcSupport);
	if (!sfcSupport) {
		fprintf_color(COLOR_RED, stderr, "Surface not supported by device!");
		throw;
	}

	ThrowIfFailed(vkCreateSwapchainKHR(*mDevice, &createInfo, nullptr, &mSwapchain), "vkCreateSwapchainKHR failed");
	mDevice->SetObjectName(mSwapchain, mTitle + " Swapchain", VK_OBJECT_TYPE_SWAPCHAIN_KHR);
	#pragma endregion

	// get the back buffers
	vector<VkImage> images;
	vkGetSwapchainImagesKHR(*mDevice, mSwapchain, &mImageCount, nullptr);
	images.resize(mImageCount);
	vkGetSwapchainImagesKHR(*mDevice, mSwapchain, &mImageCount, images.data());

	mFrameData = new FrameData[mImageCount];

	mImageAvailableSemaphores.resize(mImageCount);
	mImageAvailableSemaphoreIndex = 0;

	auto commandBuffer = device->GetCommandBuffer();

	vector<VkImageMemoryBarrier> barriers(mImageCount);

	// create per-frame objects
	for (uint32_t i = 0; i < mImageCount; i++) {
		mFrameData[i] = FrameData();
		mFrameData[i].mSwapchainImage = images[i];
		mDevice->SetObjectName(images[i], mTitle + " Image " + to_string(i), VK_OBJECT_TYPE_IMAGE);

		barriers[i] = {};
		barriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barriers[i].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barriers[i].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[i].image = mFrameData[i].mSwapchainImage;
		barriers[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barriers[i].subresourceRange.levelCount = 1;
		barriers[i].subresourceRange.layerCount = 1;

		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = mFrameData[i].mSwapchainImage;
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = mFormat.format;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;
		ThrowIfFailed(vkCreateImageView(*mDevice, &createInfo, nullptr, &mFrameData[i].mSwapchainImageView), "vkCreateImageView failed for swapchain");
		mDevice->SetObjectName(mFrameData[i].mSwapchainImageView, mTitle + " Image View " + to_string(i), VK_OBJECT_TYPE_IMAGE_VIEW);
		
		mImageAvailableSemaphores[i] = make_shared<Semaphore>(mDevice);
		mDevice->SetObjectName(mImageAvailableSemaphores[i]->operator VkSemaphore(), mTitle + " Image Avaiable Semaphore " + to_string(i), VK_OBJECT_TYPE_SEMAPHORE);
	}
	vkCmdPipelineBarrier(*commandBuffer,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		0,
		0, nullptr,
		0, nullptr,
		(uint32_t)barriers.size(), barriers.data()
	);

	device->Execute(commandBuffer, false)->Wait();
}

void Window::DestroySwapchain() {
	mDevice->Flush();

	for (uint32_t i = 0; i < mImageCount; i++) {
		if (mFrameData[i].mSwapchainImageView != VK_NULL_HANDLE)
			vkDestroyImageView(*mDevice, mFrameData[i].mSwapchainImageView, nullptr);
		mFrameData[i].mSwapchainImageView = VK_NULL_HANDLE;
	}
	mImageAvailableSemaphores.clear();

	if (mSwapchain != VK_NULL_HANDLE)
		vkDestroySwapchainKHR(*mDevice, mSwapchain, nullptr);
	safe_delete_array(mFrameData);
	mSwapchain = VK_NULL_HANDLE;
}