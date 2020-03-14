#include <Core/Buffer.hpp>
#include <Util/Util.hpp>

#include <cstring>

using namespace std;

Buffer::Buffer(const std::string& name, ::Device* device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
	: mName(name), mDevice(device), mSize(size), mUsageFlags(usage), mMemoryProperties(properties), mBuffer(VK_NULL_HANDLE), mView(VK_NULL_HANDLE), mViewFormat(VK_FORMAT_UNDEFINED), mMemory({}) {
	Allocate();
}
Buffer::Buffer(const std::string& name, ::Device* device, VkDeviceSize size, VkBufferUsageFlags usage, VkFormat viewFormat, VkMemoryPropertyFlags properties)
	: mName(name), mDevice(device), mSize(size), mUsageFlags(usage), mMemoryProperties(properties), mBuffer(VK_NULL_HANDLE), mView(VK_NULL_HANDLE), mViewFormat(viewFormat), mMemory({}) {
	Allocate();
}
Buffer::Buffer(const std::string& name, ::Device* device, const void* data, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
	: mName(name), mDevice(device), mSize(size), mUsageFlags(usage), mMemoryProperties(properties), mBuffer(VK_NULL_HANDLE), mView(VK_NULL_HANDLE), mViewFormat(VK_FORMAT_UNDEFINED), mMemory({}) {
	if ((properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
		mUsageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	Allocate();
	Upload(data, size);
}
Buffer::Buffer(const std::string& name, ::Device* device, const void* data, VkDeviceSize size, VkBufferUsageFlags usage, VkFormat viewFormat, VkMemoryPropertyFlags properties)
	: mName(name), mDevice(device), mSize(size), mUsageFlags(usage), mMemoryProperties(properties), mBuffer(VK_NULL_HANDLE), mView(VK_NULL_HANDLE), mViewFormat(viewFormat), mMemory({}) {
	if ((properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
		mUsageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	Allocate();
	Upload(data, size);
}
Buffer::Buffer(const Buffer& src)
	: mName(src.mName), mDevice(src.mDevice), mSize(0), mUsageFlags(src.mUsageFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT), mMemoryProperties(src.mMemoryProperties),
	mBuffer(VK_NULL_HANDLE), mView(VK_NULL_HANDLE), mViewFormat(src.mViewFormat), mMemory({}) {
	CopyFrom(src);
}
Buffer::~Buffer() {
	if (mView) vkDestroyBufferView(*mDevice, mView, nullptr);
	if (mBuffer) vkDestroyBuffer(*mDevice, mBuffer, nullptr);
	mDevice->FreeMemory(mMemory);
}

void Buffer::Upload(const void* data, VkDeviceSize size) {
	if (!data) return;
	if (size > mSize) throw runtime_error("Data size out of bounds");
	if (mMemoryProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
		memcpy(MappedData(), data, size);
	} else {
		if ((mUsageFlags & VK_BUFFER_USAGE_TRANSFER_DST_BIT) == 0) {
			mUsageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			
			if (mView) vkDestroyBufferView(*mDevice, mView, nullptr);
			if (mBuffer) vkDestroyBuffer(*mDevice, mBuffer, nullptr);
			mDevice->FreeMemory(mMemory);
			mSize = size;
			Allocate();
		}
		Buffer uploadBuffer(mName + " upload", mDevice, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		memcpy(uploadBuffer.MappedData(), data, size);
		CopyFrom(uploadBuffer);
	}
}

void Buffer::CopyFrom(const Buffer& other) {
	if (mSize != other.mSize) {
		if (mView) vkDestroyBufferView(*mDevice, mView, nullptr);
		if (mBuffer) vkDestroyBuffer(*mDevice, mBuffer, nullptr);
		mDevice->FreeMemory(mMemory);
		mSize = other.mSize;
		Allocate();
	}

	auto commandBuffer = mDevice->GetCommandBuffer();

	VkBufferMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.buffer = other;
	barrier.size = mSize;
	vkCmdPipelineBarrier(*commandBuffer,
		VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, nullptr,
		1, &barrier,
		0, nullptr);

	VkBufferCopy copyRegion = {};
	copyRegion.size = mSize;
	vkCmdCopyBuffer(*commandBuffer, other.mBuffer, mBuffer, 1, &copyRegion);
	mDevice->Execute(commandBuffer, false)->Wait();
}

void Buffer::Allocate(){
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = mSize;
	bufferInfo.usage = mUsageFlags;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ThrowIfFailed(vkCreateBuffer(*mDevice, &bufferInfo, nullptr, &mBuffer), "vkCreateBuffer failed for " + mName);
	mDevice->SetObjectName(mBuffer, mName, VK_OBJECT_TYPE_BUFFER);

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(*mDevice, mBuffer, &memRequirements);
	mMemory = mDevice->AllocateMemory(memRequirements, mMemoryProperties, mName);
	vkBindBufferMemory(*mDevice, mBuffer, mMemory.mDeviceMemory, mMemory.mOffset);

	if (mViewFormat != VK_FORMAT_UNDEFINED) {
		VkBufferViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
		viewInfo.buffer = mBuffer;
		viewInfo.offset = 0;
		viewInfo.range = mSize;
		viewInfo.format = mViewFormat;
		ThrowIfFailed(vkCreateBufferView(*mDevice, &viewInfo, nullptr, &mView), "vkCreateBufferView failed for " + mName);
		mDevice->SetObjectName(mView, mName, VK_OBJECT_TYPE_BUFFER_VIEW);
	}
}