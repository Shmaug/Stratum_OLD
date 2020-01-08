#include <Content/Texture.hpp>
#include <Core/Window.hpp>
#include <Core/Device.hpp>
#include <Core/Instance.hpp>
#include <Input/MouseKeyboardInput.hpp>
#include <Scene/Camera.hpp>
#include <Util/Profiler.hpp>
#include <Util/Util.hpp>

using namespace std;

#ifdef __linux
xcb_connection_t* XCBConnection = nullptr;
#endif

void Window::WindowPosCallback(GLFWwindow* window, int x, int y) {
	Window* win = (Window*)glfwGetWindowUserPointer(window);
	glfwGetWindowPos(window, &win->mClientRect.offset.x, &win->mClientRect.offset.y);
	glfwGetWindowSize(window, (int*)&win->mClientRect.extent.width, (int*)&win->mClientRect.extent.height);
}
void Window::FramebufferResizeCallback(GLFWwindow* window, int width, int height) {
	Window* win = (Window*)glfwGetWindowUserPointer(window);
	if (width > 0 && height > 0) {
		glfwGetWindowPos(window, &win->mClientRect.offset.x, &win->mClientRect.offset.y);
		glfwGetWindowSize(window, (int*)&win->mClientRect.extent.width, (int*)&win->mClientRect.extent.height);
		if (win->mDevice) win->CreateSwapchain(win->mDevice);
	}
}
void Window::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (action == GLFW_REPEAT) return;
	Window* win = (Window*)glfwGetWindowUserPointer(window);

	win->mInput->mLastWindow = win;
	win->mInput->mCurrent.mKeys[key] = action;
	if (win && win->mInput->mCurrent.mKeys[GLFW_KEY_LEFT_ALT] == GLFW_PRESS && win->mInput->mCurrent.mKeys[GLFW_KEY_ENTER] == GLFW_PRESS && (key == GLFW_KEY_ENTER || key == GLFW_KEY_LEFT_ALT))
		win->Fullscreen(!win->Fullscreen());
}
void Window::CursorPosCallback(GLFWwindow* window, double x, double y) {
	Window* win = (Window*)glfwGetWindowUserPointer(window);
	win->mInput->mLastWindow = win;
	win->mInput->mCurrent.mCursorPos = float2((float)x, (float)y) + float2((float)win->mClientRect.offset.x, (float)win->mClientRect.offset.y);
	win->mInput->mCurrent.mCursorDelta = win->mInput->mCurrent.mCursorPos - win->mInput->mLast.mCursorPos;

	if (win->mTargetCamera) {
		float2 uv = float2((float)x, (float)y) / float2((float)win->ClientRect().extent.width, (float)win->ClientRect().extent.height);
		win->mInput->mMousePointer.mWorldRay = win->mTargetCamera->ScreenToWorldRay(uv);
	}
}
void Window::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	Window* win = (Window*)glfwGetWindowUserPointer(window);
	win->mInput->mMousePointer.mAxis[(uint32_t)button] = action == GLFW_PRESS;
	win->mInput->mLastWindow = win;
}
void Window::ScrollCallback(GLFWwindow* window, double x, double y) {
	Window* win = (Window*)glfwGetWindowUserPointer(window);
	win->mInput->mLastWindow = win;
	win->mInput->mCurrent.mScrollDelta += float2((float)x, (float)y);
}

Window::Window(Instance* instance, const string& title, MouseKeyboardInput* input, VkRect2D position)
	: mInstance(instance), mTargetCamera(nullptr), mDevice(nullptr), mTitle(title), mSwapchainSize({}), mFullscreen(false), mClientRect(position), mWindowedRect({}), mInput(input),
	mSwapchain(VK_NULL_HANDLE), mImageCount(0), mFormat({}), mPhysicalDevice(VK_NULL_HANDLE),
	mCurrentBackBufferIndex(0), mImageAvailableSemaphoreIndex(0), mFrameData(nullptr)
	#ifdef __linux
	,mXCBScreen(nullptr),
	mXCBWindow(0),
	mXCBProtocols(0),
 	mXCBDeleteWin(0)
	#endif
	{

	if (position.extent.width == 0 || position.extent.height == 0) {
		position.extent.width = 1600;
		position.extent.height = 900;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	mGLFWWindow = glfwCreateWindow(position.extent.width, position.extent.height, mTitle.c_str(), nullptr, nullptr);
	if (mGLFWWindow == nullptr) {
		const char* msg;
		if (int c = glfwGetError(&msg)) {
			fprintf_color(Red, stderr, "Failed to create GLFW window (%d): %s\n", c, msg);
			throw;
		}
	}
	glfwSetWindowUserPointer(mGLFWWindow, this);

	glfwSetWindowPos(mGLFWWindow, position.offset.x, position.offset.y);

	if (glfwRawMouseMotionSupported() == GLFW_TRUE)
		glfwSetInputMode(mGLFWWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

	glfwSetFramebufferSizeCallback(mGLFWWindow, FramebufferResizeCallback);
	glfwSetWindowPosCallback(mGLFWWindow, WindowPosCallback);
	glfwSetKeyCallback(mGLFWWindow, KeyCallback);
	glfwSetCursorPosCallback(mGLFWWindow, CursorPosCallback);
	glfwSetScrollCallback(mGLFWWindow, ScrollCallback);
	glfwSetMouseButtonCallback(mGLFWWindow, MouseButtonCallback);

	if (glfwCreateWindowSurface(*instance, mGLFWWindow, nullptr, &mSurface) != VK_SUCCESS) {
		const char* msg;
		if (int c = glfwGetError(&msg)) {
			fprintf_color(Red, stderr, "Failed to create GLFW window surface (%d): %s\n", c, msg);
			throw;
		}
	}

	glfwGetWindowPos(mGLFWWindow, &mClientRect.offset.x, &mClientRect.offset.y);
	glfwGetWindowSize(mGLFWWindow, (int*)&mClientRect.extent.width, (int*)&mClientRect.extent.height);
	mWindowedRect = mClientRect;
}
Window::Window(Instance* instance, const string& title, VkRect2D position, uint32_t displayIndex)
	: mInstance(instance), mTargetCamera(nullptr), mDevice(nullptr), mTitle(title), mSwapchainSize({}), mFullscreen(false), mClientRect(position), mWindowedRect({}), mInput(nullptr),
	mSwapchain(VK_NULL_HANDLE), mImageCount(0), mFormat({}), mGLFWWindow(nullptr), 
	mCurrentBackBufferIndex(0), mImageAvailableSemaphoreIndex(0), mFrameData(nullptr)
	#ifdef __linux
	,mXCBScreen(nullptr),
	mXCBWindow(0),
	mXCBProtocols(0),
 	mXCBDeleteWin(0)
	#endif
	{

	#ifdef __linux

	int screenp = 0;
	if (!XCBConnection) {
		XCBConnection = xcb_connect(nullptr, &screenp);
		if (int err = xcb_connection_has_error(XCBConnection)) {
			XCBConnection = nullptr;
			fprintf_color(Red, stderr, "xcb_connect failed: %d\n", err);
			throw;
		}
	}

	xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(XCBConnection));
	for (int s = screenp; s > 0; s--)
		xcb_screen_next(&iter);
	mXCBScreen = iter.data;
	mXCBWindow = xcb_generate_id(XCBConnection);

	uint32_t valueList[] { mXCBScreen->black_pixel, 0 };
	xcb_create_window(
		XCBConnection,
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
		XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
		valueList);

	xcb_change_property(
		XCBConnection,
		XCB_PROP_MODE_REPLACE,
		mXCBWindow,
		XCB_ATOM_WM_NAME,
		XCB_ATOM_STRING,
		8,
		title.length(),
		title.c_str());

	xcb_intern_atom_cookie_t wmDeleteCookie = xcb_intern_atom(XCBConnection, 0, strlen("WM_DELETE_WINDOW"), "WM_DELETE_WINDOW");
	xcb_intern_atom_cookie_t wmProtocolsCookie = xcb_intern_atom(XCBConnection, 0, strlen("WM_PROTOCOLS"), "WM_PROTOCOLS");
	xcb_intern_atom_reply_t *wmDeleteReply = xcb_intern_atom_reply(XCBConnection, wmDeleteCookie, NULL);
	xcb_intern_atom_reply_t *wmProtocolsReply = xcb_intern_atom_reply(XCBConnection, wmProtocolsCookie, NULL);
	mXCBDeleteWin = wmDeleteReply->atom;
	mXCBProtocols = wmProtocolsReply->atom;
	xcb_change_property(XCBConnection, XCB_PROP_MODE_REPLACE, mXCBWindow, wmProtocolsReply->atom, 4, 32, 1, &wmDeleteReply->atom);

	xcb_map_window(XCBConnection, mXCBWindow);
	xcb_flush(XCBConnection);

	VkXcbSurfaceCreateInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	info.connection = XCBConnection;
	info.window = mXCBWindow;
	ThrowIfFailed(vkCreateXcbSurfaceKHR(*mInstance, &info, nullptr, &mSurface), "vkCreateXcbSurfaceKHR Failed");

	#endif
}
Window::~Window() {
	#ifdef __linux
 	xcb_destroy_window(XCBConnection, mXCBWindow);
	#endif
	DestroySwapchain();
	vkDestroySurfaceKHR(*mInstance, mSurface, nullptr);
	if (mGLFWWindow) glfwDestroyWindow(mGLFWWindow);
}

void Window::Title(const string& title) {
	if (!mGLFWWindow) return;
	glfwSetWindowTitle(mGLFWWindow, title.c_str());
	mTitle = title;
}
void Window::Icon(GLFWimage* icon){
	if (!mGLFWWindow) return;
	glfwSetWindowIcon(mGLFWWindow, 1, icon);
}

VkImage Window::AcquireNextImage() {
	if (mSwapchain == VK_NULL_HANDLE) return VK_NULL_HANDLE;

	mImageAvailableSemaphoreIndex = (mImageAvailableSemaphoreIndex + 1) % (uint32_t)mImageAvailableSemaphores.size();
	VkResult err = vkAcquireNextImageKHR(*mDevice, mSwapchain, numeric_limits<uint64_t>::max(), *mImageAvailableSemaphores[mImageAvailableSemaphoreIndex], VK_NULL_HANDLE, &mCurrentBackBufferIndex);
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
		CreateSwapchain(mDevice);

	if (mFrameData == nullptr) return VK_NULL_HANDLE;
	if (mSwapchain == VK_NULL_HANDLE) return VK_NULL_HANDLE; // swapchain was destroyed during CreateSwapchain (happens when window is minimized)
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

bool Window::ShouldClose() const{
	if (mGLFWWindow)
		return glfwWindowShouldClose(mGLFWWindow);
	
	#if __linux
	if (XCBConnection) {
		bool close = false;
		xcb_generic_event_t* event = xcb_poll_for_event(XCBConnection);
		if (!event) return false;

		switch (event->response_type & ~0x80) {
		case XCB_CLIENT_MESSAGE: {
			close = ((xcb_client_message_event_t *)event)->data.data32[0] == mXCBDeleteWin;
			break;
		}
		}
		free(event);

		return close;
	}
	#endif

	return false;
}

GLFWmonitor* Window::GetCurrentMonitor(const GLFWvidmode** mode) const {
	int nmonitors, i;
	int wx, wy, ww, wh;
	int mx, my, mw, mh;
	int overlap, bestoverlap;
	GLFWmonitor* bestmonitor;
	GLFWmonitor** monitors;

	bestoverlap = 0;
	bestmonitor = NULL;

	glfwGetWindowPos(mGLFWWindow, &wx, &wy);
	glfwGetWindowSize(mGLFWWindow, &ww, &wh);
	monitors = glfwGetMonitors(&nmonitors);

	for (i = 0; i < nmonitors; i++) {
		const GLFWvidmode* modei = glfwGetVideoMode(monitors[i]);
		glfwGetMonitorPos(monitors[i], &mx, &my);
		mw = modei->width;
		mh = modei->height;

		overlap =
			max(0, min(wx + ww, mx + mw) - max(wx, mx)) *
			max(0, min(wy + wh, my + mh) - max(wy, my));

		if (bestoverlap < overlap) {
			bestoverlap = overlap;
			bestmonitor = monitors[i];
			*mode = modei;
		}
	}

	return bestmonitor;
}

void Window::Fullscreen(bool fs) {
	if (!mGLFWWindow) return;
	if (fs) {
		mWindowedRect = mClientRect;

		const GLFWvidmode* mode;
		GLFWmonitor* monitor = GetCurrentMonitor(&mode);

		glfwSetWindowAttrib(mGLFWWindow, GLFW_RED_BITS, mode->redBits);
		glfwSetWindowAttrib(mGLFWWindow, GLFW_GREEN_BITS, mode->greenBits);
		glfwSetWindowAttrib(mGLFWWindow, GLFW_BLUE_BITS, mode->blueBits);
		glfwSetWindowAttrib(mGLFWWindow, GLFW_REFRESH_RATE, mode->refreshRate);
		glfwSetWindowMonitor(mGLFWWindow, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);

		mFullscreen = true;
	} else {
		glfwSetWindowMonitor(mGLFWWindow, nullptr, 0, 0, mWindowedRect.extent.width, mWindowedRect.extent.height, GLFW_DONT_CARE);

		glfwSetWindowPos(mGLFWWindow, mWindowedRect.offset.x, mWindowedRect.offset.y);
		glfwSetWindowSize(mGLFWWindow, mWindowedRect.extent.width, mWindowedRect.extent.height);

		mFullscreen = false;
	}
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
		fprintf_color(Red, stderr, "Failed to find queue families");
		throw;
	}
	uint32_t queueFamilyIndices[] = { graphicsFamily, presentFamily };

	// find the size of the swapchain
	if (capabilities.currentExtent.width != numeric_limits<uint32_t>::max() && capabilities.currentExtent.width != 0)
		mSwapchainSize = capabilities.currentExtent;
	else {
		mSwapchainSize.width = std::clamp(mClientRect.extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		mSwapchainSize.height = std::clamp(mClientRect.extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
	}

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
	//if (!mGLFWWindow || !mFullscreen) {
		for (const auto& availablePresentMode : presentModes)
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				presentMode = availablePresentMode;
				break;
			} else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
				presentMode = availablePresentMode;
	//}

	// find the preferrable number of back buffers
	mImageCount = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0 && mImageCount > capabilities.maxImageCount)
		mImageCount = capabilities.maxImageCount;

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
		fprintf_color(Red, stderr, "Surface not supported by device!");
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
	mDevice->FlushFrames();
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