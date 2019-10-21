#pragma once

#include <variant>
#include <unordered_map>

#include <Content/Shader.hpp>
#include <Content/Texture.hpp>
#include <Core/CommandBuffer.hpp>
#include <Core/Sampler.hpp>
#include <Core/RenderPass.hpp>
#include <Core/DescriptorSet.hpp>

typedef std::variant<
	std::shared_ptr<Texture>,
	std::shared_ptr<Sampler>,
	Texture*,
	Sampler*,
	float,
	float2,
	float3,
	float4
> MaterialParameter;

class Material {
public:
	const std::string mName;

	ENGINE_EXPORT Material(const std::string& name, ::Shader* shader);
	ENGINE_EXPORT Material(const std::string& name, std::shared_ptr<::Shader> shader);
	ENGINE_EXPORT ~Material();

	ENGINE_EXPORT GraphicsShader* GetShader(Device* device);

	inline void RenderQueue(uint32_t q) { mRenderQueueOverride = q; }
	inline uint32_t RenderQueue() const { return (mRenderQueueOverride == ~0) ? Shader()->RenderQueue() : mRenderQueueOverride; }

	inline void CullMode(VkCullModeFlags c) { mCullMode = c; }
	inline VkCullModeFlags CullMode() const { return mCullMode; }

	inline void BlendMode(::BlendMode c) { mBlendMode = c; }
	inline ::BlendMode BlendMode() const { return mBlendMode; }

	ENGINE_EXPORT void SetParameter(const std::string& name, const MaterialParameter& param);
	ENGINE_EXPORT void DisableKeyword(const std::string& kw);
	ENGINE_EXPORT void EnableKeyword(const std::string& kw);

private:
	friend class CommandBuffer;
	ENGINE_EXPORT VkPipelineLayout Bind(CommandBuffer* commandBuffer, uint32_t backBufferIndex, RenderPass* renderPass, const VertexInput* input, VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

	inline ::Shader* Shader() const { return mShader.index() == 0 ? std::get<::Shader*>(mShader) : std::get<std::shared_ptr<::Shader>>(mShader).get(); };

	std::variant<::Shader*, std::shared_ptr<::Shader>> mShader;
	std::set<std::string> mShaderKeywords;
	VkCullModeFlags mCullMode;
	::BlendMode mBlendMode;

	uint32_t mRenderQueueOverride;

	std::unordered_map<std::string, MaterialParameter> mParameters;

	bool mIsBound;

	struct DeviceData {
		::DescriptorSet** mDescriptorSets;
		bool* mDirty;
		GraphicsShader* mShaderVariant;
		inline DeviceData() : mDescriptorSets(nullptr), mDirty(nullptr), mShaderVariant(nullptr) {};
	};
	std::unordered_map<Device*, DeviceData> mDeviceData;
};