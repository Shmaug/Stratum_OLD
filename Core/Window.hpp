#pragma once

#include <GLFW/glfw3.h>

#include <Content/Texture.hpp>
#include <Core/CommandBuffer.hpp>
#include <Input/MouseKeyboardInput.hpp>
#include <Util/Util.hpp>

class DeviceManager;
class Camera;

class Window {
public:
	ENGINE_EXPORT ~Window();

	ENGINE_EXPORT VkImage AcquireNextImage();
	ENGINE_EXPORT void Present(const std::vector<std::shared_ptr<Fence>>& fences);

	inline bool Fullscreen() const { return mFullscreen; }
	ENGINE_EXPORT void Fullscreen(bool fs);

	inline VkRect2D ClientRect() const { return mClientRect; };

	inline std::string Title() const { return mTitle; }

	inline MouseKeyboardInput* Input() const { return mInput; }

	inline uint32_t CurrentBackBufferIndex() const { return mCurrentBackBufferIndex; }
	inline VkImage CurrentBackBuffer() const { if (!mFrameData) return VK_NULL_HANDLE; return mFrameData[mCurrentBackBufferIndex].mSwapchainImage; }
	inline VkImageView BackBufferView(uint32_t i) const { if (!mFrameData) return VK_NULL_HANDLE; return mFrameData[i].mSwapchainImageView; }
	inline uint32_t BackBufferCount() const { return mImageCount; }
	inline VkExtent2D BackBufferSize() const { return mSwapchainSize; }
	inline VkSurfaceKHR Surface() const { return mSurface; }
	inline VkSurfaceFormatKHR Format() const { return mFormat; }

	inline bool ShouldClose() const { return glfwWindowShouldClose(mWindow); }
	inline ::Device* Device() const { return mDevice; }

private:
	friend class Camera;
	friend class DeviceManager;
	ENGINE_EXPORT Window(VkInstance instance, const std::string& title, MouseKeyboardInput* input, VkRect2D position, int monitorIndex = -1);

	ENGINE_EXPORT static void WindowPosCallback(GLFWwindow* window, int x, int y);
	ENGINE_EXPORT static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);
	ENGINE_EXPORT static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	ENGINE_EXPORT static void CursorPosCallback(GLFWwindow* window, double x, double y);
	ENGINE_EXPORT static void ScrollCallback(GLFWwindow* window, double x, double y);
	ENGINE_EXPORT static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

	bool mFullscreen;
	VkRect2D mWindowedRect;
	VkRect2D mClientRect;
	std::string mTitle;

	MouseKeyboardInput* mInput;

	VkInstance mInstance;
	::Device* mDevice;

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
		std::vector<std::shared_ptr<Fence>> mFences;
		inline FrameData() : mSwapchainImage(VK_NULL_HANDLE), mSwapchainImageView(VK_NULL_HANDLE) {};
	};
	FrameData* mFrameData;

	// semaphores for detecting when a swapchain image becomes available
	std::vector<VkSemaphore> mImageAvailableSemaphores;
	// next semaphore in mImageAvailableSemaphores
	uint32_t mImageAvailableSemaphoreIndex;

	Camera* mTargetCamera;

	ENGINE_EXPORT void CreateSwapchain(::Device* device);
	ENGINE_EXPORT void DestroySwapchain();

	ENGINE_EXPORT GLFWmonitor* GetCurrentMonitor(const GLFWvidmode** mode) const;
};