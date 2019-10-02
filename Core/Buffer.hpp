#pragma once

#include <Util/Util.hpp>
#include <Core/Device.hpp>

class Buffer {
public:
	const std::string mName;

	ENGINE_EXPORT Buffer(const std::string& name, Device* device, const void* data, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	ENGINE_EXPORT Buffer(const std::string& name, Device* device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	ENGINE_EXPORT Buffer(const Buffer& src);
	ENGINE_EXPORT ~Buffer();

	ENGINE_EXPORT void* Map();
	ENGINE_EXPORT void Unmap();

	ENGINE_EXPORT void Upload(const void* data, VkDeviceSize size);

	inline void* MappedData() const { return mMappedData; }

	inline VkDeviceSize Size() const { return mSize; }

	ENGINE_EXPORT Buffer& operator=(const Buffer& other);

	inline operator VkBuffer() const { return mBuffer; }

private:
	Device* mDevice;
	VkBuffer mBuffer;
	VkDeviceMemory mMemory;

	void* mMappedData;
	VkDeviceSize mSize;

	VkBufferUsageFlags mUsageFlags;
	VkMemoryPropertyFlags mMemoryFlags;

	ENGINE_EXPORT void Allocate();
};