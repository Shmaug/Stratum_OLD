#include <glm/gtx/quaternion.hpp>

#include <Core/DescriptorSet.hpp>
#include <Scene/TextRenderer.hpp>
#include <Scene/Camera.hpp>
#include <Util/Profiler.hpp>

#include <Shaders/shadercompat.h>

using namespace std;

TextRenderer::TextRenderer(const string& name) : Renderer(name), mVisible(true), mTextScale(1.f), mHorizontalAnchor(Left), mVerticalAnchor(Top) {}
TextRenderer::~TextRenderer() {
	for (auto& d : mDeviceData) {
		for (uint32_t i = 0; i < d.first->MaxFramesInFlight(); i++) {
			safe_delete(d.second.mObjectBuffers[i]);
			safe_delete(d.second.mDescriptorSets[i]);
		}
		safe_delete(d.second.mGlyphBuffer);
		safe_delete_array(d.second.mObjectBuffers);
		safe_delete_array(d.second.mDescriptorSets);
	}
}

void TextRenderer::BuildText(Device* device, DeviceData& d) {
	PROFILER_BEGIN("Build Text");
	vector<TextGlyph> glyphs;
	uint32_t glyphCount = Font()->GenerateGlyphs(mText, mTextScale, mTextAABB, glyphs, mHorizontalAnchor, mVerticalAnchor);
	PROFILER_END;

	if (glyphCount == 0) return;

	PROFILER_BEGIN("Upload");
	if (d.mGlyphBuffer && d.mGlyphCount < glyphCount)
		safe_delete(d.mGlyphBuffer);
	if (!d.mGlyphBuffer)
		d.mGlyphBuffer = new Buffer(mName + " Glyph Buffer", device, nullptr, glyphCount * sizeof(TextGlyph), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	d.mGlyphBuffer->Upload(glyphs.data(), glyphCount * sizeof(TextGlyph));
	d.mGlyphCount = glyphCount;
	PROFILER_END;
}

void TextRenderer::Text(const string& text) {
	mText = text;
	for (auto& d : mDeviceData)
		d.second.mDirty = true;
}

void TextRenderer::Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, ::Material* materialOverride) {
	::Material* material = materialOverride ? materialOverride : mMaterial.get();
	if (!material) return;

	if (!mDeviceData.count(commandBuffer->Device())) {
		DeviceData& d = mDeviceData[commandBuffer->Device()];
		d.mDirty = true;
		d.mGlyphBuffer = nullptr;
		d.mGlyphCount = 0;
		d.mObjectBuffers = new Buffer * [commandBuffer->Device()->MaxFramesInFlight()];
		d.mDescriptorSets = new DescriptorSet * [commandBuffer->Device()->MaxFramesInFlight()];
		memset(d.mObjectBuffers, 0, sizeof(Buffer*) * commandBuffer->Device()->MaxFramesInFlight());
		memset(d.mDescriptorSets, 0, sizeof(DescriptorSet*) * commandBuffer->Device()->MaxFramesInFlight());
	}

	DeviceData& data = mDeviceData[commandBuffer->Device()];

	if (data.mDirty) {
		BuildText(commandBuffer->Device(), data);
		data.mDirty = false;
	}

	if (!data.mGlyphCount) return;

	VkPipelineLayout layout = commandBuffer->BindMaterial(material, backBufferIndex, nullptr);
	if (!layout) return;

	if (!data.mObjectBuffers[backBufferIndex]) {
		data.mObjectBuffers[backBufferIndex] = new Buffer(mName + " ObjectBuffer", commandBuffer->Device(), sizeof(ObjectBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		data.mObjectBuffers[backBufferIndex]->Map();
	}
	if (!data.mDescriptorSets[backBufferIndex]) {
		auto shader = mMaterial->GetShader(commandBuffer->Device());
		data.mDescriptorSets[backBufferIndex] = new DescriptorSet(mName + " PerObject DescriptorSet", commandBuffer->Device()->DescriptorPool(), shader->mDescriptorSetLayouts[PER_OBJECT]);
		data.mDescriptorSets[backBufferIndex]->CreateUniformBufferDescriptor(data.mObjectBuffers[backBufferIndex], OBJECT_BUFFER_BINDING);
	}
	data.mDescriptorSets[backBufferIndex]->CreateSRVBufferDescriptor(data.mGlyphBuffer, BINDING_START + 2);

	ObjectBuffer* objbuffer = (ObjectBuffer*)data.mObjectBuffers[backBufferIndex]->MappedData();
	objbuffer->ObjectToWorld = ObjectToWorld();
	objbuffer->WorldToObject = WorldToObject();

	VkDescriptorSet camds = *camera->DescriptorSet(backBufferIndex);
	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_CAMERA, 1, &camds, 0, nullptr);

	VkDescriptorSet objds = *data.mDescriptorSets[backBufferIndex];
	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &objds, 0, nullptr);

	vkCmdDraw(*commandBuffer, data.mGlyphCount * 6, 1, 0, 0);
}

bool TextRenderer::UpdateTransform() {
	if (!Object::UpdateTransform()) return false;
	mAABB = AABB(mTextAABB, ObjectToWorld());
	return true;
}