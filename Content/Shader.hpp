#pragma once

#include <set>
#include <unordered_map>

#include <Content/Asset.hpp>
#include <Core/DeviceManager.hpp>
#include <Core/Sampler.hpp>
#include <Core/RenderPass.hpp>

class Shader;

struct PipelineInstance {
	VkRenderPass mRenderPass;
	const VertexInput* mVertexInput;
	VkPrimitiveTopology mTopology;
	VkCullModeFlags mCullMode;

	inline PipelineInstance(VkRenderPass renderPass, const VertexInput* vertexInput, VkPrimitiveTopology topology, VkCullModeFlags cullMode)
		: mRenderPass(renderPass), mVertexInput(vertexInput), mTopology(topology), mCullMode(cullMode) {};

	ENGINE_EXPORT bool operator==(const PipelineInstance& rhs) const;
};

namespace std {
	template<>
	struct hash<PipelineInstance> {
		inline std::size_t operator()(const  PipelineInstance& p) const {
			std::size_t h;
			hash_combine(h, p.mRenderPass);
			if (p.mVertexInput) hash_combine(h, *p.mVertexInput);
			hash_combine(h, p.mTopology);
			hash_combine(h, p.mCullMode);
			return h;
		}
	};
}

class ShaderVariant {
public:
	VkPipelineLayout mPipelineLayout;
	std::vector<VkDescriptorSetLayout> mDescriptorSetLayouts;
	std::unordered_map<std::string, std::pair<uint32_t, VkDescriptorSetLayoutBinding>> mDescriptorBindings; // descriptorset, binding
	std::unordered_map<std::string, VkPushConstantRange> mPushConstants;

	inline ShaderVariant() : mPipelineLayout(VK_NULL_HANDLE) {}
	inline virtual ~ShaderVariant() {}
};
class ComputeShader : public ShaderVariant {
public:
	std::string mEntryPoint;
	VkPipelineShaderStageCreateInfo mStage;
	uint3 mWorkgroupSize;
	VkPipeline mPipeline;

	inline ComputeShader() : ShaderVariant() { mPipeline = VK_NULL_HANDLE;  mStage = {}; mWorkgroupSize = {}; }
};
class GraphicsShader : public ShaderVariant {
public:
	std::vector<std::string> mEntryPoints;
	std::vector<VkPipelineShaderStageCreateInfo> mStages;
	std::unordered_map<PipelineInstance, VkPipeline> mPipelines;
	Shader* mShader;

	inline GraphicsShader() : ShaderVariant() { mShader = nullptr; }
	ENGINE_EXPORT VkPipeline GetPipeline(RenderPass* renderPass, const VertexInput* vertexInput, VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VkCullModeFlags cullMode = VK_CULL_MODE_FLAG_BITS_MAX_ENUM);
};

class Shader : public Asset {
public:
	const std::string mName;

	ENGINE_EXPORT ~Shader() override;

	ENGINE_EXPORT GraphicsShader* GetGraphics(Device* device, const std::set<std::string>& keywords) const;
	ENGINE_EXPORT ComputeShader* GetCompute(Device* device, const std::string& kernel, const std::set<std::string>& keywords) const;

	inline uint32_t RenderQueue() const { return mRenderQueue; }

private:
	friend class AssetManager;
	ENGINE_EXPORT Shader(const std::string& name, DeviceManager* devices, const std::string& filename);

	friend class GraphicsShader;
	std::set<std::string> mKeywords;

	uint32_t mRenderQueue;
	VkPipelineColorBlendAttachmentState mBlendState;
	VkPipelineViewportStateCreateInfo mViewportState;
	VkPipelineRasterizationStateCreateInfo mRasterizationState;
	VkPipelineDepthStencilStateCreateInfo mDepthStencilState;
	VkPipelineDynamicStateCreateInfo mDynamicState;
	std::vector<VkDynamicState> mDynamicStates;

	struct DeviceData {
		std::unordered_map<std::string, std::unordered_map<std::string, ComputeShader*>> mComputeVariants;
		std::unordered_map<std::string, GraphicsShader*> mGraphicsVariants;
		std::vector<Sampler*> mStaticSamplers;
	};

	std::unordered_map<Device*, DeviceData> mDeviceData;
};