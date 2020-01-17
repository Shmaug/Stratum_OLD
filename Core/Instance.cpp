#include <Core/Instance.hpp>
#include <Core/Device.hpp>
#include <Core/Window.hpp>
#include <Scene/Camera.hpp>
#include <Util/Profiler.hpp>

using namespace std;

#ifdef WINDOWS
std::vector<Instance*> Instance::sInstances;
LRESULT CALLBACK Instance::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_PAINT:
		break;
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_SYSCHAR:
		break;
	case WM_DESTROY:
	case WM_QUIT:
	case WM_MOVE:
	case WM_SIZE:
		for (Instance* i : sInstances)
			i->HandleMessage(hwnd, message, wParam, lParam);
		break;
	default:
		return DefWindowProcA(hwnd, message, wParam, lParam);
	}
	return 0;
}
#endif

Instance::Instance(const DisplayCreateInfo& display)
	: mInstance(VK_NULL_HANDLE), mFrameCount(0), mMaxFramesInFlight(0), mTotalTime(0), mDeltaTime(0), mWindow(nullptr), mWindowInput(nullptr), mDestroyPending(false) {

	vector<const char*> deviceExtensions {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,

		VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
	};
	
	set<string> instanceExtensions;
	#ifdef ENABLE_DEBUG_LAYERS
	instanceExtensions.insert(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	#endif

	instanceExtensions.insert(VK_KHR_SURFACE_EXTENSION_NAME);
	#ifdef __linux
	instanceExtensions.insert(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
	instanceExtensions.insert(VK_KHR_DISPLAY_EXTENSION_NAME);
	instanceExtensions.insert(VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME);
	instanceExtensions.insert(VK_EXT_ACQUIRE_XLIB_DISPLAY_EXTENSION_NAME);
	#else
	instanceExtensions.insert(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
	#endif

	vector<const char*> validationLayers;
	#ifdef ENABLE_DEBUG_LAYERS
	validationLayers.push_back("VK_LAYER_KHRONOS_validation");
	validationLayers.push_back("VK_LAYER_LUNARG_core_validation");
	validationLayers.push_back("VK_LAYER_LUNARG_standard_validation");
	#endif

	#pragma region Create Vulkan Instance

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

	#pragma endregion

	#ifdef WINDOWS
	HINSTANCE hInstance = GetModuleHandleA(NULL);

	WNDCLASSEXA windowClass = {};
	windowClass.cbSize = sizeof(WNDCLASSEXA);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = &WndProc;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = hInstance;
	windowClass.hIcon = ::LoadIcon(hInstance, NULL); //  MAKEINTRESOURCE(APPLICATION_ICON));
	windowClass.hCursor = ::LoadCursor(NULL, IDC_ARROW);
	windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	windowClass.lpszMenuName = NULL;
	windowClass.lpszClassName = "Stratum";
	windowClass.hIconSm = ::LoadIcon(hInstance, NULL); //  MAKEINTRESOURCE(APPLICATION_ICON));
	HRESULT hr = ::RegisterClassExA(&windowClass);
	if (FAILED(hr)) {
		fprintf_color(COLOR_RED, stderr, "Failed to register window class\n");
		throw;
	}
	// register raw input devices
	RAWINPUTDEVICE rID[2];
	// Mouse
	rID[0].usUsagePage = 0x01;
	rID[0].usUsage = 0x02;
	rID[0].dwFlags = 0;
	rID[0].hwndTarget = NULL;
	// Keyboard
	rID[1].usUsagePage = 0x01;
	rID[1].usUsage = 0x06;
	rID[1].dwFlags = 0;
	rID[1].hwndTarget = NULL;
	if (RegisterRawInputDevices(rID, 2, sizeof(RAWINPUTDEVICE)) == FALSE)
		fprintf_color(COLOR_RED, stderr, "Failed to register raw input device(s)\n");
	sInstances.push_back(this);
	#endif


	mWindowInput = new MouseKeyboardInput();

	mMaxFramesInFlight = 0xFFFF;

	uint32_t deviceCount;
	vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
	vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());

	#pragma region Create Windows
	if (display.mDeviceIndex >= devices.size()){
		fprintf_color(COLOR_RED, stderr, "Device index out of bounds: %u\n", display.mDeviceIndex);
		throw;
	}
	VkPhysicalDevice physicalDevice = devices[display.mDeviceIndex];

	#ifdef __linux
	// create xcb connection
	if (!mXCBConnection) {
		mXCBConnection = xcb_connect(nullptr, nullptr);
		if (int err = xcb_connection_has_error(mXCBConnection)){
			fprintf_color(COLOR_RED, stderr, "Failed to connect to xcb: %d\n", err);
			throw;
		}
		mXCBKeySymbols = xcb_key_symbols_alloc(mXCBConnection);
	}

	// find xcb screen
	for (xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(mXCBConnection)); iter.rem; xcb_screen_next(&iter)) {
		xcb_screen_t* screen = iter.data;

		// find suitable physical device
		uint32_t queueFamilyCount;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
		vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

		for (uint32_t q = 0; q < queueFamilyCount; q++){
			if (vkGetPhysicalDeviceXcbPresentationSupportKHR(physicalDevice, q, mXCBConnection, screen->root_visual)){
				mWindow = new ::Window(this, "Stratum", mWindowInput, display.mWindowPosition, mXCBConnection, screen);
				break;
			}
		}
		if (mWindow) break;
	}
	#else
	mWindow = new Window(this, "Stratum " + to_string(mWindows.size()), mWindowInput, it.mWindowPosition, hInstance);
	#endif
	#pragma endregion

	uint32_t graphicsQueue, presentQueue;
	Device::FindQueueFamilies(physicalDevice, mWindow->Surface(), graphicsQueue, presentQueue);
	mDevice = new ::Device(this, physicalDevice, display.mDeviceIndex, graphicsQueue, presentQueue, deviceExtensions, validationLayers);

	mWindow->CreateSwapchain(mDevice);
	mMaxFramesInFlight = mWindow->mImageCount;

	mDevice->mFrameContexts = new Device::FrameContext[mMaxFramesInFlight];

	mStartTime = mClock.now();
	mLastFrame = mClock.now();
}
Instance::~Instance() {
	safe_delete(mWindow);

	#ifdef __linux
	xcb_key_symbols_free(mXCBKeySymbols);
	xcb_disconnect(mXCBConnection);
	#else
	for (auto it = sInstances.begin(); it != sInstances.end();)
		if (*it == this)
			it = sInstances.erase(it);
		else
			it++;
	#endif

	safe_delete(mDevice);

	vkDestroyInstance(mInstance, nullptr);
}

#ifdef WINDOWS
void Instance::HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message){
	case WM_DESTROY:
	case WM_QUIT:
		mDestroyPending = true;
		break;
	case WM_SIZE:
	case WM_MOVE:
		for (Window* w : mWindows)
			if (w->mHwnd == hwnd){
				RECT cr;
				GetClientRect(w->mHwnd, &cr);
				w->mClientRect.offset = { (int32_t)cr.top, (int32_t)cr.left };
				w->mClientRect.extent = { (uint32_t)((int32_t)cr.bottom - (int32_t)cr.top), (uint32_t)((int32_t)cr.right - (int32_t)cr.left) };
			}
		break;
	}
}
#elif defined(__linux)
void Instance::ProcessEvent(xcb_generic_event_t* event) {
	xcb_motion_notify_event_t* mn = (xcb_motion_notify_event_t*)event;
	xcb_resize_request_event_t* rr = (xcb_resize_request_event_t*)event;
	xcb_button_press_event_t* bp = (xcb_button_press_event_t*)event;
	xcb_key_press_event_t* kp = (xcb_key_press_event_t*)event;
	xcb_key_release_event_t* kr = (xcb_key_release_event_t*)event;
	xcb_client_message_event_t* cm = (xcb_client_message_event_t*)event;

	KeyCode kc;

	switch (event->response_type & ~0x80) {
	case XCB_MOTION_NOTIFY:
		if (mn->same_screen){
			mWindowInput->mCurrent.mCursorPos = float2((float)mn->event_x, (float)mn->event_y);
			if (mWindow->mXCBWindow == mn->event)
				if (mWindow->mTargetCamera){
					float2 uv = mWindowInput->mCurrent.mCursorPos / float2((float)mWindow->mClientRect.extent.width, (float)mWindow->mClientRect.extent.height);
					mWindowInput->mMousePointer.mWorldRay = mWindow->mTargetCamera->ScreenToWorldRay(uv);
				}
		}
		break;

	case XCB_KEY_PRESS:
		kc = (KeyCode)xcb_key_press_lookup_keysym(mXCBKeySymbols, kp, 0);
		mWindowInput->mCurrent.mKeys[kc] = true;
		if ((kc == KEY_LALT || kc == KEY_ENTER) && mWindowInput->KeyDown(KEY_ENTER) && mWindowInput->KeyDown(KEY_LALT))
			mWindow->Fullscreen(!mWindow->Fullscreen());
		break;
	case XCB_KEY_RELEASE:
		kc = (KeyCode)xcb_key_release_lookup_keysym(mXCBKeySymbols, kp, 0);
		mWindowInput->mCurrent.mKeys[kc] = false;
		break;

	case XCB_BUTTON_PRESS:
		if (bp->detail == 4){
			mWindowInput->mCurrent.mScrollDelta += 1.0f;
			break;
		}
		if (bp->detail == 5){
			mWindowInput->mCurrent.mScrollDelta =- 1.0f;
			break;
		}
	case XCB_BUTTON_RELEASE:
		switch (bp->detail){
		case 1:
			mWindowInput->mCurrent.mKeys[MOUSE_LEFT] = (event->response_type & ~0x80) == XCB_BUTTON_PRESS;
			mWindowInput->mMousePointer.mAxis[0] = ((event->response_type & ~0x80) == XCB_BUTTON_PRESS) ? 1.f : 0.f;
			break;
		case 2:
			mWindowInput->mCurrent.mKeys[MOUSE_MIDDLE] = (event->response_type & ~0x80) == XCB_BUTTON_PRESS;
			mWindowInput->mMousePointer.mAxis[2] = ((event->response_type & ~0x80) == XCB_BUTTON_PRESS) ? 1.f : 0.f;
			break;
		case 3:
			mWindowInput->mCurrent.mKeys[MOUSE_RIGHT] = (event->response_type & ~0x80) == XCB_BUTTON_PRESS;
			mWindowInput->mMousePointer.mAxis[1] = ((event->response_type & ~0x80) == XCB_BUTTON_PRESS) ? 1.f : 0.f;
			break;
		}
		break;

	case XCB_CLIENT_MESSAGE:
		if (cm->data.data32[0] == mWindow->mXCBDeleteWin)
			mDestroyPending = true;
		break;
	}
}
xcb_generic_event_t* Instance::PollEvent() {
    xcb_generic_event_t* cur = xcb_poll_for_event(mXCBConnection);
	xcb_generic_event_t* nxt = xcb_poll_for_event(mXCBConnection);

	if (cur && (cur->response_type & ~0x80) == XCB_KEY_RELEASE &&
		nxt && (nxt->response_type & ~0x80) == XCB_KEY_PRESS) {

		xcb_key_press_event_t* kp = (xcb_key_press_event_t*)cur;
		xcb_key_press_event_t* nkp = (xcb_key_press_event_t*)nxt;

		if (nkp->time == kp->time && nkp->detail == kp->detail) {
			free(cur);
			free(nxt);
			return PollEvent(); // ignore repeat key press events
		}
	}

	if (cur) {
		ProcessEvent(cur);
		free(cur);
	}
	return nxt;
}
#endif

bool Instance::PollEvents() {
    auto t1 = mClock.now();
	mDeltaTime = (t1 - mLastFrame).count() * 1e-9f;
	mTotalTime = (t1 - mStartTime).count() * 1e-9f;
	mLastFrame = t1;

	#ifdef __linux
	xcb_generic_event_t* event;
	while (event = PollEvent()){
		if (!event) break;
		ProcessEvent(event);
		free(event);
	}

	xcb_get_geometry_cookie_t cookie = xcb_get_geometry(mXCBConnection, mWindow->mXCBWindow);
	if (xcb_get_geometry_reply_t* reply = xcb_get_geometry_reply(mXCBConnection, cookie, NULL)) {
		mWindow->mClientRect.offset.x = reply->x;
		mWindow->mClientRect.offset.y = reply->y;
		mWindow->mClientRect.extent.width = reply->width;
		mWindow->mClientRect.extent.height = reply->height;
		free(reply);
	}
	

	mWindowInput->mCurrent.mCursorDelta = mWindowInput->mCurrent.mCursorPos - mWindowInput->mLast.mCursorPos;
	return !mDestroyPending;

	#elif defined(WINDOWS)
	while (true) {
		if (mDestroyPending) return false;
		MSG msg = {};
		if (!GetMessageA(&msg, NULL, 0, 0)) return false;
		TranslateMessage(&msg);
		DispatchMessageA(&msg);

		if (mDestroyPending) return false;

		switch (msg.message) {
		case WM_INPUT: {
			uint32_t dwSize = 0;
			GetRawInputData((HRAWINPUT)msg.lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
			uint8_t* lpb = new uint8_t[dwSize];
			if (GetRawInputData((HRAWINPUT)msg.lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
				fprintf_color(COLOR_YELLOW, stderr, "Incorrect GetRawInputData size\n");
			RAWINPUT* raw = (RAWINPUT*)lpb;

			if (raw->header.dwType == RIM_TYPEMOUSE) {
				int x = raw->data.mouse.lLastX;
				int y = raw->data.mouse.lLastY;
				mWindowInput->mCurrent.mCursorDelta += int2(x, y);

				if (mWindowInput->mLockMouse) {
					RECT rect;
					GetWindowRect(msg.hwnd, &rect);
					SetCursorPos((rect.right + rect.left) / 2, (rect.bottom + rect.top) / 2);
				}

				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_DOWN) {
					mWindowInput->mCurrent.mKeys[MOUSE_LEFT] = true;
					mWindowInput->mMousePointer.mAxis[0] = 1.f;
				}
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_UP){
					mWindowInput->mCurrent.mKeys[MOUSE_LEFT] = false;
					mWindowInput->mMousePointer.mAxis[0] = 0.f;
				}
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_DOWN){
					mWindowInput->mCurrent.mKeys[MOUSE_RIGHT] = true;
					mWindowInput->mMousePointer.mAxis[1] = 1.f;
				}
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_UP){
					mWindowInput->mCurrent.mKeys[MOUSE_RIGHT] = false;
					mWindowInput->mMousePointer.mAxis[1] = 0.f;
				}
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_DOWN){
					mWindowInput->mCurrent.mKeys[MOUSE_MIDDLE] = true;
					mWindowInput->mMousePointer.mAxis[2] = 1.f;
				}
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_UP){
					mWindowInput->mCurrent.mKeys[MOUSE_MIDDLE] = false;
					mWindowInput->mMousePointer.mAxis[2] = 0.f;
				}
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN)
					mWindowInput->mCurrent.mKeys[MOUSE_X1] = true;
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP)
					mWindowInput->mCurrent.mKeys[MOUSE_X1] = false;
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN)
					mWindowInput->mCurrent.mKeys[MOUSE_X2] = true;
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP)
					mWindowInput->mCurrent.mKeys[MOUSE_X2] = false;

				if (raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL)
					mWindowInput->mCurrent.mScrollDelta += (short)(raw->data.mouse.usButtonData) / (float)WHEEL_DELTA;
			}
			if (raw->header.dwType == RIM_TYPEKEYBOARD) {
				USHORT key = raw->data.keyboard.VKey;
				if (key == VK_SHIFT) key = VK_LSHIFT;
				if (key == VK_MENU) key = VK_LMENU;
				if (key == VK_CONTROL) key = VK_LCONTROL;
				mWindowInput->mCurrent.mKeys[(KeyCode)key] = (raw->data.keyboard.Flags & RI_KEY_BREAK) == 0;

				if ((raw->data.keyboard.Flags & RI_KEY_BREAK) == 0 &&
					((KeyCode)raw->data.keyboard.VKey == KEY_LALT || (KeyCode)raw->data.keyboard.VKey == KEY_ENTER) &&
					mWindowInput->KeyDown(KEY_LALT) && mWindowInput->KeyDown(KEY_ENTER)) {
					if (mWindow->mHwnd == msg.hwnd)
						mWindow->Fullscreen(!mWindow->Fullscreen());
				}
			}
			delete[] lpb;
			break;
		}
		}

		if (msg.message == WM_PAINT) break; // break and allow a frame to execute
	}

	POINT pt;
	GetCursorPos(&pt);
	for (Window* w : mWindows) {
		RECT r;
		GetWindowRect(w->mHwnd, &r);
		if (!PtInRect(&r, pt)) continue;
		ScreenToClient(w->mHwnd, &pt);
		mWindowInput->mCurrent.mCursorPos = float2((float)pt.x, (float)pt.y);
		if (w->mTargetCamera) {
			float2 uv = mWindowInput->mCurrent.mCursorPos / float2((float)w->mSwapchainSize.width, (float)w->mSwapchainSize.height);
			mWindowInput->mMousePointer.mWorldRay = w->mTargetCamera->ScreenToWorldRay(uv);
		}
		break;
	}

	return true;
	#endif
}

void Instance::AdvanceFrame() {
	uint32_t wi = 0;
	vector<VkSemaphore> waitSemaphores;
	for (const shared_ptr<Semaphore>& s : mDevice->CurrentFrameContext()->mSemaphores)
		waitSemaphores.push_back(*s);
	// will wait on the semaphores signalled by the frame mMaxFramesInFlight ago
	mWindow->Present(waitSemaphores);
	wi++;

	mFrameCount++;

	mDevice->mFrameContextIndex = mFrameCount % mMaxFramesInFlight;
	mDevice->CurrentFrameContext()->Reset();
}