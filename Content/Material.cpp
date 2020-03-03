#include <Content/Material.hpp>
#include <Shaders/include/shadercompat.h>
#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Content/AssetManager.hpp>
#include <Util/Profiler.hpp>

using namespace std;

Material::Material(const string& name, ::Shader* shader)
	: mName(name), mShader(shader), mDevice(shader->Device()), mCullMode(VK_CULL_MODE_FLAG_BITS_MAX_ENUM), mBlendMode(BLEND_MODE_MAX_ENUM), mRenderQueue(~0), mPassMask(PASS_MASK_MAX_ENUM) {}
Material::Material(const string& name, shared_ptr<::Shader> shader)
	: mName(name), mShader(shader), mDevice(shader->Device()), mCullMode(VK_CULL_MODE_FLAG_BITS_MAX_ENUM), mBlendMode(BLEND_MODE_MAX_ENUM), mRenderQueue(~0), mPassMask(PASS_MASK_MAX_ENUM) {}
Material::~Material() {
	for (auto& kp : mVariantData) {
		for (uint32_t i = 0; i < mDevice->MaxFramesInFlight(); i++)
			safe_delete(kp.second->mDescriptorSets[i]);
		safe_delete_array(kp.second->mDescriptorSets);
		safe_delete_array(kp.second->mDirty);
		safe_delete(kp.second);
	}
}

void Material::EnableKeyword(const string& kw) {
	if (mShaderKeywords.count(kw)) return;
	mShaderKeywords.insert(kw);
	for (auto& d : mVariantData) {
		memset(d.second->mDirty, true, sizeof(bool) * mDevice->MaxFramesInFlight());
		d.second->mShaderVariant = nullptr;
	}
}
void Material::DisableKeyword(const string& kw) {
	if (!mShaderKeywords.count(kw)) return;
	mShaderKeywords.erase(kw);
	for (auto& d : mVariantData) {
		memset(d.second->mDirty, true, sizeof(bool) * mDevice->MaxFramesInFlight());
		d.second->mShaderVariant = nullptr;
	}
}

void Material::SetUniformBuffer(const string& name, VkDeviceSize offset, VkDeviceSize range, std::shared_ptr<Buffer> param) {
	if (!mUniformBuffers.count(name)) {
		auto& p = mUniformBuffers[name];
		p.mBuffer = param;
		p.mOffset = offset;
		p.mRange = range;
		for (auto& d : mVariantData)
			memset(d.second->mDirty, true, sizeof(bool) * mDevice->MaxFramesInFlight());
	} else {
		auto& p = mUniformBuffers[name];
		if (p.mBuffer.index() != 0 || get<shared_ptr<Buffer>>(p.mBuffer) != param || p.mOffset != offset || p.mRange != range) {
			p.mBuffer = param;
			p.mOffset = offset;
			p.mRange = range;
			for (auto& d : mVariantData)
				memset(d.second->mDirty, true, sizeof(bool) * mDevice->MaxFramesInFlight());
		}
	}
}
void Material::SetUniformBuffer(const string& name, VkDeviceSize offset, VkDeviceSize range, Buffer* param) {
	if (!mUniformBuffers.count(name)) {
		auto& p = mUniformBuffers[name];
		p.mBuffer = param;
		p.mOffset = offset;
		p.mRange = range;
		for (auto& d : mVariantData)
			memset(d.second->mDirty, true, sizeof(bool) * mDevice->MaxFramesInFlight());
	} else {
		auto& p = mUniformBuffers[name];
		if (p.mBuffer.index() != 1 || get<Buffer*>(p.mBuffer) != param || p.mOffset != offset || p.mRange != range) {
			p.mBuffer = param;
			p.mOffset = offset;
			p.mRange = range;
			for (auto& d : mVariantData)
				memset(d.second->mDirty, true, sizeof(bool) * mDevice->MaxFramesInFlight());
		}
	}
}

void Material::SetParameter(const string& name, const MaterialParameter& param) {
	MaterialParameter& p = mParameters[name];
	if (p != param) {
		p = param;
		if (param.index() < 4) // push constants dont make descriptors dirty
			for (auto& d : mVariantData)
				memset(d.second->mDirty, true, sizeof(bool) * mDevice->MaxFramesInFlight());
	}
}
void Material::SetParameter(const string& name, uint32_t index, shared_ptr<Texture> param) {
	auto& p = mArrayParameters[name][index];
	if (p.index() != 0 || get<shared_ptr<Texture>>(p) != param) {
		p = param;
		for (auto& d : mVariantData)
			memset(d.second->mDirty, true, sizeof(bool) * mDevice->MaxFramesInFlight());
	}
}
void Material::SetParameter(const string& name, uint32_t index, Texture* param) {
	auto& p = mArrayParameters[name][index];
	if (p.index() != 1 || get<Texture*>(p) != param) {
		p = param;
		for (auto& d : mVariantData)
			memset(d.second->mDirty, true, sizeof(bool) * mDevice->MaxFramesInFlight());
	}
}

Material::VariantData* Material::GetData(PassType pass) {
	if (mVariantData.count(pass) == 0) {
		GraphicsShader* shader = Shader()->GetGraphics(pass, mShaderKeywords);
		if (!shader) return nullptr;

		VariantData* data = new VariantData();
		data->mDescriptorSets = new DescriptorSet*[mDevice->MaxFramesInFlight()];
		data->mDirty = new bool[mDevice->MaxFramesInFlight()];
		memset(data->mDescriptorSets, 0, sizeof(DescriptorSet*) * mDevice->MaxFramesInFlight());
		memset(data->mDirty, true, sizeof(bool) * mDevice->MaxFramesInFlight());
		data->mShaderVariant  = shader;
		mVariantData.emplace(pass, data);
		return data;
	}

	auto& data = mVariantData.at(pass);
	if (!data->mShaderVariant) data->mShaderVariant = Shader()->GetGraphics(pass, mShaderKeywords);
	return data;
}
GraphicsShader* Material::GetShader(PassType pass) {
	return GetData(pass)->mShaderVariant;
}

void Material::SetDescriptorParameters(CommandBuffer* commandBuffer, Camera* camera, VariantData* data) {
	GraphicsShader* shader = data->mShaderVariant;
	if (shader->mDescriptorSetLayouts.size() > PER_MATERIAL && shader->mDescriptorBindings.size()) {
		uint32_t frameContextIndex = commandBuffer->Device()->FrameContextIndex();
		DescriptorSet*& ds = data->mDescriptorSets[frameContextIndex];
		if (!ds || (ds->Layout() != shader->mDescriptorSetLayouts[PER_MATERIAL])) {
			safe_delete(ds);
			ds = new DescriptorSet(mName + " DescriptorSet", commandBuffer->Device(), shader->mDescriptorSetLayouts[PER_MATERIAL]);
			data->mDirty[frameContextIndex] = true;
		}

		// set descriptor parameters
		if (data->mDirty[frameContextIndex]) {
			PROFILER_BEGIN("Write Descriptor Sets");
			for (auto& m : mParameters) {
				if (m.second.index() > 4) continue;
				if (shader->mDescriptorBindings.count(m.first) == 0) continue;
				auto& bindings = shader->mDescriptorBindings.at(m.first);
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

			for (auto& m : mArrayParameters) {
				if (shader->mDescriptorBindings.count(m.first) == 0) continue;
				auto& bindings = shader->mDescriptorBindings.at(m.first);
				if (bindings.first != PER_MATERIAL) continue;

				for (auto& p : m.second) {
					if (p.first >= bindings.second.descriptorCount) continue;
					Texture* t = p.second.index() == 0 ? get<shared_ptr<Texture>>(p.second).get() : get<Texture*>(p.second);
					ds->CreateSampledTextureDescriptor(t, p.first, bindings.second.binding);
				}

			}

			for (auto& m : mUniformBuffers) {
				if (shader->mDescriptorBindings.count(m.first) == 0) continue;
				auto& bindings = shader->mDescriptorBindings.at(m.first);
				if (bindings.first != PER_MATERIAL) continue;

				auto binding = bindings.second;

				switch (m.second.mBuffer.index()) {
				case 0:
					ds->CreateUniformBufferDescriptor(get<Buffer*>(m.second.mBuffer), m.second.mOffset, m.second.mRange, binding.binding);
					break;
				case 1:
					ds->CreateUniformBufferDescriptor(get<shared_ptr<Buffer>>(m.second.mBuffer).get(), m.second.mOffset, m.second.mRange, binding.binding);
					break;
				}
			}

			ds->FlushWrites();
			data->mDirty[frameContextIndex] = false;
			PROFILER_END;
		}

		PROFILER_BEGIN("Bind Descriptor Sets");
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->mPipelineLayout, PER_MATERIAL, 1, *ds, 0, nullptr);
		PROFILER_END;
	}

	if (camera && shader->mDescriptorSetLayouts.size() > PER_CAMERA && shader->mDescriptorBindings.count("Camera")) {
		auto binding = shader->mDescriptorBindings.at("Camera");
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->mPipelineLayout, PER_CAMERA, 1, *camera->DescriptorSet(binding.second.stageFlags), 0, nullptr);
	}
}
void Material::SetPushConstantParameters(CommandBuffer* commandBuffer, Camera* camera, VariantData* data) {
	PROFILER_BEGIN("Push Constants");
	// set push constant parameters
	GraphicsShader* shader = data->mShaderVariant;
	for (auto& m : mParameters) {
		if (m.second.index() < 4) continue;
		if (shader->mPushConstants.count(m.first) == 0) continue;
		auto& range = shader->mPushConstants.at(m.first);

		union pvalue {
			float4 fvalue;
			uint4 uvalue;
			int4 ivalue;
			float4x4 f4x4value;
		};
		pvalue value = { uint4(0) };

		switch (m.second.index()) {
		case 4:
			if (range.size != sizeof(float)) continue;
			value.fvalue = float4(get<float>(m.second), 0, 0, 0);
			break;
		case 5:
			if (range.size != sizeof(float2)) continue;
			value.fvalue = float4(get<float2>(m.second), 0, 0);
			break;
		case 6:
			if (range.size != sizeof(float3)) continue;
			value.fvalue = float4(get<float3>(m.second), 0);
			break;
		case 7:
			if (range.size != sizeof(float4)) continue;
			value.fvalue = get<float4>(m.second);
			break;

		case 8:
			if (range.size != sizeof(uint32_t)) continue;
			value.uvalue = uint4(get<uint32_t>(m.second), 0, 0, 0);
			break;
		case 9:
			if (range.size != sizeof(uint2)) continue;
			value.uvalue = uint4(get<uint2>(m.second), 0, 0);
			break;
		case 10:
			if (range.size != sizeof(uint3)) continue;
			value.uvalue = uint4(get<uint3>(m.second), 0);
			break;
		case 11:
			if (range.size != sizeof(uint4)) continue;
			value.uvalue = get<uint4>(m.second);
			break;

		case 12:
			if (range.size != sizeof(int32_t)) continue;
			value.ivalue = int4(get<int32_t>(m.second), 0, 0, 0);
			break;
		case 13:
			if (range.size != sizeof(int2)) continue;
			value.ivalue = int4(get<int2>(m.second), 0, 0);
			break;
		case 14:
			if (range.size != sizeof(int3)) continue;
			value.ivalue = int4(get<int3>(m.second), 0);
			break;
		case 15:
			if (range.size != sizeof(int4)) continue;
			value.ivalue = get<int4>(m.second);
			break;

		case 16:
			if (range.size != sizeof(float4x4)) continue;
			value.f4x4value = get<float4x4>(m.second);
			break;
		}

		vkCmdPushConstants(*commandBuffer, shader->mPipelineLayout, range.stageFlags, range.offset, range.size, &value);
	}
	PROFILER_END;
}