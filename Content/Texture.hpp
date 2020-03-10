#pragma once

#include <Content/Asset.hpp>
#include <Core/Device.hpp>
#include <Core/Sampler.hpp>
#include <Util/Util.hpp>

class Texture : public Asset {
public:
	const std::string mName;

	ENGINE_EXPORT Texture(const std::string& name, Device* device,
		uint32_t width, uint32_t height, uint32_t depth, VkFormat format,
		VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
		VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT, VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		
	ENGINE_EXPORT Texture(const std::string& name, Device* device,
		void* pixels, VkDeviceSize imageSize, uint32_t width, uint32_t height, uint32_t depth, VkFormat format, uint32_t mipLevels,
		VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
		VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT, VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	ENGINE_EXPORT ~Texture() override;

	inline uint32_t Width() const { return mWidth; }
	inline uint32_t Height() const { return mHeight; }
	inline uint32_t Depth() const { return mDepth; }
	inline uint32_t MipLevels() const { return mMipLevels; }
	inline VkFormat Format() const { return mFormat; }
	inline VkSampleCountFlagBits SampleCount() const { return mSampleCount; }
	inline VkImageUsageFlags Usage() const { return mUsage; }

	inline VkImage Image() const { return mImage; }
	inline VkImageView View() const { return mView; }

	ENGINE_EXPORT static void TransitionImageLayout(VkImage image, VkFormat format, uint32_t mipLevels, VkImageLayout oldLayout, VkImageLayout newLayout, CommandBuffer* commandBuffer);
	ENGINE_EXPORT void TransitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout, CommandBuffer* commandBuffer);
	ENGINE_EXPORT VkImageMemoryBarrier TransitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags& srcStage, VkPipelineStageFlags& dstStage);

	// Texture must have been created with the appropriate mipmap levels defined
	// Texture must be in VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	ENGINE_EXPORT void GenerateMipMaps(CommandBuffer* commandBuffer);

private:
	friend class AssetManager;
	ENGINE_EXPORT Texture(const std::string& name, Device* device, const std::string& filename, bool srgb = true);
	ENGINE_EXPORT Texture(const std::string& name, Device* device, const std::string& px, const std::string& nx, const std::string& py, const std::string& ny, const std::string& pz, const std::string& nz, bool srgb = true);

	Device* mDevice;
	DeviceMemoryAllocation mMemory;
	
	uint32_t mWidth;
	uint32_t mHeight;
	uint32_t mDepth;
	uint32_t mMipLevels;
	uint32_t mArrayLayers;

	VkFormat mFormat;
	VkSampleCountFlagBits mSampleCount;
	VkImageTiling mTiling;
	VkImageUsageFlags mUsage;
	VkMemoryPropertyFlags mMemoryProperties;

	VkMemoryAllocateInfo mAllocationInfo;

	VkImage mImage;
	VkImageView mView;

	ENGINE_EXPORT void CreateImage();
	ENGINE_EXPORT void CreateImageView(VkImageAspectFlags flags);
};