#include <cmath>

#include <Content/Texture.hpp>

#include <Core/Buffer.hpp>
#include <Core/CommandBuffer.hpp>
#include <Util/Util.hpp>
#include <ThirdParty/stb_image.h>

using namespace std;

Texture::Texture(const string& name, ::DeviceManager* devices, const string& filename, bool srgb) : mName(name) {
	uint8_t* pixels = nullptr;
	uint32_t size = 0;
	int32_t x, y, channels;
	stbi_info(filename.c_str(), &x, &y, &channels);
	int desiredChannels = 0;
	bool stbifree = false;
	if (channels == 3) desiredChannels = 4;

	size_t h = 0;
	hash_combine(h, filename);
	hash_combine(h, srgb);
	hash_combine(h, x);
	hash_combine(h, y);
	hash_combine(h, channels);
	string hfilename = "Cache/" + to_string(h);

	std::ifstream cachef(hfilename, std::ios::binary);
	if (cachef.is_open()) {
		int32_t cx, cy, cchannels;
		bool csrgb;

		cachef.read(reinterpret_cast<char*>(&cx), sizeof(int32_t));
		cachef.read(reinterpret_cast<char*>(&cy), sizeof(int32_t));
		cachef.read(reinterpret_cast<char*>(&cchannels), sizeof(int32_t));
		cachef.read(reinterpret_cast<char*>(&csrgb), sizeof(bool));
		
		if (x == cx && y == cy && channels == cchannels && srgb == csrgb) {
			cachef.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));
			uint64_t imgsize;
			cachef.read(reinterpret_cast<char*>(&imgsize), sizeof(uint64_t));
			pixels = new uint8_t[imgsize];
			cachef.read(reinterpret_cast<char*>(pixels), imgsize);
		}

		cachef.close();
	}

	if (!pixels) {
		if (!srgb && stbi_is_16_bit(filename.c_str())) {
			pixels = (uint8_t*)stbi_load_16(filename.c_str(), &x, &y, &channels, desiredChannels);
			size = 2;
		} else if (!srgb && stbi_is_hdr(filename.c_str())) {
			pixels = (uint8_t*)stbi_loadf(filename.c_str(), &x, &y, &channels, desiredChannels);
			size = 4;
		} else {
			pixels = (uint8_t*)stbi_load(filename.c_str(), &x, &y, &channels, desiredChannels);
			size = 1;
		}
		if (!pixels) throw runtime_error("Failed to load image");
		if (desiredChannels > 0) channels = desiredChannels;

		stbifree = true;

		uint64_t imgsize = x * y * size * channels;

		ofstream cachew(hfilename, ios::binary);
		cachew.write(reinterpret_cast<const char*>(&x), sizeof(int32_t));
		cachew.write(reinterpret_cast<const char*>(&y), sizeof(int32_t));
		cachew.write(reinterpret_cast<const char*>(&channels), sizeof(int32_t));
		cachew.write(reinterpret_cast<const char*>(&srgb), sizeof(bool));
		cachew.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
		cachew.write(reinterpret_cast<const char*>(&imgsize), sizeof(uint64_t));
		cachew.write((const char*)pixels, imgsize);
	}

	if (srgb) {
		const VkFormat formatMap[4] {
			VK_FORMAT_R8_SRGB, VK_FORMAT_R8G8_SRGB, VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
		};
		mFormat = formatMap[channels - 1];
	} else {
		const VkFormat formatMap[4][4]{
			{ VK_FORMAT_R8_UNORM  , VK_FORMAT_R8G8_UNORM   , VK_FORMAT_R8G8B8_UNORM    , VK_FORMAT_R8G8B8A8_UNORM      },
			{ VK_FORMAT_R16_UNORM , VK_FORMAT_R16G16_UNORM , VK_FORMAT_R16G16B16_UNORM , VK_FORMAT_R16G16B16A16_UNORM  },
			{ VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT },
			{ VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT },
		};
		mFormat = formatMap[size - 1][channels - 1];
	}

	mWidth = x;
	mHeight = y;
	mDepth = 1;
	mMipLevels = (uint32_t)std::floor(std::log2(std::max(mWidth, mHeight))) + 1;
	mSampleCount = VK_SAMPLE_COUNT_1_BIT;
	mTiling = VK_IMAGE_TILING_OPTIMAL;
	mUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	mMemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	for (uint32_t i = 0; i < devices->DeviceCount(); i++)
		mDeviceData.emplace(devices->GetDevice(i), DeviceData());

	CreateImage();
	CreateImageView(VK_IMAGE_ASPECT_COLOR_BIT);
	
	VkBufferImageCopy copyRegion = {};
	copyRegion.bufferOffset = 0;
	copyRegion.bufferRowLength = 0;
	copyRegion.bufferImageHeight = 0;
	copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.imageSubresource.mipLevel = 0;
	copyRegion.imageSubresource.baseArrayLayer = 0;
	copyRegion.imageSubresource.layerCount = 1;
	copyRegion.imageOffset = { 0, 0, 0 };
	copyRegion.imageExtent = { mWidth, mHeight, 1 };

	vector<shared_ptr<Buffer>> uploadBuffers;
	vector<shared_ptr<Fence>> fences;
	VkDeviceSize dataSize = mWidth * mHeight * size * channels;
	for (uint32_t i = 0; i < devices->DeviceCount(); i++) {
		auto& d = mDeviceData.at(devices->GetDevice(i));

		shared_ptr<Buffer> uploadBuffer = make_shared<Buffer>(name + " Copy", devices->GetDevice(i), pixels, dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		uploadBuffers.push_back(uploadBuffer);

		auto commandBuffer = devices->GetDevice(i)->GetCommandBuffer();
		TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandBuffer.get());
		vkCmdCopyBufferToImage(*commandBuffer, *uploadBuffer.get(), d.mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
		GenerateMipMaps(commandBuffer.get());
		fences.push_back(devices->GetDevice(i)->Execute(commandBuffer));
	}
	for (auto& f : fences)
		f->Wait();

	if (stbifree)
		stbi_image_free(pixels);
	else
		safe_delete_array(pixels);

	printf("Loaded %s: %dx%d %s (%.1fkb)\n", filename.c_str(), mWidth, mHeight, FormatToString(mFormat), dataSize / 1000.f);
}
Texture::Texture(const string& name, DeviceManager* devices, void* pixels, VkDeviceSize imageSize, uint32_t width, uint32_t height, uint32_t depth, VkFormat format, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties)
	: mName(name), mWidth(width), mHeight(height), mDepth(depth), mMipLevels(mipLevels), mFormat(format), mSampleCount(numSamples), mTiling(tiling), mUsage(usage), mMemoryProperties(properties) {
	if (mipLevels == 0) {
		mMipLevels = (uint32_t)std::floor(std::log2(std::max(mWidth, mHeight))) + 1;
		mUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}
	mUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	for (uint32_t i = 0; i < devices->DeviceCount(); i++)
		mDeviceData.emplace(devices->GetDevice(i), DeviceData());
	CreateImage();
	CreateImageView(VK_IMAGE_ASPECT_COLOR_BIT);

	VkBufferImageCopy copyRegion = {};
	copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.imageSubresource.layerCount = 1;
	copyRegion.imageExtent = { mWidth, mHeight, mDepth };

	vector<shared_ptr<Buffer>> uploadBuffers;
	vector<shared_ptr<Fence>> fences;
	for (uint32_t i = 0; i < devices->DeviceCount(); i++) {
		auto& d = mDeviceData.at(devices->GetDevice(i));

		shared_ptr<Buffer> uploadBuffer = make_shared<Buffer>(name + " Copy", devices->GetDevice(i), pixels, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		uploadBuffers.push_back(uploadBuffer);

		auto commandBuffer = devices->GetDevice(i)->GetCommandBuffer();
		TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandBuffer.get());
		vkCmdCopyBufferToImage(*commandBuffer, *uploadBuffer.get(), d.mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		if (mipLevels == 0)
			GenerateMipMaps(commandBuffer.get());
		else
			TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0 ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer.get());

		fences.push_back(devices->GetDevice(i)->Execute(commandBuffer));
	}
	for (auto& f : fences)
		f->Wait();

}
Texture::Texture(const string& name, Device* device, void* pixels, VkDeviceSize imageSize, uint32_t width, uint32_t height, uint32_t depth, VkFormat format, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties)
	: mName(name), mWidth(width), mHeight(height), mDepth(depth), mMipLevels(mipLevels), mFormat(format), mSampleCount(numSamples), mTiling(tiling), mUsage(usage), mMemoryProperties(properties) {
	if (mipLevels == 0) {
		mMipLevels = (uint32_t)std::floor(std::log2(std::max(mWidth, mHeight))) + 1;
		mUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}
	mUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	mDeviceData.emplace(device, DeviceData());
	CreateImage();
	CreateImageView(VK_IMAGE_ASPECT_COLOR_BIT);

	VkBufferImageCopy copyRegion = {};
	copyRegion.bufferOffset = 0;
	copyRegion.bufferRowLength = 0;
	copyRegion.bufferImageHeight = 0;
	copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.imageSubresource.mipLevel = 0;
	copyRegion.imageSubresource.baseArrayLayer = 0;
	copyRegion.imageSubresource.layerCount = 1;
	copyRegion.imageOffset = { 0, 0, 0 };
	copyRegion.imageExtent = { mWidth, mHeight, mDepth };

	auto& d = mDeviceData.at(device);

	Buffer* uploadBuffer = new Buffer(name + " Copy", device, pixels, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	auto commandBuffer = device->GetCommandBuffer();
	TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandBuffer.get());
	vkCmdCopyBufferToImage(*commandBuffer, *uploadBuffer, d.mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

	if (mipLevels == 0)
		GenerateMipMaps(commandBuffer.get());
	else
		TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0 ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer.get());

	device->Execute(commandBuffer)->Wait();

	delete uploadBuffer;
}

Texture::Texture(const string& name, DeviceManager* devices, uint32_t width, uint32_t height, uint32_t depth, VkFormat format, VkSampleCountFlagBits numSamples, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties)
	: mName(name), mWidth(width), mHeight(height), mDepth(depth), mMipLevels(1), mFormat(format), mSampleCount(numSamples), mTiling(tiling), mUsage(usage), mMemoryProperties(properties) {
	
	for (uint32_t i = 0; i < devices->DeviceCount(); i++)
		mDeviceData.emplace(devices->GetDevice(i), DeviceData());
	CreateImage();

	VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	if (mUsage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if (HasStencilComponent(mFormat))
		aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	CreateImageView(aspect);
}
Texture::Texture(const string& name, Device* device, uint32_t width, uint32_t height, uint32_t depth, VkFormat format, VkSampleCountFlagBits numSamples, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties)
	: mName(name), mWidth(width), mHeight(height), mDepth(depth), mMipLevels(1), mFormat(format), mSampleCount(numSamples), mTiling(tiling), mUsage(usage), mMemoryProperties(properties) {

	mDeviceData.emplace(device, DeviceData());
	CreateImage();

	VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	if (mUsage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if (HasStencilComponent(mFormat))
		aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	CreateImageView(aspect);
}

Texture::~Texture() {
	for (const auto& d : mDeviceData) {
		vkDestroyImage(*d.first, d.second.mImage, nullptr);
		vkDestroyImageView(*d.first, d.second.mView, nullptr);
		vkFreeMemory(*d.first, d.second.mImageMemory, nullptr);
	}
}

void Texture::GenerateMipMaps(CommandBuffer* commandBuffer) {
	auto& d = mDeviceData.at(commandBuffer->Device());

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = d.mImage;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = mWidth;
	int32_t mipHeight = mHeight;
	int32_t mipDepth = mDepth;

	for (uint32_t i = 1; i < mMipLevels; i++) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(*commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		VkImageBlit blit = {};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { mipWidth, mipHeight, mipDepth };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, mipDepth > 1 ? mipDepth / 2 : 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;

		vkCmdBlitImage(*commandBuffer,
			d.mImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			d.mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(*commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;
		if (mipDepth > 1) mipDepth /= 2;
	}

	barrier.subresourceRange.baseMipLevel = mMipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(*commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
		0, nullptr,
		0, nullptr,
		1, &barrier);
}

void Texture::CreateImage() {
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = mDepth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = mWidth;
	imageInfo.extent.height = mHeight;
	imageInfo.extent.depth = mDepth;
	imageInfo.mipLevels = mMipLevels;
	imageInfo.arrayLayers = 1;
	imageInfo.format = mFormat;
	imageInfo.tiling = mTiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = mUsage;
	imageInfo.samples = mSampleCount;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	for (auto& d : mDeviceData) {
		ThrowIfFailed(vkCreateImage(*d.first, &imageInfo, nullptr, &d.second.mImage), "vkCreateImage failed for " + mName);
		d.first->SetObjectName(d.second.mImage, mName, VK_OBJECT_TYPE_IMAGE);

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(*d.first, d.second.mImage, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = d.first->FindMemoryType(memRequirements.memoryTypeBits, mMemoryProperties);

		ThrowIfFailed(vkAllocateMemory(*d.first, &allocInfo, nullptr, &d.second.mImageMemory), "vkAllocateMemory failed for " + mName);
		d.first->SetObjectName(d.second.mImageMemory, mName + " Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
		vkBindImageMemory(*d.first, d.second.mImage, d.second.mImageMemory, 0);
	}
}

void Texture::CreateImageView(VkImageAspectFlags aspectFlags) {
	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.viewType = mDepth > 1 ? VK_IMAGE_VIEW_TYPE_3D :VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = mFormat;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mMipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	for (auto& d : mDeviceData) {
		viewInfo.image = d.second.mImage;
		ThrowIfFailed(vkCreateImageView(*d.first, &viewInfo, nullptr, &d.second.mView), "vkCreateImageView failed for " + mName);
		d.first->SetObjectName(d.second.mView, mName + " View", VK_OBJECT_TYPE_IMAGE_VIEW);
	}
}

void Texture::TransitionImageLayout(VkImage image, VkFormat format, uint32_t mipLevels, VkImageLayout oldLayout, VkImageLayout newLayout, CommandBuffer* commandBuffer) {
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;

	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		if (HasStencilComponent(format))
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	} else
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mipLevels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkPipelineStageFlags destinationStage;
	VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

	switch (oldLayout) {
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		break;
	case VK_IMAGE_LAYOUT_GENERAL:
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		break;
	}
	switch (newLayout) {
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		break;
	case VK_IMAGE_LAYOUT_GENERAL:
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		break;
	default:
		throw invalid_argument("unsupported layout transition!");
	}

	vkCmdPipelineBarrier(*commandBuffer,
		srcStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);
}
void Texture::TransitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout, CommandBuffer* commandBuffer) {
	auto& d = mDeviceData.at(commandBuffer->Device());
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = d.mImage;

	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		if (HasStencilComponent(mFormat))
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	} else
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mMipLevels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkPipelineStageFlags destinationStage;
	VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

	switch (oldLayout) {
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		break;
	case VK_IMAGE_LAYOUT_GENERAL:
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		break;
	}
	switch (newLayout) {
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		break;
	case VK_IMAGE_LAYOUT_GENERAL:
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		break;
	default:
		throw invalid_argument("unsupported layout transition!");
	}

	vkCmdPipelineBarrier(*commandBuffer,
		srcStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);
}