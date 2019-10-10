#include <Core/Buffer.hpp>
#include <Core/Buffer.hpp>
#include <Util/Util.hpp>

using namespace std;

Buffer::Buffer(const std::string& name, Device* device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryFlags)
	: mName(name), mDevice(device), mSize(size), mUsageFlags(usage), mMemoryFlags(memoryFlags), mMappedData(nullptr), mBuffer(VK_NULL_HANDLE), mMemory(VK_NULL_HANDLE) {
	Allocate();
}
Buffer::Buffer(const std::string& name, Device* device, const void* data, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryFlags)
	: mName(name), mDevice(device), mSize(size), mUsageFlags(usage), mMemoryFlags(memoryFlags), mMappedData(nullptr), mBuffer(VK_NULL_HANDLE), mMemory(VK_NULL_HANDLE) {
	if ((memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
		mUsageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	Allocate();
	Upload(data, size);
}
Buffer::Buffer(const Buffer& src)
	: mName(src.mName), mDevice(src.mDevice), mSize(0), mUsageFlags(src.mUsageFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT), mMemoryFlags(src.mMemoryFlags), mMappedData(nullptr), mBuffer(VK_NULL_HANDLE), mMemory(VK_NULL_HANDLE) {
	CopyFrom(src);
}
Buffer::~Buffer() {
	if (mMappedData) Unmap();
	if (mBuffer != VK_NULL_HANDLE) vkDestroyBuffer(*mDevice, mBuffer, nullptr);
	if (mMemory != VK_NULL_HANDLE) vkFreeMemory(*mDevice, mMemory, nullptr);
}

void* Buffer::Map() {
	vkMapMemory(*mDevice, mMemory, 0, mSize, 0, &mMappedData);
	return mMappedData;
}
void Buffer::Unmap() {
	vkUnmapMemory(*mDevice, mMemory);
	mMappedData = nullptr;
}
void Buffer::Upload(const void* data, VkDeviceSize size) {
	if (!data) return;
	if (size > mSize) throw runtime_error("Data size out of bounds");
	if (mMemoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
		memcpy(Map(), data, size);
		Unmap();
	} else {
		if ((mUsageFlags & VK_BUFFER_USAGE_TRANSFER_DST_BIT) == 0) {
			mUsageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			Allocate();
		}
		Buffer uploadBuffer(mName + " upload", mDevice, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		memcpy(uploadBuffer.Map(), data, size);
		uploadBuffer.Unmap();
		CopyFrom(uploadBuffer);
	}
}

void Buffer::CopyFrom(const Buffer& other) {
	if (mMappedData) Unmap();
	if (mSize != other.mSize) {
		if (mBuffer) vkDestroyBuffer(*mDevice, mBuffer, nullptr);
		if (mMemory) vkFreeMemory(*mDevice, mMemory, nullptr);
		mSize = other.mSize;
		Allocate();
	}

	auto commandBuffer = mDevice->GetCommandBuffer();
	VkBufferCopy copyRegion = {};
	copyRegion.size = mSize;
	vkCmdCopyBuffer(*commandBuffer, other.mBuffer, mBuffer, 1, &copyRegion);
	mDevice->Execute(commandBuffer)->Wait();
}

void Buffer::Allocate(){
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = mSize;
	bufferInfo.usage = mUsageFlags;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	ThrowIfFailed(vkCreateBuffer(*mDevice, &bufferInfo, nullptr, &mBuffer));
	mDevice->SetObjectName(mBuffer, mName);

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(*mDevice, mBuffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = mDevice->FindMemoryType(memRequirements.memoryTypeBits, mMemoryFlags);

	ThrowIfFailed(vkAllocateMemory(*mDevice, &allocInfo, nullptr, &mMemory));

	vkBindBufferMemory(*mDevice, mBuffer, mMemory, 0);
	mDevice->SetObjectName(mMemory, mName + " Memory");
}