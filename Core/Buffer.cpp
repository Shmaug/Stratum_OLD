#include <Core/Buffer.hpp>
#include <Util/Util.hpp>

#include <cstring>

using namespace std;

Buffer::Buffer(const std::string& name, ::Device* device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryFlags)
	: mName(name), mDevice(device), mSize(size), mUsageFlags(usage), mMemoryFlags(memoryFlags), mMappedData(nullptr), mBuffer(VK_NULL_HANDLE), mMemory(VK_NULL_HANDLE) {
	Allocate();
}
Buffer::Buffer(const std::string& name, ::Device* device, const void* data, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryFlags)
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
	if (mMemory != VK_NULL_HANDLE) {
		vkFreeMemory(*mDevice, mMemory, nullptr);
		#ifdef PRINT_VK_ALLOCATIONS
		fprintf_color(COLOR_BLUE, stdout, "Freed %.1fkb for %s\n", mAllocationInfo.allocationSize / 1024.f, mName.c_str());
		#endif
	}
}

void* Buffer::Map() {
	if (mMappedData) return mMappedData;
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
		if (mMappedData) {
			memcpy(mMappedData, data, size);
		} else {
			memcpy(Map(), data, size);
			Unmap();
		}
	} else {
		if ((mUsageFlags & VK_BUFFER_USAGE_TRANSFER_DST_BIT) == 0) {
			mUsageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			
			if (mBuffer) vkDestroyBuffer(*mDevice, mBuffer, nullptr);
			if (mMemory) {
				vkFreeMemory(*mDevice, mMemory, nullptr);
				#ifdef PRINT_VK_ALLOCATIONS
				fprintf_color(COLOR_BLUE, stdout, "Freed %.1fkb for %s\n", mAllocationInfo.allocationSize / 1024.f, mName.c_str());
				#endif
			}
			mSize = size;
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
		if (mMemory) {
			vkFreeMemory(*mDevice, mMemory, nullptr);
			#ifdef PRINT_VK_ALLOCATIONS
			fprintf_color(COLOR_BLUE, stdout, "Freed %.1fkb for %s\n", mAllocationInfo.allocationSize / 1024.f, mName.c_str());
			#endif
		}
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

	mAllocationInfo = {};
	mAllocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mAllocationInfo.allocationSize = memRequirements.size;
	mAllocationInfo.memoryTypeIndex = mDevice->FindMemoryType(memRequirements.memoryTypeBits, mMemoryFlags);
	ThrowIfFailed(vkAllocateMemory(*mDevice, &mAllocationInfo, nullptr, &mMemory), "vkAllocateMemory failed for " + mName);
	#ifdef PRINT_VK_ALLOCATIONS
	fprintf_color(COLOR_YELLOW, stdout, "Allocated %.1fkb for %s\n", mAllocationInfo.allocationSize / 1024.f, mName.c_str());
	#endif

	vkBindBufferMemory(*mDevice, mBuffer, mMemory, 0);
	mDevice->SetObjectName(mMemory, mName + " Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
}