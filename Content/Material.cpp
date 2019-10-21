#include <Content/Material.hpp>
#include <Shaders/shadercompat.h>
#include <Scene/Camera.hpp>

using namespace std;

Material::Material(const string& name, ::Shader* shader)
	: mName(name), mShader(shader), mCullMode(VK_CULL_MODE_FLAG_BITS_MAX_ENUM), mBlendMode(BLEND_MODE_MAX_ENUM), mRenderQueueOverride(~0) {}
Material::Material(const string& name, shared_ptr<::Shader> shader)
	: mName(name), mShader(shader), mCullMode(VK_CULL_MODE_FLAG_BITS_MAX_ENUM), mBlendMode(BLEND_MODE_MAX_ENUM), mRenderQueueOverride(~0) {}
Material::~Material(){
	for (auto& d : mDeviceData) {
		for (uint32_t i = 0; i < d.first->MaxFramesInFlight(); i++)
			safe_delete(d.second.mDescriptorSets[i]);
		safe_delete(d.second.mDescriptorSets);
		safe_delete(d.second.mDirty);
	}
}

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
	for (auto& d : mDeviceData)
		memset(d.second.mDirty, true, sizeof(bool) * d.first->MaxFramesInFlight());
}

GraphicsShader* Material::GetShader(Device* device) {
	if (!mDeviceData.count(device)) {
		DeviceData& d = mDeviceData[device];
		d.mDescriptorSets = new DescriptorSet*[device->MaxFramesInFlight()];
		d.mDirty = new bool[device->MaxFramesInFlight()];
		memset(d.mDescriptorSets, 0, sizeof(DescriptorSet*) * device->MaxFramesInFlight());
		memset(d.mDirty, true, sizeof(bool) * device->MaxFramesInFlight());
		d.mShaderVariant = Shader()->GetGraphics(device, mShaderKeywords);
		return d.mShaderVariant;
	} else {
		auto& d = mDeviceData[device];
		if (!d.mShaderVariant)
			d.mShaderVariant = Shader()->GetGraphics(device, mShaderKeywords);
		return d.mShaderVariant;
	}
}
VkPipelineLayout Material::Bind(CommandBuffer* commandBuffer, uint32_t backBufferIndex, RenderPass* renderPass, const VertexInput* input, VkPrimitiveTopology topology) {
	if (renderPass == VK_NULL_HANDLE) return VK_NULL_HANDLE;

	auto variant = GetShader(renderPass->Device());
	if (!variant) return VK_NULL_HANDLE;

	VkPipeline pipeline = variant->GetPipeline(renderPass, input, topology, mCullMode, mBlendMode);
	vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	if (variant->mDescriptorSetLayouts.size() > PER_MATERIAL &&
		variant->mDescriptorBindings.size()) {
		auto& data = mDeviceData[commandBuffer->Device()];
		if (!data.mDescriptorSets[backBufferIndex])
			data.mDescriptorSets[backBufferIndex] = new DescriptorSet(mName + " PerMaterial DescriptorSet", commandBuffer->Device()->DescriptorPool(), variant->mDescriptorSetLayouts[PER_MATERIAL]);

		// set descriptor parameters
		if (data.mDirty[backBufferIndex]) {
			for (auto& m : mParameters) {
				if (m.second.index() > 3) continue;
				if (variant->mDescriptorBindings.count(m.first) == 0) continue;
				auto& bindings = variant->mDescriptorBindings.at(m.first);
				if (bindings.first != PER_MATERIAL) continue;

				switch (m.second.index()) {
				case 0:
					data.mDescriptorSets[backBufferIndex]->CreateSampledTextureDescriptor(get<shared_ptr<Texture>>(m.second).get(), bindings.second.binding);
					break;
				case 1:
					data.mDescriptorSets[backBufferIndex]->CreateSamplerDescriptor(get<shared_ptr<Sampler>>(m.second).get(), bindings.second.binding);
					break;
				case 2:
					data.mDescriptorSets[backBufferIndex]->CreateSampledTextureDescriptor(get<Texture*>(m.second), bindings.second.binding);
					break;
				case 3:
					data.mDescriptorSets[backBufferIndex]->CreateSamplerDescriptor(get<Sampler*>(m.second), bindings.second.binding);
					break;
				}
			}
			data.mDirty[backBufferIndex] = false;
		}

		VkDescriptorSet matds = *data.mDescriptorSets[backBufferIndex];
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, variant->mPipelineLayout, PER_MATERIAL, 1, &matds, 0, nullptr);
	}
	
	if (renderPass->Camera() && variant->mDescriptorBindings.count("Camera")) {
		auto binding = variant->mDescriptorBindings.at("Camera");
		VkDescriptorSet camds = *renderPass->Camera()->DescriptorSet(backBufferIndex, binding.second.stageFlags);
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, variant->mPipelineLayout, PER_CAMERA, 1, &camds, 0, nullptr);
	}

	// set push constant parameters
	for (auto& m : mParameters) {
		if (m.second.index() <= 3) continue;
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
	
	return variant->mPipelineLayout;
}