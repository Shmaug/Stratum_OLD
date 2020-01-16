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

Instance::Instance() : mInstance(VK_NULL_HANDLE), mFrameCount(0), mMaxFramesInFlight(0), mTotalTime(0), mDeltaTime(0), mWindowInput(nullptr) {
	#ifdef __linux
	mDestroyPending = false;
	#endif

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


	#ifdef WINDOWS
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
	mDestroyPending = false;
	#endif
}
Instance::~Instance() {
	for (Window* w : mWindows)
		safe_delete(w);

	#ifdef __linux
	for (auto& kp : mXCBConnections){
		xcb_key_symbols_free(kp.second.mKeySymbols);
		xcb_disconnect(kp.second.mConnection);
	}
	for (auto& c : mXDisplays)
		x11::XCloseDisplay(c.second);
	#else
	for (auto it = sInstances.begin(); it != sInstances.end();)
		if (*it == this)
			it = sInstances.erase(it);
		else
			it++;
	#endif

	for (Device* d : mDevices)
		safe_delete(d);

	vkDestroyInstance(mInstance, nullptr);
}

void Instance::CreateDevicesAndWindows(const vector<DisplayCreateInfo>& displays) {
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
	#endif

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

	mWindowInput = new MouseKeyboardInput();

	mMaxFramesInFlight = 0xFFFF;

	uint32_t deviceCount;
	vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
	vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());

	std::unordered_map<VkPhysicalDevice, uint32_t> deviceMap;

	// create windows
	for (auto& it : displays) {
		if (it.mDeviceIndex >= devices.size()){
			fprintf_color(COLOR_RED, stderr, "Device index out of bounds: %u\n", it.mDeviceIndex);
			throw;
		}
		VkPhysicalDevice physicalDevice = devices[it.mDeviceIndex];

		Window* w = nullptr;

		#ifdef __linux
		if (it.mXDirectDisplay) {
			x11::Display* dpy;
			if (mXDisplays.count(it.mXDisplay))
				dpy = mXDisplays.at(it.mXDisplay);
			else {
				dpy = x11::XOpenDisplay(it.mXDisplay.c_str());
				if (!dpy) {
					fprintf_color(COLOR_RED, stderr, "Failed to open X display: %s\n", it.mXDisplay.c_str());
					throw;
				}
				mXDisplays.emplace(it.mXDisplay, dpy);
			}

			x11::PFN_vkGetRandROutputDisplayEXT vkGetRandROutputDisplay = (x11::PFN_vkGetRandROutputDisplayEXT)vkGetInstanceProcAddr(mInstance, "vkGetRandROutputDisplayEXT");
			x11::PFN_vkAcquireXlibDisplayEXT vkAcquireXlibDisplay = (x11::PFN_vkAcquireXlibDisplayEXT)vkGetInstanceProcAddr(mInstance, "vkAcquireXlibDisplayEXT");

			x11::XRRScreenResources* scr     = x11::XRRGetScreenResources(dpy, x11::XDefaultRootWindow(dpy));

			VkDisplayKHR display = VK_NULL_HANDLE;
			for(int b = 0; b < scr->noutput; b++) {
				if (vkGetRandROutputDisplay(physicalDevice, dpy, scr->outputs[b], &display) == VK_SUCCESS && display != VK_NULL_HANDLE)
					break;
			}
			if (physicalDevice == VK_NULL_HANDLE || display == VK_NULL_HANDLE) {
				fprintf_color(COLOR_RED, stderr, "Failed to find a suitable physical device/display\n");
				throw;
			}
			ThrowIfFailed(vkAcquireXlibDisplay(physicalDevice, dpy, display), "Failed to acquire Xlib display");

			x11::XRRFreeScreenResources(scr);

			w = new Window(this, physicalDevice, display);
		}else{
			// create xcb connection
			xcb_connection_t* conn = nullptr;
			if (!mXCBConnections.count(it.mXDisplay)) {
				conn = xcb_connect(it.mXDisplay.data(), nullptr);
				mXCBConnections.emplace(it.mXDisplay, XCBConnection(conn, nullptr));
				if (int err = xcb_connection_has_error(conn)) {
					mXCBConnections.erase(it.mXDisplay);
					fprintf_color(COLOR_RED, stderr, "Failed to connect to display %s: %d\n", it.mXDisplay.c_str(), err);
					throw;
				}
				mXCBConnections[it.mXDisplay].mKeySymbols = xcb_key_symbols_alloc(conn);
			} else
				conn = mXCBConnections.at(it.mXDisplay).mConnection;

			// find xcb screen
			for (xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(conn)); iter.rem; xcb_screen_next(&iter)) {
				xcb_screen_t* screen = iter.data;

				// find suitable physical device
				uint32_t queueFamilyCount;
				vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
				vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
				vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

				for (uint32_t q = 0; q < queueFamilyCount; q++){
					if (vkGetPhysicalDeviceXcbPresentationSupportKHR(physicalDevice, q, conn, screen->root_visual)){
						w = new Window(this, "Stratum " + to_string(mWindows.size()), mWindowInput, it.mWindowPosition, conn, screen);
						break;
					}
				}
				if (w) break;
			}
		}
		#else
		w = new Window(this, "Stratum " + to_string(mWindows.size()), mWindowInput, it.mWindowPosition, hInstance);
		#endif


		if (!deviceMap.count(physicalDevice)) {
			uint32_t graphicsQueue, presentQueue;
			Device::FindQueueFamilies(physicalDevice, w->Surface(), graphicsQueue, presentQueue);
			Device* d = new Device(this, physicalDevice, mDevices.size(), graphicsQueue, presentQueue, deviceExtensions, validationLayers);
			deviceMap.emplace(physicalDevice, mDevices.size());
			mDevices.push_back(d);
		}

		Device* device = mDevices[deviceMap.at(physicalDevice)];
		device->mWindowCount++;
		w->CreateSwapchain(device);
		mWindows.push_back(w);
		mMaxFramesInFlight = min(mMaxFramesInFlight, w->mImageCount);
	}

	// remove null devices and setup frame contexts
	for (auto d = mDevices.begin(); d != mDevices.end();) {
		if (!*d) { d = mDevices.erase(d); continue; }
		(*d)->mFrameContexts.resize(mMaxFramesInFlight);
		for (uint32_t i = 0; i < mMaxFramesInFlight; i++)
			(*d)->mFrameContexts[i].mSemaphores.resize((*d)->mWindowCount);
		d++;
	}

	mStartTime = mClock.now();
	mLastFrame = mClock.now();
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
void Instance::ProcessEvent(Instance::XCBConnection* connection, xcb_generic_event_t* event) {
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
			for (Window* w : mWindows)
				if (w->mXCBConnection == connection->mConnection && w->mXCBWindow == mn->event){
					if (w->mTargetCamera){
						float2 uv = mWindowInput->mCurrent.mCursorPos / float2((float)w->mClientRect.extent.width, (float)w->mClientRect.extent.height);
						mWindowInput->mMousePointer.mWorldRay = w->mTargetCamera->ScreenToWorldRay(uv);
					}
				}
		}
		break;

	case XCB_KEY_PRESS:
		kc = (KeyCode)xcb_key_press_lookup_keysym(connection->mKeySymbols, kp, 0);
		mWindowInput->mCurrent.mKeys[kc] = true;
		if ((kc == KEY_LALT || kc == KEY_ENTER) && mWindowInput->KeyDown(KEY_ENTER) && mWindowInput->KeyDown(KEY_LALT))
			for (const auto& w : mWindows)
				w->Fullscreen(!w->Fullscreen());
		break;
	case XCB_KEY_RELEASE:
		kc = (KeyCode)xcb_key_release_lookup_keysym(connection->mKeySymbols, kp, 0);
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
		for (const auto& w : mWindows)
			if (w->mXCBConnection == connection->mConnection && cm->data.data32[0] == w->mXCBDeleteWin)
				mDestroyPending = true;
		break;
	}
}
xcb_generic_event_t* Instance::PollEvent(Instance::XCBConnection* connection) {
    xcb_generic_event_t* cur = xcb_poll_for_event(connection->mConnection);
	xcb_generic_event_t* nxt = xcb_poll_for_event(connection->mConnection);

	if (cur && (cur->response_type & ~0x80) == XCB_KEY_RELEASE &&
		nxt && (nxt->response_type & ~0x80) == XCB_KEY_PRESS) {

		xcb_key_press_event_t* kp = (xcb_key_press_event_t*)cur;
		xcb_key_press_event_t* nkp = (xcb_key_press_event_t*)nxt;

		if (nkp->time == kp->time && nkp->detail == kp->detail) {
			free(cur);
			free(nxt);
			return PollEvent(connection); // ignore repeat key press events
		}
	}

	if (cur) {
		ProcessEvent(connection, cur);
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
	for (auto& c : mXCBConnections) {
		xcb_generic_event_t* event;
		while (event = PollEvent(&c.second)){
			if (!event) break;
			ProcessEvent(&c.second, event);
			free(event);
		}
	}

	for (const auto& w : mWindows) {
		xcb_get_geometry_cookie_t cookie = xcb_get_geometry(w->mXCBConnection, w->mXCBWindow);
		if (xcb_get_geometry_reply_t* reply = xcb_get_geometry_reply(w->mXCBConnection, cookie, NULL)) {
			w->mClientRect.offset.x = reply->x;
			w->mClientRect.offset.y = reply->y;
			w->mClientRect.extent.width = reply->width;
			w->mClientRect.extent.height = reply->height;
			free(reply);
		}
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
					for (Window* w : mWindows)
						if (w->mHwnd == msg.hwnd)
							w->Fullscreen(!w->Fullscreen());
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
	for (Window* w : mWindows) {
		vector<VkSemaphore> waitSemaphores;
		for (const shared_ptr<Semaphore>& s : w->Device()->CurrentFrameContext()->mSemaphores[wi])
			waitSemaphores.push_back(*s);
		// will wait on the semaphores signalled by the frame mMaxFramesInFlight ago
		w->Present(waitSemaphores);
		wi++;
	}

	mFrameCount++;

	for (uint32_t i = 0; i < mDevices.size(); i++) {
		mDevices[i]->mFrameContextIndex = mFrameCount % mMaxFramesInFlight;
		mDevices[i]->CurrentFrameContext()->Reset();
	}
}