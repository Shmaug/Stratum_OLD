#include <Interface/UILabel.hpp>

#include <Interface/UICanvas.hpp>

#include <Core/DescriptorSet.hpp>
#include <Scene/TextRenderer.hpp>
#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Util/Profiler.hpp>

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

	PROFILER_BEGIN("Upload Text");
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

void UILabel::Draw(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {
	if (!mVisible || !Font()) return;
	if (!mShader) mShader = Canvas()->Scene()->AssetManager()->LoadShader("Shaders/font.stm");
	GraphicsShader* shader = mShader->GetGraphics(commandBuffer->Device(), pass, { "CANVAS_BOUNDS" });
	if (!shader) return;

	VkPipelineLayout layout = commandBuffer->BindShader(shader, pass, nullptr, camera);
	if (!layout) return;

	if (!mDeviceData.count(commandBuffer->Device())) {
		DeviceData& d = mDeviceData[commandBuffer->Device()];
		d.mGlyphCount = 0;
		uint32_t c = commandBuffer->Device()->MaxFramesInFlight();
		d.mDirty = new bool[c];
		d.mGlyphBuffers = new Buffer*[c];
		d.mDescriptorSets = new DescriptorSet*[c];
		memset(d.mDirty, true, sizeof(bool) * c);
		memset(d.mGlyphBuffers, 0, sizeof(Buffer*) * c);
		memset(d.mDescriptorSets, 0, sizeof(DescriptorSet*) * c);
	}
	DeviceData& data = mDeviceData[commandBuffer->Device()];

	uint32_t frameContextIndex = commandBuffer->Device()->FrameContextIndex();

	if (data.mDirty[frameContextIndex]) {
		data.mGlyphCount = BuildText(commandBuffer->Device(), data.mGlyphBuffers[frameContextIndex]);
		data.mDirty[frameContextIndex] = false;
	}
	if (data.mGlyphCount == 0) return;

	// Create and assign descriptor sets
	if (!data.mDescriptorSets[frameContextIndex]) {
		data.mDescriptorSets[frameContextIndex] = new DescriptorSet(mName + " PerObject DescriptorSet", commandBuffer->Device(), shader->mDescriptorSetLayouts[PER_OBJECT]);
		data.mDescriptorSets[frameContextIndex]->CreateSampledTextureDescriptor(Font()->Texture(), BINDING_START + 0);
	}
	// Assign glyph buffer
	data.mDescriptorSets[frameContextIndex]->CreateStorageBufferDescriptor(data.mGlyphBuffers[frameContextIndex], 0, data.mGlyphBuffers[frameContextIndex]->Size(), BINDING_START + 2);
	data.mDescriptorSets[frameContextIndex]->FlushWrites();

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

	
	float4x4 mt = Canvas()->ObjectToWorld() * float4x4::Translate(-camera->WorldPosition()) * camera->ViewProjection();
	float2 bounds = Canvas()->Extent();
	float3 normal = Canvas()->WorldRotation().forward();

	VkPushConstantRange wvp = shader->mPushConstants.at("WorldViewProjection");
	VkPushConstantRange nrm = shader->mPushConstants.at("WorldNormal");
	VkPushConstantRange colorRange = shader->mPushConstants.at("Color");
	VkPushConstantRange offsetRange = shader->mPushConstants.at("Offset");
	VkPushConstantRange boundsRange = shader->mPushConstants.at("Bounds");

	VkDescriptorSet objds = *data.mDescriptorSets[frameContextIndex];
	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &objds, 0, nullptr);
	vkCmdPushConstants(*commandBuffer, layout, nrm.stageFlags, nrm.offset, nrm.size, &mt);
	vkCmdPushConstants(*commandBuffer, layout, nrm.stageFlags, nrm.offset, nrm.size, &normal);
	vkCmdPushConstants(*commandBuffer, layout, offsetRange.stageFlags, offsetRange.offset, offsetRange.size, &offset);
	vkCmdPushConstants(*commandBuffer, layout, colorRange.stageFlags, colorRange.offset, colorRange.size, &mColor);
	vkCmdPushConstants(*commandBuffer, layout, boundsRange.stageFlags, boundsRange.offset, boundsRange.size, &bounds);
	vkCmdDraw(*commandBuffer, data.mGlyphCount * 6, 1, 0, 0);
}