#include <cmath>

#include <Content/Texture.hpp>

#include <Core/Buffer.hpp>
#include <Core/CommandBuffer.hpp>
#include <Util/Util.hpp>
#include <ThirdParty/stb_image.h>

using namespace std;

uint8_t* load(const string& filename, bool srgb, uint32_t& pixelSize, int32_t& x, int32_t& y, int32_t& channels, VkFormat& format) {
	uint8_t* pixels = nullptr;
	pixelSize = 0;
	x, y, channels;
	stbi_info(filename.c_str(), &x, &y, &channels);

	int desiredChannels = 4;
	if (stbi_is_16_bit(filename.c_str())) {
		pixels = (uint8_t*)stbi_load_16(filename.c_str(), &x, &y, &channels, desiredChannels);
		pixelSize = sizeof(uint16_t);
		srgb = false;
	} else if (stbi_is_hdr(filename.c_str())) {
		pixels = (uint8_t*)stbi_loadf(filename.c_str(), &x, &y, &channels, desiredChannels);
		pixelSize = sizeof(float);
		srgb = false;
	} else {
		pixels = (uint8_t*)stbi_load(filename.c_str(), &x, &y, &channels, desiredChannels);
		pixelSize = sizeof(uint8_t);
	}
	if (!pixels) {
		fprintf_color(COLOR_RED_BOLD, stderr, "Failed to load image: %s\n", filename.c_str());
		throw;
	}
	if (desiredChannels > 0) channels = desiredChannels;

	if (srgb) {
		const VkFormat formatMap[4] {
			VK_FORMAT_R8_SRGB, VK_FORMAT_R8G8_SRGB, VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
		};
		format = formatMap[channels - 1];
	} else {
		const VkFormat formatMap[4][4]{
			{ VK_FORMAT_R8_UNORM  , VK_FORMAT_R8G8_UNORM   , VK_FORMAT_R8G8B8_UNORM    , VK_FORMAT_R8G8B8A8_UNORM      },
			{ VK_FORMAT_R16_UNORM , VK_FORMAT_R16G16_UNORM , VK_FORMAT_R16G16B16_UNORM , VK_FORMAT_R16G16B16A16_UNORM  },
			{ VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT },
			{ VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT },
		};
		format = formatMap[pixelSize - 1][channels - 1];
	}

	return pixels;
}

Texture::Texture(const string& name, Device* device, const string& filename, bool srgb) : mName(name), mDevice(device), mMemory({}) {
	int32_t x, y, channels;
	uint32_t size;
	uint8_t* pixels = load(filename, srgb, size, x, y, channels, mFormat);

	mWidth = x;
	mHeight = y;
	mDepth = 1;
	mArrayLayers = 1;
	mMipLevels = (uint32_t)std::floor(std::log2(std::max(mWidth, mHeight))) + 1;
	mSampleCount = VK_SAMPLE_COUNT_1_BIT;
	mTiling = VK_IMAGE_TILING_OPTIMAL;
	mUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	mMemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

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

	VkDeviceSize dataSize = mWidth * mHeight * size * channels;

	Buffer uploadBuffer(name + " Copy", mDevice, pixels, dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	auto commandBuffer = mDevice->GetCommandBuffer();
	TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandBuffer.get());
	vkCmdCopyBufferToImage(*commandBuffer, uploadBuffer, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
	GenerateMipMaps(commandBuffer.get());
	mDevice->Execute(commandBuffer, false)->Wait();

	stbi_image_free(pixels);

	//printf("Loaded %s: %dx%d %s\n", filename.c_str(), mWidth, mHeight, FormatToString(mFormat));
}
Texture::Texture(const string& name, Device* device, const string& px, const string& nx, const string& py, const string& ny, const string& pz, const string& nz, bool srgb)
	: mName(name), mDevice(device), mMemory({}) {
	int32_t x, y, channels;
	uint32_t size;
	
	uint8_t* pixels[6]{
		load(px, srgb, size, x, y, channels, mFormat),
		load(nx, srgb, size, x, y, channels, mFormat),
		load(py, srgb, size, x, y, channels, mFormat),
		load(ny, srgb, size, x, y, channels, mFormat),
		load(pz, srgb, size, x, y, channels, mFormat),
		load(nz, srgb, size, x, y, channels, mFormat)
	};

	mWidth = x;
	mHeight = y;
	mDepth = 1;
	mArrayLayers = 6;
	mMipLevels = (uint32_t)std::floor(std::log2(std::max(mWidth, mHeight))) + 1;
	mSampleCount = VK_SAMPLE_COUNT_1_BIT;
	mTiling = VK_IMAGE_TILING_OPTIMAL;
	mUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	mMemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	CreateImage();
	CreateImageView(VK_IMAGE_ASPECT_COLOR_BIT);

	VkBufferImageCopy copyRegion = {};
	copyRegion.bufferOffset = 0;
	copyRegion.bufferRowLength = 0;
	copyRegion.bufferImageHeight = 0;
	copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.imageSubresource.mipLevel = 0;
	copyRegion.imageSubresource.baseArrayLayer = 0;
	copyRegion.imageSubresource.layerCount = mArrayLayers;
	copyRegion.imageOffset = { 0, 0, 0 };
	copyRegion.imageExtent = { mWidth, mHeight, 1 };

	VkDeviceSize dataSize = mWidth * mHeight * size * channels;

	auto commandBuffer = mDevice->GetCommandBuffer();
	TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandBuffer.get());
	
	Buffer uploadBuffer(name + " Copy", mDevice, dataSize * mArrayLayers, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	for (uint32_t j = 0; j < mArrayLayers; j++)
		memcpy((uint8_t*)uploadBuffer.MappedData() + j * dataSize, pixels[j], dataSize);

	vkCmdCopyBufferToImage(*commandBuffer, uploadBuffer, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

	GenerateMipMaps(commandBuffer.get());
	mDevice->Execute(commandBuffer, false)->Wait();

	for (uint32_t i = 0; i < 6; i++)
		stbi_image_free(pixels[i]);

	//printf("Loaded Cubemap %s: %dx%d %s\n", nx.c_str(), mWidth, mHeight, FormatToString(mFormat));
}

Texture::Texture(const string& name, Device* device, void* pixels, VkDeviceSize imageSize, uint32_t width, uint32_t height, uint32_t depth, VkFormat format, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties)
	: mName(name), mDevice(device), mWidth(width), mHeight(height), mDepth(depth), mArrayLayers(1), mMipLevels(mipLevels), mFormat(format), mSampleCount(numSamples), mTiling(tiling), mUsage(usage), mMemoryProperties(properties), mMemory({}) {
	
	if (mipLevels == 0) mMipLevels = (uint32_t)std::floor(std::log2(std::max(mWidth, mHeight))) + 1;
	if (mMipLevels > 1) mUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	if (pixels && imageSize) {
		mUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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

		Buffer uploadBuffer(name + " Copy", mDevice, pixels, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		auto commandBuffer = mDevice->GetCommandBuffer();
		TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandBuffer.get());
		vkCmdCopyBufferToImage(*commandBuffer, uploadBuffer, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		if (mMipLevels > 1)
			GenerateMipMaps(commandBuffer.get());
		else
			TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, ((mUsage & VK_IMAGE_USAGE_STORAGE_BIT) != 0) ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer.get());
		mDevice->Execute(commandBuffer, false)->Wait();
	} else {
		CreateImage();
		CreateImageView(VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

Texture::Texture(const string& name, Device* device, uint32_t width, uint32_t height, uint32_t depth, VkFormat format, VkSampleCountFlagBits numSamples, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties)
	: mName(name), mDevice(device), mWidth(width), mHeight(height), mDepth(depth), mArrayLayers(1), mMipLevels(1), mFormat(format), mSampleCount(numSamples), mTiling(tiling), mUsage(usage), mMemoryProperties(properties), mMemory({}) {

	CreateImage();

	VkImageAspectFlags aspect = 0;
	switch (mFormat){
	default:
		aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		break;
	case VK_FORMAT_D16_UNORM:
	case VK_FORMAT_D32_SFLOAT:
		aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		break;
	case VK_FORMAT_D16_UNORM_S8_UINT:
	case VK_FORMAT_D24_UNORM_S8_UINT:
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		break;
	}
	CreateImageView(aspect);
}

Texture::~Texture() {
	vkDestroyImage(*mDevice, mImage, nullptr);
	vkDestroyImageView(*mDevice, mView, nullptr);
	mDevice->FreeMemory(mMemory);
}

void Texture::GenerateMipMaps(CommandBuffer* commandBuffer) {
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = mImage;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = mArrayLayers;
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
		blit.srcSubresource.layerCount = mArrayLayers;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, mipDepth > 1 ? mipDepth / 2 : 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = mArrayLayers;

		vkCmdBlitImage(*commandBuffer,
			mImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = ((mUsage & VK_IMAGE_USAGE_STORAGE_BIT) != 0) ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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
	barrier.newLayout = ((mUsage & VK_IMAGE_USAGE_STORAGE_BIT) != 0) ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(*commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, (mUsage & VK_IMAGE_USAGE_STORAGE_BIT) ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
		0, nullptr,
		0, nullptr,
		1, &barrier);
}

void Texture::CreateImage() {
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = mDepth > 1 ? VK_IMAGE_TYPE_3D : (mHeight > 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_1D);
	imageInfo.extent.width = mWidth;
	imageInfo.extent.height = mHeight;
	imageInfo.extent.depth = mDepth;
	imageInfo.mipLevels = mMipLevels;
	imageInfo.arrayLayers = mArrayLayers;
	imageInfo.format = mFormat;
	imageInfo.tiling = mTiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = mUsage;
	imageInfo.samples = mSampleCount;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.flags = mArrayLayers == 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

	ThrowIfFailed(vkCreateImage(*mDevice, &imageInfo, nullptr, &mImage), "vkCreateImage failed for " + mName);
	mDevice->SetObjectName(mImage, mName, VK_OBJECT_TYPE_IMAGE);

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(*mDevice, mImage, &memRequirements);

	mMemory = mDevice->AllocateMemory(memRequirements, mMemoryProperties, mName);
	vkBindImageMemory(*mDevice, mImage, mMemory.mDeviceMemory, mMemory.mOffset);
}
void Texture::CreateImageView(VkImageAspectFlags aspectFlags) {
	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.viewType = mArrayLayers == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : (mDepth > 1 ? VK_IMAGE_VIEW_TYPE_3D : (mHeight > 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_1D));
	viewInfo.format = mFormat;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mMipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = mArrayLayers;
	viewInfo.image = mImage;

	ThrowIfFailed(vkCreateImageView(*mDevice, &viewInfo, nullptr, &mView), "vkCreateImageView failed for " + mName);
	mDevice->SetObjectName(mView, mName + " View", VK_OBJECT_TYPE_IMAGE_VIEW);
}

inline void AccessFlags(VkImageLayout layout, VkAccessFlags& access, VkPipelineStageFlags& stage) {
	switch (layout) {
	case VK_IMAGE_LAYOUT_UNDEFINED:
		access = 0;
		stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		break;
	case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
		access = 0;
		stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		access = VK_ACCESS_TRANSFER_READ_BIT;
		stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		access = VK_ACCESS_TRANSFER_WRITE_BIT;
		stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		access = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		access = VK_ACCESS_SHADER_READ_BIT;
		stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		break;
	case VK_IMAGE_LAYOUT_GENERAL:
		access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		break;
	default:
		fprintf_color(COLOR_RED, stderr, "Unsupported layout transition (add it here pls)\n");
		throw;
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

	switch (format){
	default:
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		break;
	case VK_FORMAT_D16_UNORM:
	case VK_FORMAT_D32_SFLOAT:
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		break;
	case VK_FORMAT_D16_UNORM_S8_UINT:
	case VK_FORMAT_D24_UNORM_S8_UINT:
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		break;
	}
	
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mipLevels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags dstStage, srcStage;
	AccessFlags(oldLayout, barrier.srcAccessMask, srcStage);
	AccessFlags(newLayout, barrier.dstAccessMask, dstStage);

	vkCmdPipelineBarrier(*commandBuffer,
		srcStage, dstStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);
}
void Texture::TransitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout, CommandBuffer* commandBuffer) {
	VkPipelineStageFlags dstStage, srcStage;
	VkImageMemoryBarrier barrier = TransitionImageLayout(oldLayout, newLayout, srcStage, dstStage);

	vkCmdPipelineBarrier(*commandBuffer,
		srcStage, dstStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier );
}
VkImageMemoryBarrier Texture::TransitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags& srcStage, VkPipelineStageFlags& dstStage) {
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = mImage;

	switch (mFormat) {
	default:
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		break;
	case VK_FORMAT_D16_UNORM:
	case VK_FORMAT_D32_SFLOAT:
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		break;
	case VK_FORMAT_D16_UNORM_S8_UINT:
	case VK_FORMAT_D24_UNORM_S8_UINT:
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		break;
	}

	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mMipLevels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = mArrayLayers;
	barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;

	AccessFlags(oldLayout, barrier.srcAccessMask, srcStage);
	AccessFlags(newLayout, barrier.dstAccessMask, dstStage);
	return barrier;
}