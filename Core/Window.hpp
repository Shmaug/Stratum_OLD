#pragma once


#ifdef __linux
#include <vulkan/vulkan.h>
#include <xcb/xcb.h>
#include <vulkan/vulkan_xcb.h>
namespace x11{
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <vulkan/vulkan_xlib_xrandr.h>
};
#else
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#endif

#include <Core/Device.hpp>
#include <Input/MouseKeyboardInput.hpp>
#include <Util/Util.hpp>

class Instance;
class Camera;
class Texture;

class Window {
public:
	ENGINE_EXPORT ~Window();

	inline bool Fullscreen() const { return mFullscreen; }
	ENGINE_EXPORT void Fullscreen(bool fs);

	inline VkRect2D ClientRect() const { return mClientRect; };

	inline std::string Title() const { return mTitle; }

	inline uint32_t CurrentBackBufferIndex() const { return mCurrentBackBufferIndex; }
	inline VkImage BackBuffer() const { if (!mFrameData) return VK_NULL_HANDLE; return mFrameData[mCurrentBackBufferIndex].mSwapchainImage; }
	inline VkImageView BackBufferView() const { if (!mFrameData) return VK_NULL_HANDLE; return mFrameData[mCurrentBackBufferIndex].mSwapchainImageView; }
	inline VkImage BackBuffer(uint32_t i) const { if (!mFrameData) return VK_NULL_HANDLE; return mFrameData[i].mSwapchainImage; }
	inline VkImageView BackBufferView(uint32_t i) const { if (!mFrameData) return VK_NULL_HANDLE; return mFrameData[i].mSwapchainImageView; }
	inline uint32_t BackBufferCount() const { return mImageCount; }
	inline VkExtent2D BackBufferSize() const { return mSwapchainSize; }
	inline VkSurfaceKHR Surface() const { return mSurface; }
	inline VkSurfaceFormatKHR Format() const { return mFormat; }

	#ifdef WINDOWS
	inline HWND Hwnd() const { return mHwnd; }
	#endif

	inline ::Device* Device() const { return mDevice; }

private:
	friend class Camera;
	friend class Stratum;
	friend class Instance;

	#ifdef __linux
	ENGINE_EXPORT Window(Instance* instance, const std::string& title, MouseKeyboardInput* input, VkRect2D position, xcb_connection_t* XCBConnection, xcb_screen_t* XCBScreen);
	#else
	ENGINE_EXPORT Window(Instance* instance, const std::string& title, MouseKeyboardInput* input, VkRect2D position, HINSTANCE hInst);
	#endif

	ENGINE_EXPORT VkImage AcquireNextImage();
	/// Waits on all semaphores in waitSemaphores
	ENGINE_EXPORT void Present(std::vector<VkSemaphore> waitSemaphores);

	MouseKeyboardInput* mInput;

	bool mFullscreen;
	VkRect2D mClientRect;
	std::string mTitle;

	VkDisplayKHR mDirectDisplay;

	Instance* mInstance;
	::Device* mDevice;
	VkPhysicalDevice mPhysicalDevice;

	#ifdef __linux
	xcb_connection_t* mXCBConnection;
	xcb_screen_t* mXCBScreen;
	xcb_window_t mXCBWindow;
	xcb_atom_t mXCBProtocols;
	xcb_atom_t mXCBDeleteWin;
	VkRect2D mWindowedRect;
	#else
	HWND mHwnd;
	RECT mWindowedRect;
	#endif

	VkSurfaceKHR mSurface;
	VkSwapchainKHR mSwapchain;

	VkExtent2D mSwapchainSize;
	VkSurfaceFormatKHR mFormat;
	uint32_t mImageCount;
	uint32_t mCurrentBackBufferIndex;

	struct FrameData {
		VkImage mSwapchainImage;
		VkImageView mSwapchainImageView;
		inline FrameData() : mSwapchainImage(VK_NULL_HANDLE), mSwapchainImageView(VK_NULL_HANDLE) {};
	};
	FrameData* mFrameData;

	/// semaphores that signal when an image is available (via vkAcquireNextImageKHR)
	std::vector<std::shared_ptr<Semaphore>> mImageAvailableSemaphores;
	uint32_t mImageAvailableSemaphoreIndex;

	Camera* mTargetCamera;

	ENGINE_EXPORT void CreateSwapchain(::Device* device);
	ENGINE_EXPORT void DestroySwapchain();
};