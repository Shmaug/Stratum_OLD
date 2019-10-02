#pragma once

#include <unordered_map>

#include <Content/Asset.hpp>
#include <Core/DeviceManager.hpp>
#include <Util/Util.hpp>

class AssetDatabase;

class Texture : public Asset {
public:
	const std::string mName;

	ENGINE_EXPORT Texture(const std::string& name, DeviceManager* devices,
		void* pixels, VkDeviceSize imageSize, uint32_t width, uint32_t height, uint32_t depth, VkFormat format, uint32_t mipLevels,
		VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
		VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT, VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	ENGINE_EXPORT Texture(const std::string& name, DeviceManager* devices,
		uint32_t width, uint32_t height, VkFormat format,
		VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
		VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT, VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	ENGINE_EXPORT Texture(const std::string& name, Device* device,
		uint32_t width, uint32_t height, VkFormat format,
		VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
		VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT, VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	ENGINE_EXPORT ~Texture() override;

	inline uint32_t Width() const { return mWidth; }
	inline uint32_t Height() const { return mHeight; }
	inline uint32_t Depth() const { return mDepth; }
	inline uint32_t MipLevels() const { return mMipLevels; }
	inline VkFormat Format() const { return mFormat; }

	inline VkImage Image(Device* device) const { return mDeviceData.at(device).mImage; }
	inline VkImageView View(Device* device) const { return mDeviceData.at(device).mView; }
	inline VkImageLayout Layout(Device* device) const { return mDeviceData.at(device).mLayout; }

	ENGINE_EXPORT void TransitionImageLayout(VkImageLayout newLayout, CommandBuffer* commandBuffer);

private:
	friend class AssetDatabase;
	ENGINE_EXPORT Texture(const std::string& name, DeviceManager* devices, const std::string& filename, bool srgb = true);

	uint32_t mWidth;
	uint32_t mHeight;
	uint32_t mDepth;
	uint32_t mMipLevels;

	VkFormat mFormat;
	VkSampleCountFlagBits mSampleCount;
	VkImageTiling mTiling;
	VkImageUsageFlags mUsage;
	VkMemoryPropertyFlags mMemoryProperties;

	struct DeviceData {
		VkImage mImage;
		VkImageView mView;
		VkDeviceMemory mImageMemory;
		VkImageLayout mLayout;
		VkAccessFlags mAcessFlags;
		VkPipelineStageFlags mStageFlags;
		inline DeviceData() : mImage(VK_NULL_HANDLE), mView(VK_NULL_HANDLE), mImageMemory(VK_NULL_HANDLE), mLayout(VK_IMAGE_LAYOUT_UNDEFINED), mAcessFlags(0), mStageFlags(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT) {}
	};
	std::unordered_map<Device*, DeviceData> mDeviceData;

	ENGINE_EXPORT void GenerateMipMaps(CommandBuffer* commandBuffer);

	ENGINE_EXPORT void CreateImage();
	ENGINE_EXPORT void CreateImageView(VkImageAspectFlags flags);
};