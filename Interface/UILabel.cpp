#include <Interface/UILabel.hpp>

#include <Interface/UICanvas.hpp>

#include <Core/DescriptorSet.hpp>
#include <Scene/TextRenderer.hpp>
#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Util/Profiler.hpp>

#include <Shaders/shadercompat.h>

using namespace std;

UILabel::UILabel(const string& name, UICanvas* canvas)
	: UIElement(name, canvas), mColor(float4(1)), mTextScale(1.f), mHorizontalAnchor(Middle), mVerticalAnchor(Middle), mShader(nullptr) {}
UILabel::~UILabel() {
	for (auto& d : mDeviceData) {
		for (uint32_t i = 0; i < d.first->MaxFramesInFlight(); i++) {
			safe_delete(d.second.mDescriptorSets[i]);
			safe_delete(d.second.mGlyphBuffers[i]);
		}
		safe_delete(d.second.mDirty);
		safe_delete(d.second.mGlyphBuffers);
		safe_delete_array(d.second.mDescriptorSets);
	}
}

uint32_t UILabel::BuildText(Device* device, Buffer*& buffer) {
	PROFILER_BEGIN("Build Text");
	mTempGlyphs.clear();
	mTempGlyphs.reserve(mText.length());
	uint32_t glyphCount = Font()->GenerateGlyphs(mText, mTextScale, mTextAABB, mTempGlyphs, mHorizontalAnchor, mVerticalAnchor);
	PROFILER_END;

	if (glyphCount == 0) return 0;

	PROFILER_BEGIN("Upload");
	if (buffer && buffer->Size() < glyphCount * sizeof(TextGlyph))
		safe_delete(buffer);
	if (!buffer)
		buffer = new Buffer(mName + " Glyph Buffer", device, nullptr, glyphCount * sizeof(TextGlyph), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	buffer->Upload(mTempGlyphs.data(), glyphCount * sizeof(TextGlyph));
	PROFILER_END;
	return glyphCount;
}
void UILabel::Text(const string& text) {
	mText = text;
	for (auto& d : mDeviceData)
		memset(d.second.mDirty, true, d.first->MaxFramesInFlight() * sizeof(bool));
}

void UILabel::Draw(Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, ::Material* materialOverride) {
	if (!mVisible || !Font()) return;
	if (!mShader) mShader = Canvas()->Scene()->AssetManager()->LoadShader("Shaders/font.shader");
	GraphicsShader* shader = mShader->GetGraphics(commandBuffer->Device(), {"CANVAS_BOUNDS"});

	VkPipelineLayout layout = commandBuffer->BindShader(shader, backBufferIndex, nullptr);
	if (!layout) return;

	if (!mDeviceData.count(commandBuffer->Device())) {
		DeviceData& d = mDeviceData[commandBuffer->Device()];
		d.mGlyphCount = 0;
		d.mDirty = new bool[commandBuffer->Device()->MaxFramesInFlight()];
		d.mGlyphBuffers = new Buffer*[commandBuffer->Device()->MaxFramesInFlight()];
		d.mDescriptorSets = new DescriptorSet*[commandBuffer->Device()->MaxFramesInFlight()];
		memset(d.mDirty, true, sizeof(bool) * commandBuffer->Device()->MaxFramesInFlight());
		memset(d.mGlyphBuffers, 0, sizeof(Buffer*) * commandBuffer->Device()->MaxFramesInFlight());
		memset(d.mDescriptorSets, 0, sizeof(DescriptorSet*) * commandBuffer->Device()->MaxFramesInFlight());
	}
	DeviceData& data = mDeviceData[commandBuffer->Device()];

	if (data.mDirty[backBufferIndex]) {
		data.mGlyphCount = BuildText(commandBuffer->Device(), data.mGlyphBuffers[backBufferIndex]);
		data.mDirty[backBufferIndex] = false;
	}
	if (data.mGlyphCount == 0) return;

	// Create and assign descriptor sets
	if (!data.mDescriptorSets[backBufferIndex]) {
		data.mDescriptorSets[backBufferIndex] = new DescriptorSet(mName + " PerObject DescriptorSet", commandBuffer->Device()->DescriptorPool(), shader->mDescriptorSetLayouts[PER_OBJECT]);
		data.mDescriptorSets[backBufferIndex]->CreateSampledTextureDescriptor(Font()->Texture(), BINDING_START + 0);
	}
	// Assign glyph buffer
	data.mDescriptorSets[backBufferIndex]->CreateStorageBufferDescriptor(data.mGlyphBuffers[backBufferIndex], BINDING_START + 2);

	float2 offset = AbsolutePosition();
	switch (mHorizontalAnchor) {
	case Minimum:
		offset.x -= AbsoluteExtent().x;
		break;
	case Maximum:
		offset.x += AbsoluteExtent().x;
		break;
	}
	switch (mVerticalAnchor) {
	case Minimum:
		offset.y -= AbsoluteExtent().y;
		break;
	case Maximum:
		offset.y += AbsoluteExtent().y;
		break;
	}

	VkPushConstantRange o2w = shader->mPushConstants.at("ObjectToWorld");
	VkPushConstantRange w2o = shader->mPushConstants.at("WorldToObject");
	float4x4 mt = Canvas()->ObjectToWorld();
	vkCmdPushConstants(*commandBuffer, layout, o2w.stageFlags, o2w.offset, o2w.size, &mt);
	mt = Canvas()->WorldToObject();
	vkCmdPushConstants(*commandBuffer, layout, w2o.stageFlags, w2o.offset, w2o.size, &mt);

	float2 bounds = Canvas()->Extent();

	VkPushConstantRange colorRange = shader->mPushConstants.at("Color");
	VkPushConstantRange offsetRange = shader->mPushConstants.at("Offset");
	VkPushConstantRange boundsRange = shader->mPushConstants.at("Bounds");

	VkDescriptorSet objds = *data.mDescriptorSets[backBufferIndex];
	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &objds, 0, nullptr);
	vkCmdPushConstants(*commandBuffer, layout, offsetRange.stageFlags, offsetRange.offset, offsetRange.size, &offset);
	vkCmdPushConstants(*commandBuffer, layout, colorRange.stageFlags, colorRange.offset, colorRange.size, &mColor);
	vkCmdPushConstants(*commandBuffer, layout, boundsRange.stageFlags, boundsRange.offset, boundsRange.size, &bounds);
	vkCmdDraw(*commandBuffer, data.mGlyphCount * 6, 1, 0, 0);
}