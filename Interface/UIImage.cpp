#include <Interface/UIImage.hpp>

#include <Interface/UICanvas.hpp>

#include <Core/DescriptorSet.hpp>
#include <Scene/Camera.hpp>
#include <Util/Profiler.hpp>

#include <Shaders/shadercompat.h>

using namespace std;

UIImage::UIImage(const string& name) : UIElement(name), mTexture((::Texture*)nullptr), mColor(float4(1,1,1,1)) {}
UIImage::~UIImage() {
	for (auto& d : mDeviceData) {
		for (uint32_t i = 0; i < d.first->MaxFramesInFlight(); i++) {
			safe_delete(d.second.mObjectBuffers[i]);
			safe_delete(d.second.mDescriptorSets[i]);
		}
		safe_delete_array(d.second.mDescriptorSets);
	}
}

void UIImage::Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, ::Material* materialOverride) {
	::Material* material = materialOverride ? materialOverride : mMaterial.get();
	if (!material || !Texture()) return;

	VkPipelineLayout layout = commandBuffer->BindMaterial(material, backBufferIndex, nullptr);
	if (!layout) return;

	if (!mDeviceData.count(commandBuffer->Device())) {
		DeviceData& d = mDeviceData[commandBuffer->Device()];
		d.mDescriptorSets = new DescriptorSet*[commandBuffer->Device()->MaxFramesInFlight()];
		d.mObjectBuffers = new Buffer*[commandBuffer->Device()->MaxFramesInFlight()];
		memset(d.mDescriptorSets, 0, sizeof(DescriptorSet*) * commandBuffer->Device()->MaxFramesInFlight());
		memset(d.mObjectBuffers, 0, sizeof(Buffer*) * commandBuffer->Device()->MaxFramesInFlight());
	}
	DeviceData& data = mDeviceData[commandBuffer->Device()];

	if (!data.mObjectBuffers[backBufferIndex]) {
		data.mObjectBuffers[backBufferIndex] = new Buffer(mName + " ObjectBuffer", commandBuffer->Device(), sizeof(ObjectBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		data.mObjectBuffers[backBufferIndex]->Map();
	}
	if (!data.mDescriptorSets[backBufferIndex]) {
		auto shader = mMaterial->GetShader(commandBuffer->Device());
		data.mDescriptorSets[backBufferIndex] = new DescriptorSet(mName + " PerObject DescriptorSet", commandBuffer->Device()->DescriptorPool(), shader->mDescriptorSetLayouts[PER_OBJECT]);
		data.mDescriptorSets[backBufferIndex]->CreateUniformBufferDescriptor(data.mObjectBuffers[backBufferIndex], OBJECT_BUFFER_BINDING);
		data.mDescriptorSets[backBufferIndex]->CreateSampledTextureDescriptor(Texture(), shader->mDescriptorBindings.at("MainTexture").second.binding);
	}

	float3 offset = AbsolutePosition();
	offset.z = 0;
	
	ObjectBuffer* objbuffer = (ObjectBuffer*)data.mObjectBuffers[backBufferIndex]->MappedData();
	objbuffer->ObjectToWorld = float4x4::Scale(float3(AbsoluteExtent(), 1)) * Canvas()->ObjectToWorld() * float4x4::Translate(offset);
	objbuffer->WorldToObject = float4x4::Scale(float3(1.f / AbsoluteExtent(), 1)) * Canvas()->WorldToObject() * float4x4::Translate(-offset);

	VkDescriptorSet objds = *data.mDescriptorSets[backBufferIndex];
	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &objds, 0, nullptr);

	VkPushConstantRange colorRange = mMaterial->GetShader(commandBuffer->Device())->mPushConstants.at("Color");
	vkCmdPushConstants(*commandBuffer, layout, colorRange.stageFlags, colorRange.offset, colorRange.size, &mColor);
	vkCmdDraw(*commandBuffer, 6, 1, 0, 0);
}