#include <Interface/TextButton.hpp>

#include <Interface/UICanvas.hpp>

#include <Core/DescriptorSet.hpp>
#include <Scene/TextRenderer.hpp>
#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Util/Profiler.hpp>

#include <Shaders/shadercompat.h>

using namespace std;

TextButton::TextButton(const string& name, UICanvas* canvas)
	: UIElement(name, canvas), mTextScale(1.f), mHorizontalAnchor(Middle), mVerticalAnchor(Middle), mShader(nullptr) {}
TextButton::~TextButton() {
	for (auto& d : mDeviceData) {
		for (uint32_t i = 0; i < d.first->MaxFramesInFlight(); i++) {
			safe_delete(d.second.mObjectBuffers[i]);
			safe_delete(d.second.mDescriptorSets[i]);
			safe_delete(d.second.mGlyphBuffers[i]);
		}
		safe_delete(d.second.mDirty);
		safe_delete(d.second.mGlyphBuffers);
		safe_delete_array(d.second.mObjectBuffers);
		safe_delete_array(d.second.mDescriptorSets);
	}
}

uint32_t TextButton::BuildText(Device* device, Buffer*& buffer) {
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
void TextButton::Text(const string& text) {
	mText = text;
	for (auto& d : mDeviceData)
		memset(d.second.mDirty, true, d.first->MaxFramesInFlight() * sizeof(bool));
}

void TextButton::Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, ::Material* materialOverride) {
	if (!mShader) mShader = Canvas()->Scene()->AssetManager()->LoadShader("Shaders/font.shader");
	GraphicsShader* shader = mShader->GetGraphics(commandBuffer->Device(), {});

	if (!mDeviceData.count(commandBuffer->Device())) {
		DeviceData& d = mDeviceData[commandBuffer->Device()];
		d.mGlyphCount = 0;
		d.mDirty = new bool[commandBuffer->Device()->MaxFramesInFlight()];
		d.mGlyphBuffers = new Buffer*[commandBuffer->Device()->MaxFramesInFlight()];
		d.mObjectBuffers = new Buffer*[commandBuffer->Device()->MaxFramesInFlight()];
		d.mDescriptorSets = new DescriptorSet*[commandBuffer->Device()->MaxFramesInFlight()];
		memset(d.mDirty, true, sizeof(bool) * commandBuffer->Device()->MaxFramesInFlight());
		memset(d.mGlyphBuffers, 0, sizeof(Buffer*) * commandBuffer->Device()->MaxFramesInFlight());
		memset(d.mObjectBuffers, 0, sizeof(Buffer*) * commandBuffer->Device()->MaxFramesInFlight());
		memset(d.mDescriptorSets, 0, sizeof(DescriptorSet*) * commandBuffer->Device()->MaxFramesInFlight());
	}
	DeviceData& data = mDeviceData[commandBuffer->Device()];

	if (data.mDirty[backBufferIndex]) {
		data.mGlyphCount = BuildText(commandBuffer->Device(), data.mGlyphBuffers[backBufferIndex]);
		data.mDirty[backBufferIndex] = false;
	}
	if (data.mGlyphCount == 0) return;

	VkPipelineLayout layout = commandBuffer->BindShader(shader, backBufferIndex, nullptr);
	if (!layout) return;

	// Create object buffers
	if (!data.mObjectBuffers[backBufferIndex]) {
		data.mObjectBuffers[backBufferIndex] = new Buffer(mName + " ObjectBuffer", commandBuffer->Device(), sizeof(ObjectBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		data.mObjectBuffers[backBufferIndex]->Map();
	}
	// Create and assign descriptor sets
	if (!data.mDescriptorSets[backBufferIndex]) {
		data.mDescriptorSets[backBufferIndex] = new DescriptorSet(mName + " PerObject DescriptorSet", commandBuffer->Device()->DescriptorPool(), shader->mDescriptorSetLayouts[PER_OBJECT]);
		data.mDescriptorSets[backBufferIndex]->CreateUniformBufferDescriptor(data.mObjectBuffers[backBufferIndex], OBJECT_BUFFER_BINDING);
		data.mDescriptorSets[backBufferIndex]->CreateSampledTextureDescriptor(Font()->Texture(), BINDING_START + 0);
	}
	// Assign glyph buffer
	data.mDescriptorSets[backBufferIndex]->CreateStorageBufferDescriptor(data.mGlyphBuffers[backBufferIndex], BINDING_START + 2);

	float3 offset = AbsolutePosition();
	switch (mHorizontalAnchor) {
	case Minimum:
		offset.x += -AbsoluteExtent().x;
		break;
	case Maximum:
		offset.x += AbsoluteExtent().x;
		break;
	}
	switch (mVerticalAnchor) {
	case Minimum:
		offset.y += -AbsoluteExtent().y;
		break;
	case Maximum:
		offset.y += AbsoluteExtent().y;
		break;
	}
	offset.z = 0;

	// Update object buffer
	ObjectBuffer* objbuffer = (ObjectBuffer*)data.mObjectBuffers[backBufferIndex]->MappedData();
	objbuffer->ObjectToWorld = float4x4::Translate( offset) * Canvas()->ObjectToWorld();
	objbuffer->WorldToObject = float4x4::Translate(-offset) * Canvas()->WorldToObject();

	VkDescriptorSet objds = *data.mDescriptorSets[backBufferIndex];
	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &objds, 0, nullptr);
	vkCmdDraw(*commandBuffer, data.mGlyphCount * 6, 1, 0, 0);
}