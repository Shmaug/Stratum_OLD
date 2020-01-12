#ifdef __linux
#include <vulkan/vulkan.h>
namespace x11{
#include <xcb/xcb.h>
#include <vulkan/vulkan_xlib.h>
#include <vulkan/vulkan_xlib_xrandr.h>
};
#endif

#include <Core/Instance.hpp>
#include <Core/Device.hpp>
#include <Core/Window.hpp>
#include <Util/Profiler.hpp>

using namespace std;

#ifdef WINDOWS
LRESULT CALLBACK Instance::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_PAINT:
		break;
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_SYSCHAR:
		break;
	case WM_SIZE:
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProcW(hwnd, message, wParam, lParam);
	}
	return 0;
}
#endif

Instance::Instance() : mInstance(VK_NULL_HANDLE), mFrameCount(0), mMaxFramesInFlight(0), mTotalTime(0), mDeltaTime(0), mWindowInput(nullptr) {
	set<string> instanceExtensions;
	#ifdef ENABLE_DEBUG_LAYERS
	instanceExtensions.insert(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	#endif

	instanceExtensions.insert(VK_KHR_SURFACE_EXTENSION_NAME);
	#ifdef __linux
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
	#endif
}
Instance::~Instance() {
	for (Window* w : mWindows)
		safe_delete(w);

	#ifdef __linux
	for (auto& c : mXCBConnections)
		xcb_disconnect(c.second);
	#endif

	for (Device* d : mDevices)
		safe_delete(d);

	vkDestroyInstance(mInstance, nullptr);
}

void Instance::CreateDevicesAndWindows(const vector<DisplayCreateInfo>& displays) {
	#ifdef WINDOWS
	HINSTANCE hInstance = GetModuleHandleA(NULL);

	WNDCLASSEXA windowClass = {};
	windowClass.cbSize = sizeof(WNDCLASSEXW);
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
	
	// create windows
	for (auto& it : displays) {
		if (devices.size() <= it.mDevice){
			fprintf_color(COLOR_RED, stderr, "Device index out of bounds: %d\n", it.mDevice);
			throw;
		}
		VkPhysicalDevice physicalDevice = devices[it.mDevice];
		if (mDevices.size() <= it.mDevice) mDevices.resize(it.mDevice + 1);
		
		#ifdef __linux
		// TODO: acquire xlib displays
		Window* w = new Window(this, "Stratum " + to_string(mWindows.size()), mWindowInput, it.mWindowPosition, conn, screenp);
		#else
		Window* w = new Window(this, "Stratum " + to_string(mWindows.size()), mWindowInput, it.mWindowPosition, hInstance);
		#endif

		if (!mDevices[it.mDevice]) {
			uint32_t gq, pq;
			Device::FindQueueFamilies(physicalDevice, w->Surface(), gq, pq);
			mDevices[it.mDevice] = new Device(this, physicalDevice, it.mDevice, gq, pq, deviceExtensions, validationLayers);
		}
		mDevices[it.mDevice]->mWindowCount++;
		
		w->CreateSwapchain(mDevices[it.mDevice]);
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
	MSG msg = {};
	while (GetMessageA(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessageA(&msg);

		switch (msg.message) {
		case WM_QUIT: return false;
		case WM_MOVE:
			for (Window* w : mWindows)
				if (w->mHwnd == msg.hwnd) {
					RECT cr;
					GetClientRect(w->mHwnd, &cr);
					w->mClientRect.offset = { (int32_t)cr.top, (int32_t)cr.left };
					w->mClientRect.extent = { (uint32_t)((int32_t)cr.bottom - (int32_t)cr.top), (uint32_t)((int32_t)cr.right - (int32_t)cr.left) };
				}
		case WM_SIZE:
			for (Window* w : mWindows)
				if (w->mHwnd == msg.hwnd)
					w->ResizeSwapchain();
			break;
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

				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_DOWN)
					mWindowInput->mCurrent.mKeys[MOUSE_LEFT] = true;
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_UP)
					mWindowInput->mCurrent.mKeys[MOUSE_LEFT] = false;
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_DOWN)
					mWindowInput->mCurrent.mKeys[MOUSE_RIGHT] = true;
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_UP)
					mWindowInput->mCurrent.mKeys[MOUSE_RIGHT] = false;
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_DOWN)
					mWindowInput->mCurrent.mKeys[MOUSE_MIDDLE] = true;
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_UP)
					mWindowInput->mCurrent.mKeys[MOUSE_MIDDLE] = false;
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN)
					mWindowInput->mCurrent.mKeys[MOUSE_X1] = true;
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP)
					mWindowInput->mCurrent.mKeys[MOUSE_X1] = false;
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN)
					mWindowInput->mCurrent.mKeys[MOUSE_X2] = true;
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP)
					mWindowInput->mCurrent.mKeys[MOUSE_X2] = false;

				if (raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL)
					mWindowInput->mCurrent.mScrollDelta += (short)raw->data.mouse.usButtonData / (float)WHEEL_DELTA;
			}
			if (raw->header.dwType == RIM_TYPEKEYBOARD)
				mWindowInput->mCurrent.mKeys[(KeyCode)raw->data.keyboard.VKey] = (raw->data.keyboard.Flags & RI_KEY_BREAK) == 0;

			delete[] lpb;
			break;
		}
		}

		if (msg.message == WM_PAINT) break; // break and allow a frame to execute
	}

	POINT pt;
	GetCursorPos(&pt);
	mWindowInput->mCurrent.mCursorPos = float2((float)pt.x, (float)pt.y);
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