#include <Content/Texture.hpp>
#include <Core/DescriptorSet.hpp>
#include <Scene/TextRenderer.hpp>
#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Util/Profiler.hpp>

#include <Shaders/shadercompat.h>

using namespace std;

TextRenderer::TextRenderer(const string& name)
	: Object(name), mVisible(true), mColor(float4(1)), mTextScale(1.f), mHorizontalAnchor(Middle), mVerticalAnchor(Middle), mShader(nullptr) {}
TextRenderer::~TextRenderer() {
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

bool TextRenderer::UpdateTransform() {
	if (!Object::UpdateTransform()) return false;
	mAABB = AABB(mTextAABB, ObjectToWorld());
	return true;
}

uint32_t TextRenderer::BuildText(Device* device, Buffer*& buffer) {
	PROFILER_BEGIN("Build Text");
	mTempGlyphs.clear();
	mTempGlyphs.reserve(mText.length());
	uint32_t glyphCount = Font()->GenerateGlyphs(mText, mTextScale, mTextAABB, mTempGlyphs, mHorizontalAnchor, mVerticalAnchor);
	mAABB = AABB(mTextAABB, ObjectToWorld());
	mTextAABB.mExtents.z = .001f;
	PROFILER_END;

	if (glyphCount == 0) return 0;

	PROFILER_BEGIN("Upload");
	if (buffer && buffer->Size() < glyphCount * sizeof(TextGlyph))
		safe_delete(buffer);
	if (!buffer)
		buffer = new Buffer(mName + " Glyph Buffer", device, nullptr, glyphCount * sizeof(TextGlyph), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	buffer->Upload(mTempGlyphs.data(), glyphCount * sizeof(TextGlyph));
	PROFILER_END;
	return glyphCount;
}

void TextRenderer::Text(const string& text) {
	mText = text;
	for (auto& d : mDeviceData)
		memset(d.second.mDirty, true, d.first->MaxFramesInFlight() * sizeof(bool));
}

void TextRenderer::Draw(CommandBuffer* commandBuffer, Camera* camera, Scene::PassType pass) {
	if (!mDeviceData.count(commandBuffer->Device())) {
		DeviceData& d = mDeviceData[commandBuffer->Device()];
		d.mGlyphCount = 0;
		uint32_t c = commandBuffer->Device()->MaxFramesInFlight();
		d.mDirty = new bool[c];
		d.mGlyphBuffers = new Buffer*[c];
		d.mDescriptorSets = new DescriptorSet*[c];
		memset(d.mDirty, true, sizeof(bool)*c);
		memset(d.mGlyphBuffers, 0, sizeof(Buffer*) * c);
		memset(d.mDescriptorSets, 0, sizeof(DescriptorSet*) * c);
	}
	DeviceData& data = mDeviceData[commandBuffer->Device()];
	uint32_t frameContextIndex = commandBuffer->Device()->FrameContextIndex();

	if (data.mDirty[frameContextIndex]) {
		data.mGlyphCount = BuildText(commandBuffer->Device(), data.mGlyphBuffers[frameContextIndex]);
		data.mDirty[frameContextIndex] = false;
	}
	if (!data.mGlyphCount) return;

	if (!mShader) mShader = Scene()->AssetManager()->LoadShader("Shaders/font.shader");
	GraphicsShader* shader = mShader->GetGraphics(commandBuffer->Device(), {});

	VkPipelineLayout layout = commandBuffer->BindShader(shader, nullptr, camera);
	if (!layout) return;

	// Create and assign descriptor sets
	if (!data.mDescriptorSets[frameContextIndex]) {
		data.mDescriptorSets[frameContextIndex] = new DescriptorSet(mName + " PerObject DescriptorSet", commandBuffer->Device(), shader->mDescriptorSetLayouts[PER_OBJECT]);
		data.mDescriptorSets[frameContextIndex]->CreateSampledTextureDescriptor(Font()->Texture(), BINDING_START + 0);
	}
	// Assign glyph buffer
	data.mDescriptorSets[frameContextIndex]->CreateStorageBufferDescriptor(data.mGlyphBuffers[frameContextIndex], 0, data.mGlyphBuffers[frameContextIndex]->Size(), BINDING_START + 2);

	VkPushConstantRange o2w = shader->mPushConstants.at("ObjectToWorld");
	VkPushConstantRange w2o = shader->mPushConstants.at("WorldToObject");
	float4x4 mt = ObjectToWorld();
	vkCmdPushConstants(*commandBuffer, layout, o2w.stageFlags, o2w.offset, o2w.size, &mt);
	mt = WorldToObject();
	vkCmdPushConstants(*commandBuffer, layout, w2o.stageFlags, w2o.offset, w2o.size, &mt);

	float2 offset(0);
	VkPushConstantRange colorRange = shader->mPushConstants.at("Color");
	VkPushConstantRange offsetRange = shader->mPushConstants.at("Offset");

	VkDescriptorSet objds = *data.mDescriptorSets[frameContextIndex];
	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &objds, 0, nullptr);
	vkCmdPushConstants(*commandBuffer, layout, offsetRange.stageFlags, offsetRange.offset, offsetRange.size, &offset);
	vkCmdPushConstants(*commandBuffer, layout, colorRange.stageFlags, colorRange.offset, colorRange.size, &mColor);
	vkCmdDraw(*commandBuffer, data.mGlyphCount * 6, 1, 0, 0);
	commandBuffer->mTriangleCount += data.mGlyphCount * 2;
}