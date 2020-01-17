#include <Interface/UIImage.hpp>

#include <Interface/UICanvas.hpp>

#include <Content/Shader.hpp>
#include <Core/DescriptorSet.hpp>
#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Util/Profiler.hpp>

using namespace std;

UIImage::UIImage(const string& name, UICanvas* canvas)
	: UIElement(name, canvas), mTexture((::Texture*)nullptr), mColor(float4(1)), mOutlineColor(float4(1)), mOutline(false), mShader(nullptr) {}
UIImage::~UIImage() {}

void UIImage::Draw(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {
	if (!Texture() || !mVisible) return;
	if (mColor.a <= 0 && (!mOutline || mOutlineColor.a <= 0)) return;
	if (!mShader) mShader = Canvas()->Scene()->AssetManager()->LoadShader("Shaders/ui.stm");

	GraphicsShader* shader = mShader->GetGraphics(pass, {});
	if (!shader) return;

	uint32_t frameContextIndex = commandBuffer->Device()->FrameContextIndex();
	DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet(mName + " PerObject DescriptorSet", shader->mDescriptorSetLayouts[PER_OBJECT]);
	ds->CreateSampledTextureDescriptor(Texture(), BINDING_START + 0);
	ds->FlushWrites();

	float2 offset = AbsolutePosition();
	float2 extent = AbsoluteExtent();
	float2 bounds = Canvas()->Extent();
	float4x4 o2w = Canvas()->ObjectToWorld();
	float4x4 w2o = Canvas()->WorldToObject();
	VkDescriptorSet objds = *ds;

	if (mColor.a > 0) {
		VkPipelineLayout layout = commandBuffer->BindShader(shader, pass, nullptr, camera);
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &objds, 0, nullptr);
		commandBuffer->PushConstant(shader, "ObjectToWorld", &o2w);
		commandBuffer->PushConstant(shader, "WorldToObject", &w2o);
		commandBuffer->PushConstant(shader, "Color", &mColor);
		commandBuffer->PushConstant(shader, "Offset", &offset);
		commandBuffer->PushConstant(shader, "Extent", &extent);
		commandBuffer->PushConstant(shader, "Bounds", &bounds);
		vkCmdDraw(*commandBuffer, 6, 1, 0, 0);
	}

	if (mOutline && mOutlineColor.a > 0) {
		VkPipelineLayout layout = commandBuffer->BindShader(shader, pass, nullptr, camera, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP);
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &objds, 0, nullptr);
		commandBuffer->PushConstant(shader, "ObjectToWorld", &o2w);
		commandBuffer->PushConstant(shader, "WorldToObject", &w2o);
		commandBuffer->PushConstant(shader, "Color", &mColor);
		commandBuffer->PushConstant(shader, "Offset", &offset);
		commandBuffer->PushConstant(shader, "Extent", &extent);
		commandBuffer->PushConstant(shader, "Bounds", &bounds);
		vkCmdDraw(*commandBuffer, 5, 1, 0, 0);
	}
}