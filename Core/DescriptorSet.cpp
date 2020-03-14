#include <Core/DescriptorSet.hpp>
#include <Core/Device.hpp>
#include <Core/Buffer.hpp>
#include <Content/Texture.hpp>

using namespace std;

DescriptorSet::DescriptorSet(const string& name, Device* device, VkDescriptorSetLayout layout) : mDevice(device), mLayout(layout) {
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = mDevice->mDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout;
	lock_guard lock(device->mDescriptorPoolMutex);
	ThrowIfFailed(vkAllocateDescriptorSets(*mDevice, &allocInfo, &mDescriptorSet), "vkAllocateDescriptorSets failed");
	mDevice->SetObjectName(mDescriptorSet, name, VK_OBJECT_TYPE_DESCRIPTOR_SET);
	mDevice->mDescriptorSetCount++;
}
DescriptorSet::~DescriptorSet() {
	for (auto c : mCurrent) {
		safe_delete(c.second.pImageInfo);
		safe_delete(c.second.pBufferInfo);
	}

	for (VkDescriptorBufferInfo*& d : mPendingBuffers)
		safe_delete(d);
	while (!mBufferInfoPool.empty()) {
		delete mBufferInfoPool.front();
		mBufferInfoPool.pop();
	}
	for (VkDescriptorImageInfo*& d : mPendingImages)
		safe_delete(d);
	while (!mImageInfoPool.empty()) {
		delete mImageInfoPool.front();
		mImageInfoPool.pop();
	}
	lock_guard lock(mDevice->mDescriptorPoolMutex);
	ThrowIfFailed(vkFreeDescriptorSets(*mDevice, mDevice->mDescriptorPool, 1, &mDescriptorSet), "vkFreeDescriptorSets failed");
	mDevice->mDescriptorSetCount--;
}

void DescriptorSet::CreateStorageBufferDescriptor(Buffer* buffer, uint32_t index, VkDeviceSize offset, VkDeviceSize range, uint32_t binding) {
	uint64_t idx = (uint64_t)binding | ((uint64_t)index << 32);
	if (mCurrent.count(idx)) {
		const VkWriteDescriptorSet& c = mCurrent.at(idx);
		if (c.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER &&
			c.pBufferInfo->buffer == *buffer &&
			c.pBufferInfo->offset == offset &&
			c.pBufferInfo->range == range) return;
	}

	VkDescriptorBufferInfo* info;
	if (mBufferInfoPool.empty())
		info = new VkDescriptorBufferInfo();
	else {
		info = mBufferInfoPool.front();
		mBufferInfoPool.pop();
	}
	info->buffer = *buffer;
	info->offset = offset;
	info->range = range;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = index;
	write.pBufferInfo = info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	write.descriptorCount = 1;
	mPending.push_back(write);
	mPendingBuffers.push_back(info);
}
void DescriptorSet::CreateStorageBufferDescriptor(Buffer* buffer, VkDeviceSize offset, VkDeviceSize range, uint32_t binding) {
	uint64_t idx = (uint64_t)binding;
	if (mCurrent.count(idx)) {
		const VkWriteDescriptorSet& c = mCurrent.at(idx);
		if (c.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER &&
			c.pBufferInfo->buffer == *buffer &&
			c.pBufferInfo->offset == offset &&
			c.pBufferInfo->range == range) return;
	}

	VkDescriptorBufferInfo* info;
	if (mBufferInfoPool.empty())
		info = new VkDescriptorBufferInfo();
	else {
		info = mBufferInfoPool.front();
		mBufferInfoPool.pop();
	}
	info->buffer = *buffer;
	info->offset = offset;
	info->range = range;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pBufferInfo = info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	write.descriptorCount = 1;
	mPending.push_back(write);
	mPendingBuffers.push_back(info);
}
void DescriptorSet::CreateStorageTexelBufferDescriptor(Buffer* buffer, uint32_t binding) {
	uint64_t idx = (uint64_t)binding;
	if (mCurrent.count(idx)) {
		const VkWriteDescriptorSet& c = mCurrent.at(idx);
		if (c.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER && c.pTexelBufferView == &buffer->View()) return;
	}

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pTexelBufferView = &buffer->View();
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
	write.descriptorCount = 1;
	mPending.push_back(write);
}

void DescriptorSet::CreateUniformBufferDescriptor(Buffer* buffer, VkDeviceSize offset, VkDeviceSize range, uint32_t binding) {
	uint64_t idx = (uint64_t)binding;
	if (mCurrent.count(idx)) {
		const VkWriteDescriptorSet& c = mCurrent.at(idx);
		if (c.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER &&
			c.pBufferInfo->buffer == *buffer &&
			c.pBufferInfo->offset == offset &&
			c.pBufferInfo->range == range) return;
	}

	VkDescriptorBufferInfo* info;
	if (mBufferInfoPool.empty())
		info = new VkDescriptorBufferInfo();
	else {
		info = mBufferInfoPool.front();
		mBufferInfoPool.pop();
	}
	info->buffer = *buffer;
	info->offset = offset;
	info->range = range;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pBufferInfo = info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	write.descriptorCount = 1;
	mPending.push_back(write);
	mPendingBuffers.push_back(info);
}

void DescriptorSet::CreateStorageTextureDescriptor(Texture* texture, uint32_t binding, VkImageLayout layout) {
	uint64_t idx = (uint64_t)binding;
	if (mCurrent.count(idx)) {
		const VkWriteDescriptorSet& c = mCurrent.at(idx);
		if (c.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE &&
			c.pImageInfo->imageLayout == layout &&
			c.pImageInfo->imageView == texture->View()) return;
	}

	VkDescriptorImageInfo* info;
	if (mImageInfoPool.empty())
		info = new VkDescriptorImageInfo();
	else {
		info = mImageInfoPool.front();
		mImageInfoPool.pop();
	}
	info->imageLayout = layout;
	info->imageView = texture->View();
	info->sampler = VK_NULL_HANDLE;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pImageInfo = info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	write.descriptorCount = 1;
	mPending.push_back(write);
	mPendingImages.push_back(info);
}
void DescriptorSet::CreateStorageTextureDescriptor(Texture* texture, uint32_t index, uint32_t binding, VkImageLayout layout) {
	uint64_t idx = (uint64_t)binding | ((uint64_t)index << 32);
	if (mCurrent.count(idx)) {
		const VkWriteDescriptorSet& c = mCurrent.at(idx);
		if (c.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE &&
			c.pImageInfo->imageLayout == layout &&
			c.pImageInfo->imageView == texture->View()) return;
	}

	VkDescriptorImageInfo* info;
	if (mImageInfoPool.empty())
		info = new VkDescriptorImageInfo();
	else {
		info = mImageInfoPool.front();
		mImageInfoPool.pop();
	}
	info->imageLayout = layout;
	info->imageView = texture->View();
	info->sampler = VK_NULL_HANDLE;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = index;
	write.pImageInfo = info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	write.descriptorCount = 1;
	mPending.push_back(write);
	mPendingImages.push_back(info);
}
void DescriptorSet::CreateSampledTextureDescriptor(Texture* texture, uint32_t binding, VkImageLayout layout) {
	uint64_t idx = (uint64_t)binding;
	if (mCurrent.count(idx)) {
		const VkWriteDescriptorSet& c = mCurrent.at(idx);
		if (c.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE &&
			c.pImageInfo->imageLayout == layout &&
			c.pImageInfo->imageView == texture->View()) return;
	}

	VkDescriptorImageInfo* info;
	if (mImageInfoPool.empty())
		info = new VkDescriptorImageInfo();
	else {
		info = mImageInfoPool.front();
		mImageInfoPool.pop();
	}
	info->imageLayout = layout;
	info->imageView = texture->View();
	info->sampler = VK_NULL_HANDLE;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pImageInfo = info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	write.descriptorCount = 1;
	mPending.push_back(write);
	mPendingImages.push_back(info);
}
void DescriptorSet::CreateSampledTextureDescriptor(Texture* texture, uint32_t index, uint32_t binding, VkImageLayout layout) {
	uint64_t idx = (uint64_t)binding | ((uint64_t)index << 32);
	if (mCurrent.count(idx)) {
		const VkWriteDescriptorSet& c = mCurrent.at(idx);
		if (c.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE &&
			c.pImageInfo->imageLayout == layout &&
			c.pImageInfo->imageView == texture->View()) return;
	}

	VkDescriptorImageInfo* info;
	if (mImageInfoPool.empty())
		info = new VkDescriptorImageInfo();
	else {
		info = mImageInfoPool.front();
		mImageInfoPool.pop();
	}
	info->imageLayout = layout;
	info->imageView = texture->View();
	info->sampler = VK_NULL_HANDLE;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = index;
	write.pImageInfo = info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	write.descriptorCount = 1;
	mPending.push_back(write);
	mPendingImages.push_back(info);
}

void DescriptorSet::CreateSamplerDescriptor(Sampler* sampler, uint32_t binding) {
	uint64_t idx = (uint64_t)binding;
	if (mCurrent.count(idx)) {
		const VkWriteDescriptorSet& c = mCurrent.at(idx);
		if (c.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER && c.pImageInfo->sampler == *sampler) return;
	}

	VkDescriptorImageInfo* info;
	if (mImageInfoPool.empty())
		info = new VkDescriptorImageInfo();
	else {
		info = mImageInfoPool.front();
		mImageInfoPool.pop();
	}
	info->imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	info->imageView = VK_NULL_HANDLE;
	info->sampler = *sampler;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pImageInfo = info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	write.descriptorCount = 1;
	mPending.push_back(write);
	mPendingImages.push_back(info);
}

void DescriptorSet::FlushWrites() {
	if (mPending.empty()) return;

	vkUpdateDescriptorSets(*mDevice, (uint32_t)mPending.size(), mPending.data(), 0, nullptr);

	for (VkWriteDescriptorSet i : mPending) {
		uint64_t idx = (uint64_t)i.dstBinding | ((uint64_t)i.dstArrayElement << 32);
		if (mCurrent.count(idx)) {
			VkWriteDescriptorSet c = mCurrent.at(idx);
			safe_delete(c.pImageInfo);
			safe_delete(c.pBufferInfo);
		}
		VkWriteDescriptorSet& c = mCurrent[idx];
		c = i;
		if (i.pBufferInfo) {
			VkDescriptorBufferInfo* b = new VkDescriptorBufferInfo();
			memcpy(b, i.pBufferInfo, sizeof(VkDescriptorBufferInfo));
			c.pBufferInfo = b;
		}
		if (i.pImageInfo) {
			VkDescriptorImageInfo* b = new VkDescriptorImageInfo();
			memcpy(b, i.pImageInfo, sizeof(VkDescriptorImageInfo));
			c.pImageInfo = b;
		}
	}

	for (VkDescriptorBufferInfo* d : mPendingBuffers) mBufferInfoPool.push(d);
	for (VkDescriptorImageInfo* d : mPendingImages) mImageInfoPool.push(d);
	
	mPending.clear();
	mPendingImages.clear();
	mPendingBuffers.clear();
}