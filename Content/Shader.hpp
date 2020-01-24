#pragma once

#include <Content/Asset.hpp>
#include <Core/Instance.hpp>
#include <Core/Sampler.hpp>
#include <Core/RenderPass.hpp>

class Shader;

struct PipelineInstance {
	VkRenderPass mRenderPass;
	const VertexInput* mVertexInput;
	VkPrimitiveTopology mTopology;
	VkCullModeFlags mCullMode;
	BlendMode mBlendMode;
	VkPolygonMode mPolygonMode;

	inline PipelineInstance(VkRenderPass renderPass, const VertexInput* vertexInput, VkPrimitiveTopology topology, VkCullModeFlags cullMode, BlendMode blendMode, VkPolygonMode polyMode)
		: mRenderPass(renderPass), mVertexInput(vertexInput), mTopology(topology), mCullMode(cullMode), mBlendMode(blendMode), mPolygonMode(polyMode) {};

	ENGINE_EXPORT bool operator==(const PipelineInstance& rhs) const;
};

namespace std {
	template<>
	struct hash<PipelineInstance> {
		inline std::size_t operator()(const  PipelineInstance& p) const {
			std::size_t h = 0;
			hash_combine(h, p.mRenderPass);
			if (p.mVertexInput) hash_combine(h, *p.mVertexInput);
			hash_combine(h, p.mTopology);
			hash_combine(h, p.mCullMode);
			hash_combine(h, p.mBlendMode);
			hash_combine(h, p.mPolygonMode);
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
	std::string mEntryPoints[2];
	VkPipelineShaderStageCreateInfo mStages[2];
	std::unordered_map<PipelineInstance, VkPipeline> mPipelines;
	Shader* mShader;

	inline GraphicsShader() : ShaderVariant() { mShader = nullptr; mStages[0] = {}; mStages[1] = {}; }
	ENGINE_EXPORT VkPipeline GetPipeline(RenderPass* renderPass, const VertexInput* vertexInput,
		VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VkCullModeFlags cullMode = VK_CULL_MODE_FLAG_BITS_MAX_ENUM,
		BlendMode blendMode = BLEND_MODE_MAX_ENUM,
		VkPolygonMode polyMode = VK_POLYGON_MODE_MAX_ENUM);
};

class Shader : public Asset {
public:
	const std::string mName;

	ENGINE_EXPORT ~Shader() override;

	/// Returns a shader variant for a specific pass and set of keywords, or nullptr if none exists
	ENGINE_EXPORT GraphicsShader* GetGraphics(PassType pass, const std::set<std::string>& keywords) const;
	/// Returns a shader variant for a specific kernel and set of keywords, or nullptr if none exists
	ENGINE_EXPORT ComputeShader* GetCompute(const std::string& kernel, const std::set<std::string>& keywords) const;

	inline ::Device* Device() const { return mDevice; }
	inline PassType PassMask() const { return mPassMask; }
	inline uint32_t RenderQueue() const { return mRenderQueue; }

private:
	friend class GraphicsShader;
	friend class AssetManager;
	ENGINE_EXPORT Shader(const std::string& name, ::Device* device, const std::string& filename);

	::Device* mDevice;

	friend class GraphicsShader;
	std::set<std::string> mKeywords;

	PassType mPassMask;
	VkColorComponentFlags mColorMask;
	uint32_t mRenderQueue;
	BlendMode mBlendMode;
	VkPipelineViewportStateCreateInfo mViewportState;
	VkPipelineRasterizationStateCreateInfo mRasterizationState;
	VkPipelineDepthStencilStateCreateInfo mDepthStencilState;
	VkPipelineDynamicStateCreateInfo mDynamicState;
	std::vector<VkDynamicState> mDynamicStates;

	std::unordered_map<std::string, std::unordered_map<std::string, ComputeShader*>> mComputeVariants;
	std::unordered_map<PassType, std::unordered_map<std::string, GraphicsShader*>> mGraphicsVariants;
	std::vector<Sampler*> mStaticSamplers;
};