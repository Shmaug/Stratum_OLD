#include <Content/Material.hpp>
#include <Shaders/include/shadercompat.h>
#include <Scene/Camera.hpp>

using namespace std;

Material::Material(const string& name, ::Shader* shader)
	: mName(name), mShader(shader), mPassMask(Main), mCullMode(VK_CULL_MODE_FLAG_BITS_MAX_ENUM), mBlendMode(BLEND_MODE_MAX_ENUM), mRenderQueueOverride(~0) {}
Material::Material(const string& name, shared_ptr<::Shader> shader)
	: mName(name), mShader(shader), mPassMask(Main), mCullMode(VK_CULL_MODE_FLAG_BITS_MAX_ENUM), mBlendMode(BLEND_MODE_MAX_ENUM), mRenderQueueOverride(~0) {}
Material::~Material() {}

void Material::DisableKeyword(const string& kw) {
	mShaderKeywords.erase(kw);
	for (auto& d : mDeviceData)
		d.second.mShaderVariant = nullptr;
}
void Material::EnableKeyword(const string& kw) {
	mShaderKeywords.insert(kw);
	for (auto& d : mDeviceData)
		d.second.mShaderVariant = nullptr;
}

void Material::SetParameter(const string& name, const MaterialParameter& param) {
	mParameters[name] = param;
}
void Material::SetParameter(const string& name, uint32_t index, Texture* param) {
	if (!mArrayParameters.count(name))
		mArrayParameters.emplace(name, unordered_map<uint32_t, variant<shared_ptr<Texture>, Texture*>>());
	mArrayParameters.at(name)[index] = param;
}
void Material::SetParameter(const string& name, uint32_t index, shared_ptr<Texture> param) {
	if (!mArrayParameters.count(name))
		mArrayParameters.emplace(name, unordered_map<uint32_t, variant<shared_ptr<Texture>, Texture*>>());
	mArrayParameters.at(name)[index] = param;
}

GraphicsShader* Material::GetShader(Device* device) {
	if (!mDeviceData.count(device)) {
		DeviceData& d = mDeviceData[device];
		d.mShaderVariant = Shader()->GetGraphics(device, mShaderKeywords);
		return d.mShaderVariant;
	} else {
		auto& d = mDeviceData[device];
		if (!d.mShaderVariant) d.mShaderVariant = Shader()->GetGraphics(device, mShaderKeywords);
		return d.mShaderVariant;
	}
}

void Material::SetParameters(CommandBuffer* commandBuffer, Camera* camera, GraphicsShader* variant) {
	if (variant->mDescriptorSetLayouts.size() > PER_MATERIAL && variant->mDescriptorBindings.size()) {
		uint32_t frameContextIndex = commandBuffer->Device()->FrameContextIndex();
		auto& data = mDeviceData[commandBuffer->Device()];

		DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet(mName + " PerMaterial DescriptorSet", variant->mDescriptorSetLayouts[PER_MATERIAL]);

		// set descriptor parameters

		for (auto& m : mParameters) {
			if (m.second.index() > 4) continue;
			if (variant->mDescriptorBindings.count(m.first) == 0) continue;
			auto& bindings = variant->mDescriptorBindings.at(m.first);
			if (bindings.first != PER_MATERIAL) continue;

			auto binding = bindings.second;

			switch (m.second.index()) {
			case 0:
				ds->CreateSampledTextureDescriptor(get<shared_ptr<Texture>>(m.second).get(), binding.binding);
				break;
			case 1:
				ds->CreateSamplerDescriptor(get<shared_ptr<Sampler>>(m.second).get(), binding.binding);
				break;
			case 2:
				ds->CreateSampledTextureDescriptor(get<Texture*>(m.second), binding.binding);
				break;
			case 3:
				ds->CreateSamplerDescriptor(get<Sampler*>(m.second), binding.binding);
				break;
			}
		}

		vector<Texture*> tmpArray;
		for (auto& m : mArrayParameters) {
			if (variant->mDescriptorBindings.count(m.first) == 0) continue;
			auto& bindings = variant->mDescriptorBindings.at(m.first);
			if (bindings.first != PER_MATERIAL) continue;
			auto binding = bindings.second;

			tmpArray.assign(binding.descriptorCount, nullptr);
			Texture* t = nullptr;
			for (auto& p : m.second) {
				if (p.first >= binding.descriptorCount) continue;
				tmpArray[p.first] = p.second.index() == 0 ? get<shared_ptr<Texture>>(p.second).get() : get<Texture*>(p.second);
				if (!t) t = tmpArray[p.first];
			}
			if (t) {
				for (uint32_t i = 0; i < tmpArray.size(); i++)
					if (!tmpArray[i]) tmpArray[i] = t;
				ds->CreateSampledTextureDescriptor(tmpArray.data(), binding.descriptorCount, binding.descriptorCount, binding.binding);
			}
		}

		VkDescriptorSet matds = *ds;
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, variant->mPipelineLayout, PER_MATERIAL, 1, &matds, 0, nullptr);
	}

	if (camera && variant->mDescriptorBindings.count("Camera")) {
		auto binding = variant->mDescriptorBindings.at("Camera");
		VkDescriptorSet camds = *camera->DescriptorSet(binding.second.stageFlags);
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, variant->mPipelineLayout, PER_CAMERA, 1, &camds, 0, nullptr);
	}

	// set push constant parameters
	for (auto& m : mParameters) {
		if (m.second.index() < 4) continue;
		if (variant->mPushConstants.count(m.first) == 0) continue;
		auto& range = variant->mPushConstants.at(m.first);

		float4 value(0);

		switch (m.second.index()) {
		case 4:
			if (range.size != sizeof(float)) continue;
			value = float4(get<float>(m.second), 0, 0, 0);
			break;
		case 5:
			if (range.size != sizeof(float2)) continue;
			value = float4(get<float2>(m.second), 0, 0);
			break;
		case 6:
			if (range.size != sizeof(float3)) continue;
			value = float4(get<float3>(m.second), 0);
			break;
		case 7:
			if (range.size != sizeof(float4)) continue;
			value = get<float4>(m.second);
			break;
		}

		vkCmdPushConstants(*commandBuffer, variant->mPipelineLayout, range.stageFlags, range.offset, range.size, &value);
	}
}
VkPipelineLayout Material::Bind(CommandBuffer* commandBuffer, const VertexInput* input, Camera* camera, VkPrimitiveTopology topology) {
	if (!commandBuffer->CurrentRenderPass()) {
		fprintf_color(BoldRed, stderr, "Attempting to bind material outside of a RenderPass!");
		throw;
	}

	GraphicsShader* variant = GetShader(commandBuffer->Device());
	if (!variant) return VK_NULL_HANDLE;

	VkPipeline pipeline = variant->GetPipeline(commandBuffer->CurrentRenderPass(), input, topology, mCullMode, mBlendMode);
	vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	SetParameters(commandBuffer, camera, variant);
	
	return variant->mPipelineLayout;
}