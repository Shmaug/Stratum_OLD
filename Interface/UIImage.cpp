#include <Interface/UIImage.hpp>

#include <Interface/UICanvas.hpp>

#include <Content/Shader.hpp>
#include <Core/DescriptorSet.hpp>
#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Util/Profiler.hpp>

#include <Shaders/shadercompat.h>

using namespace std;

UIImage::UIImage(const string& name, UICanvas* canvas)
	: UIElement(name, canvas), mTexture((::Texture*)nullptr), mColor(float4(1)), mOutlineColor(float4(1)), mOutline(false), mShader(nullptr) {}
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
	if (!Texture() || !mVisible) return;
	if (!mShader) mShader = Canvas()->Scene()->AssetManager()->LoadShader("Shaders/ui.shader");
	GraphicsShader* shader = mShader->GetGraphics(commandBuffer->Device(), {});
	
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
		data.mDescriptorSets[backBufferIndex] = new DescriptorSet(mName + " PerObject DescriptorSet", commandBuffer->Device()->DescriptorPool(), shader->mDescriptorSetLayouts[PER_OBJECT]);
		data.mDescriptorSets[backBufferIndex]->CreateUniformBufferDescriptor(data.mObjectBuffers[backBufferIndex], OBJECT_BUFFER_BINDING);
		data.mDescriptorSets[backBufferIndex]->CreateSampledTextureDescriptor(Texture(), BINDING_START + 0);
	}

	float2 offset = AbsolutePosition();
	float2 extent = AbsoluteExtent();
	ObjectBuffer* objbuffer = (ObjectBuffer*)data.mObjectBuffers[backBufferIndex]->MappedData();
	objbuffer->ObjectToWorld = Canvas()->ObjectToWorld();
	objbuffer->WorldToObject = Canvas()->WorldToObject();
	
	VkPushConstantRange colorRange = shader->mPushConstants.at("Color");
	VkPushConstantRange offsetRange = shader->mPushConstants.at("Offset");
	VkPushConstantRange extentRange = shader->mPushConstants.at("Extent");
	if (mColor.a > 0) {
		VkPipelineLayout layout = commandBuffer->BindShader(shader, backBufferIndex, nullptr);
		if (!layout) return;
		
		VkDescriptorSet objds = *data.mDescriptorSets[backBufferIndex];
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &objds, 0, nullptr);
		vkCmdPushConstants(*commandBuffer, layout, colorRange.stageFlags, colorRange.offset, colorRange.size, &mColor);
		vkCmdPushConstants(*commandBuffer, layout, offsetRange.stageFlags, offsetRange.offset, offsetRange.size, &offset);
		vkCmdPushConstants(*commandBuffer, layout, extentRange.stageFlags, extentRange.offset, extentRange.size, &extent);
		vkCmdDraw(*commandBuffer, 6, 1, 0, 0);
	}

	if (mOutline && mOutlineColor.a > 0) {
		shader = mShader->GetGraphics(commandBuffer->Device(), {"OUTLINE"});
		VkPipelineLayout layout = commandBuffer->BindShader(shader, backBufferIndex, nullptr, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
		if (!layout) return;

		VkDescriptorSet objds = *data.mDescriptorSets[backBufferIndex];
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &objds, 0, nullptr);
		vkCmdPushConstants(*commandBuffer, layout, colorRange.stageFlags, colorRange.offset, colorRange.size, &mOutlineColor);
		vkCmdPushConstants(*commandBuffer, layout, offsetRange.stageFlags, offsetRange.offset, offsetRange.size, &offset);
		vkCmdPushConstants(*commandBuffer, layout, extentRange.stageFlags, extentRange.offset, extentRange.size, &extent);
		vkCmdDraw(*commandBuffer, 8, 1, 0, 0);
	}
}