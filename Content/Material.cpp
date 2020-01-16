#include <Content/Material.hpp>
#include <Shaders/include/shadercompat.h>
#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Content/AssetManager.hpp>
#include <Util/Profiler.hpp>

using namespace std;

Material::Material(const string& name, ::Shader* shader)
	: mName(name), mShader(shader), mCullMode(VK_CULL_MODE_FLAG_BITS_MAX_ENUM), mBlendMode(BLEND_MODE_MAX_ENUM), mRenderQueueOverride(~0) {}
Material::Material(const string& name, shared_ptr<::Shader> shader)
	: mName(name), mShader(shader), mCullMode(VK_CULL_MODE_FLAG_BITS_MAX_ENUM), mBlendMode(BLEND_MODE_MAX_ENUM), mRenderQueueOverride(~0) {}
Material::~Material() {
	for (auto& dkp : mVariantData) {
		for (auto& kp : dkp.second) {
			for (uint32_t i = 0; i < dkp.first->MaxFramesInFlight(); i++)
				safe_delete(kp.second->mDescriptorSets[i]);
			safe_delete_array(kp.second->mDescriptorSets);
			safe_delete_array(kp.second->mDirty);
			safe_delete(kp.second);
		}
	}
}

void Material::EnableKeyword(const string& kw) {
	if (mShaderKeywords.count(kw)) return;
	mShaderKeywords.insert(kw);
	for (auto& dkp : mVariantData)
		for (auto& d : dkp.second) {
			memset(d.second->mDirty, true, sizeof(bool) * dkp.first->MaxFramesInFlight());
			d.second->mShaderVariant = nullptr;
		}
}
void Material::DisableKeyword(const string& kw) {
	if (!mShaderKeywords.count(kw)) return;
	mShaderKeywords.erase(kw);
	for (auto& dkp : mVariantData)
		for (auto& d : dkp.second) {
			memset(d.second->mDirty, true, sizeof(bool) * dkp.first->MaxFramesInFlight());
			d.second->mShaderVariant = nullptr;
		}
}

void Material::SetParameter(const string& name, const MaterialParameter& param) {
	MaterialParameter& p = mParameters[name];
	if (p != param) {
		p = param;
		if (param.index() < 4) // push constants dont make descriptors dirty
			for (auto& dkp : mVariantData)
				for (auto& d : dkp.second)
					memset(d.second->mDirty, true, sizeof(bool) * dkp.first->MaxFramesInFlight());
	}
}
void Material::SetParameter(const string& name, uint32_t index, Texture* param) {
	auto& p = mArrayParameters[name][index];
	if (p.index() != 1 || get<Texture*>(p) != param) {
		p = param;
		for (auto& dkp : mVariantData)
			for (auto& d : dkp.second)
				memset(d.second->mDirty, true, sizeof(bool) * dkp.first->MaxFramesInFlight());
	}
}
void Material::SetParameter(const string& name, uint32_t index, shared_ptr<Texture> param) {
	auto& p = mArrayParameters[name][index];
	if (p.index() != 0 || get<shared_ptr<Texture>>(p) != param) {
		p = param;
		for (auto& dkp : mVariantData)
			for (auto& d : dkp.second)
				memset(d.second->mDirty, true, sizeof(bool) * dkp.first->MaxFramesInFlight());
	}
}

Material::VariantData* Material::GetData(Device* device, PassType pass) {
	auto& d = mVariantData[device];
	if (d.count(pass) == 0) {
		GraphicsShader* shader = Shader()->GetGraphics(device, pass, mShaderKeywords);
		if (!shader) return nullptr;

		VariantData* data = new VariantData();
		data->mDescriptorSets = new DescriptorSet*[device->MaxFramesInFlight()];
		data->mDirty = new bool[device->MaxFramesInFlight()];
		memset(data->mDescriptorSets, 0, sizeof(DescriptorSet*) * device->MaxFramesInFlight());
		memset(data->mDirty, true, sizeof(bool) * device->MaxFramesInFlight());
		data->mShaderVariant  = shader;
		d.emplace(pass, data);
		return data;
	}

	auto& data = d.at(pass);
	if (!data->mShaderVariant) data->mShaderVariant = Shader()->GetGraphics(device, pass, mShaderKeywords);
	return data;
}
GraphicsShader* Material::GetShader(Device* device, PassType pass) {
	return GetData(device, pass)->mShaderVariant;
}

void Material::SetDescriptorParameters(CommandBuffer* commandBuffer, Camera* camera, VariantData* data) {
	GraphicsShader* shader = data->mShaderVariant;
	if (shader->mDescriptorSetLayouts.size() > PER_MATERIAL&& shader->mDescriptorBindings.size()) {
		uint32_t frameContextIndex = commandBuffer->Device()->FrameContextIndex();
		DescriptorSet*& ds = data->mDescriptorSets[frameContextIndex];
		if (!ds || (ds->Layout() != shader->mDescriptorSetLayouts[PER_MATERIAL])) {
			safe_delete(ds);
			ds = new DescriptorSet(mName + " DescriptorSet", commandBuffer->Device(), shader->mDescriptorSetLayouts[PER_MATERIAL]);
			data->mDirty[frameContextIndex] = true;
		}

		// set descriptor parameters
		if (data->mDirty[frameContextIndex]) {
			PROFILER_BEGIN_RESUME("Write Descriptor Sets");
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

			ds->FlushWrites();
			data->mDirty[frameContextIndex] = false;
			PROFILER_END;
		}

		VkDescriptorSet matds = *ds;
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->mPipelineLayout, PER_MATERIAL, 1, &matds, 0, nullptr);
	}

	if (camera && shader->mDescriptorSetLayouts.size() > PER_CAMERA&& shader->mDescriptorBindings.size() && shader->mDescriptorBindings.count("Camera")) {
		auto binding = shader->mDescriptorBindings.at("Camera");
		VkDescriptorSet camds = *camera->DescriptorSet(binding.second.stageFlags);
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->mPipelineLayout, PER_CAMERA, 1, &camds, 0, nullptr);
	}

}
void Material::SetPushConstantParameters(CommandBuffer* commandBuffer, Camera* camera, VariantData* data) {
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
		}

		vkCmdPushConstants(*commandBuffer, shader->mPipelineLayout, range.stageFlags, range.offset, range.size, &value);
	}
}