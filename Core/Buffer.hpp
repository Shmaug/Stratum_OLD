#pragma once

#include <Core/Device.hpp>
#include <Util/Util.hpp>

class Buffer {
public:
	const std::string mName;

	ENGINE_EXPORT Buffer(const std::string& name, ::Device* device, const void* data, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	ENGINE_EXPORT Buffer(const std::string& name, ::Device* device, const void* data, VkDeviceSize size, VkBufferUsageFlags usage, VkFormat viewFormat, VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	ENGINE_EXPORT Buffer(const std::string& name, ::Device* device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	ENGINE_EXPORT Buffer(const std::string& name, ::Device* device, VkDeviceSize size, VkBufferUsageFlags usage, VkFormat viewFormat, VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	ENGINE_EXPORT Buffer(const Buffer& src);
	ENGINE_EXPORT ~Buffer();

	ENGINE_EXPORT void Upload(const void* data, VkDeviceSize size);

	inline void* MappedData() const { return mMemory.mMapped; }

	inline VkDeviceSize Size() const { return mSize; }
	inline VkBufferUsageFlags Usage() const { return mUsageFlags; }
	inline VkMemoryPropertyFlags MemoryProperties() const { return mMemoryProperties; }
	inline const DeviceMemoryAllocation& Memory() const { return mMemory; }

	ENGINE_EXPORT void CopyFrom(const Buffer& other);
	Buffer& operator=(const Buffer& other) = delete;

	inline const VkBufferView& View() const { return mView; }

	inline ::Device* Device() const { return mDevice; }
	inline operator VkBuffer() const { return mBuffer; }

private:
	::Device* mDevice;
	VkBuffer mBuffer;
	DeviceMemoryAllocation mMemory;

	VkBufferView mView;
	VkFormat mViewFormat;

	VkDeviceSize mSize;

	VkBufferUsageFlags mUsageFlags;
	VkMemoryPropertyFlags mMemoryProperties;

	ENGINE_EXPORT void Allocate();
};