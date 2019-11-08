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
		for (uint32_t i = 0; i < d.first->MaxFramesInFlight(); i++)
			safe_delete(d.second.mDescriptorSets[i]);
		safe_delete_array(d.second.mDescriptorSets);
	}
}

void UIImage::Draw(CommandBuffer* commandBuffer, uint32_t backBufferIndex, Camera* camera, ::Material* materialOverride) {
	if (!Texture() || !mVisible) return;
	if (mColor.a <= 0 && (!mOutline || mOutlineColor.a <= 0)) return;
	if (!mShader) mShader = Canvas()->Scene()->AssetManager()->LoadShader("Shaders/ui.shader");

	GraphicsShader* shader = mShader->GetGraphics(commandBuffer->Device(), {});
	
	if (!mDeviceData.count(commandBuffer->Device())) {
		DeviceData& d = mDeviceData[commandBuffer->Device()];
		d.mDescriptorSets = new DescriptorSet*[commandBuffer->Device()->MaxFramesInFlight()];
		memset(d.mDescriptorSets, 0, sizeof(DescriptorSet*) * commandBuffer->Device()->MaxFramesInFlight());
	}
	DeviceData& data = mDeviceData[commandBuffer->Device()];

	if (!data.mDescriptorSets[backBufferIndex]) {
		data.mDescriptorSets[backBufferIndex] = new DescriptorSet(mName + " PerObject DescriptorSet", commandBuffer->Device()->DescriptorPool(), shader->mDescriptorSetLayouts[PER_OBJECT]);
		data.mDescriptorSets[backBufferIndex]->CreateSampledTextureDescriptor(Texture(), BINDING_START + 0);
	}

	VkPushConstantRange o2w = shader->mPushConstants.at("ObjectToWorld");
	VkPushConstantRange w2o = shader->mPushConstants.at("WorldToObject");
	VkPushConstantRange colorRange = shader->mPushConstants.at("Color");
	VkPushConstantRange offsetRange = shader->mPushConstants.at("Offset");
	VkPushConstantRange extentRange = shader->mPushConstants.at("Extent");
	VkPushConstantRange boundsRange = shader->mPushConstants.at("Bounds");
	VkDescriptorSet objds = *data.mDescriptorSets[backBufferIndex];

	float2 offset = AbsolutePosition();
	float2 extent = AbsoluteExtent();
	float2 bounds = Canvas()->Extent();

	if (mColor.a > 0) {
		VkPipelineLayout layout = commandBuffer->BindShader(shader, backBufferIndex, nullptr, camera);
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &objds, 0, nullptr);
		float4x4 mt = Canvas()->ObjectToWorld();
		vkCmdPushConstants(*commandBuffer, layout, o2w.stageFlags, o2w.offset, o2w.size, &mt);
		mt = Canvas()->WorldToObject();
		vkCmdPushConstants(*commandBuffer, layout, w2o.stageFlags, w2o.offset, w2o.size, &mt);

		vkCmdPushConstants(*commandBuffer, layout, colorRange.stageFlags, colorRange.offset, colorRange.size, &mColor);
		vkCmdPushConstants(*commandBuffer, layout, offsetRange.stageFlags, offsetRange.offset, offsetRange.size, &offset);
		vkCmdPushConstants(*commandBuffer, layout, extentRange.stageFlags, extentRange.offset, extentRange.size, &extent);
		vkCmdPushConstants(*commandBuffer, layout, boundsRange.stageFlags, boundsRange.offset, boundsRange.size, &bounds);
		vkCmdDraw(*commandBuffer, 6, 1, 0, 0);
	}

	if (mOutline && mOutlineColor.a > 0) {
		VkPipelineLayout layout = commandBuffer->BindShader(shader, backBufferIndex, nullptr, camera, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP);
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &objds, 0, nullptr);
		float4x4 mt = Canvas()->ObjectToWorld();
		vkCmdPushConstants(*commandBuffer, layout, o2w.stageFlags, o2w.offset, o2w.size, &mt);
		mt = Canvas()->WorldToObject();
		vkCmdPushConstants(*commandBuffer, layout, w2o.stageFlags, w2o.offset, w2o.size, &mt);

		vkCmdPushConstants(*commandBuffer, layout, colorRange.stageFlags, colorRange.offset, colorRange.size, &mOutlineColor);
		vkCmdPushConstants(*commandBuffer, layout, offsetRange.stageFlags, offsetRange.offset, offsetRange.size, &offset);
		vkCmdPushConstants(*commandBuffer, layout, extentRange.stageFlags, extentRange.offset, extentRange.size, &extent);
		vkCmdPushConstants(*commandBuffer, layout, boundsRange.stageFlags, boundsRange.offset, boundsRange.size, &bounds);
		vkCmdDraw(*commandBuffer, 5, 1, 0, 0);
	}
}