#pragma once

#include <GLFW/glfw3.h>

#ifndef WINDOWS
namespace x11 {
	#include <X11/Xlib.h>
	#include <X11/extensions/Xrandr.h>
	#include <vulkan/vulkan_xlib.h>
	#include <vulkan/vulkan_xlib_xrandr.h>
}
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
	inline VkImage CurrentBackBuffer() const { if (!mFrameData) return VK_NULL_HANDLE; return mFrameData[mCurrentBackBufferIndex].mSwapchainImage; }
	inline VkImageView BackBufferView(uint32_t i) const { if (!mFrameData) return VK_NULL_HANDLE; return mFrameData[i].mSwapchainImageView; }
	inline uint32_t BackBufferCount() const { return mImageCount; }
	inline VkExtent2D BackBufferSize() const { return mSwapchainSize; }
	inline VkSurfaceKHR Surface() const { return mSurface; }
	inline VkSurfaceFormatKHR Format() const { return mFormat; }

	inline bool ShouldClose() const { return mWindow ? glfwWindowShouldClose(mWindow) : false; }
	inline ::Device* Device() const { return mDevice; }

private:
	friend class Camera;
	friend class VkCAVE;
	friend class Instance;
	ENGINE_EXPORT Window(Instance* instance, const std::string& title, MouseKeyboardInput* input, VkRect2D position);
	ENGINE_EXPORT Window(Instance* instance, VkPhysicalDevice device, uint32_t displayIndex);
	ENGINE_EXPORT VkImage AcquireNextImage(std::vector<std::shared_ptr<Semaphore>>& signalSemaphores);
	ENGINE_EXPORT void Present(const std::vector<VkSemaphore>& waitSemaphores);

	ENGINE_EXPORT static void WindowPosCallback(GLFWwindow* window, int x, int y);
	ENGINE_EXPORT static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);
	ENGINE_EXPORT static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	ENGINE_EXPORT static void CursorPosCallback(GLFWwindow* window, double x, double y);
	ENGINE_EXPORT static void ScrollCallback(GLFWwindow* window, double x, double y);
	ENGINE_EXPORT static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

	MouseKeyboardInput* mInput;

	bool mFullscreen;
	VkRect2D mWindowedRect;
	VkRect2D mClientRect;
	std::string mTitle;

	Instance* mInstance;
	::Device* mDevice;
	VkPhysicalDevice mPhysicalDevice;

	#ifndef WINDOWS
	x11::Display* mXDisplay;
	#endif

	GLFWwindow* mWindow;
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

	// semaphores for detecting when a swapchain image becomes available
	std::vector<std::shared_ptr<Semaphore>> mImageAvailableSemaphores;
	uint32_t mImageAvailableSemaphoreIndex;

	Camera* mTargetCamera;

	ENGINE_EXPORT void CreateSwapchain(::Device* device);
	ENGINE_EXPORT void DestroySwapchain();

	ENGINE_EXPORT GLFWmonitor* GetCurrentMonitor(const GLFWvidmode** mode) const;
};