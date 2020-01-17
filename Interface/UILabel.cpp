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
	for (Buffer* b : mGlyphBuffers)
		safe_delete(b);
}

uint32_t UILabel::BuildText(Device* device, Buffer*& buffer) {
	PROFILER_BEGIN("Build Text");
	mTempGlyphs.clear();
	mTempGlyphs.reserve(mText.length());
	uint32_t glyphCount = Font()->GenerateGlyphs(mText, mTextScale, &mTextAABB, mTempGlyphs, mHorizontalAnchor, mVerticalAnchor);

	if (glyphCount == 0) { PROFILER_END; return 0; }

	if (buffer && buffer->Size() < glyphCount * sizeof(TextGlyph))
		safe_delete(buffer);
	if (!buffer) buffer = new Buffer(mName + " Glyph Buffer", device, nullptr, glyphCount * sizeof(TextGlyph), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	buffer->Upload(mTempGlyphs.data(), glyphCount * sizeof(TextGlyph));
	PROFILER_END;
	return glyphCount;
}

void UILabel::Draw(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {
	if (!mVisible || !Font()) return;

	if (mDirty.size() != commandBuffer->Device()->MaxFramesInFlight()){
		mDirty.resize(commandBuffer->Device()->MaxFramesInFlight());
		mDirty.assign(mDirty.size(), true);
		mGlyphBuffers.resize(commandBuffer->Device()->MaxFramesInFlight());
		mGlyphBuffers.assign(mGlyphBuffers.size(), nullptr);
	}
	
	uint32_t frameContextIndex = commandBuffer->Device()->FrameContextIndex();
	
	if (mDirty[frameContextIndex]) {
		mGlyphCount = BuildText(commandBuffer->Device(), mGlyphBuffers[frameContextIndex]);
		mDirty[frameContextIndex] = false;
	}
	if (mGlyphCount == 0) return;

	if (!mShader) mShader = Canvas()->Scene()->AssetManager()->LoadShader("Shaders/font.stm");
	GraphicsShader* shader = mShader->GetGraphics(pass, { "CANVAS_BOUNDS" });
	if (!shader) return;

	VkPipelineLayout layout = commandBuffer->BindShader(shader, pass, nullptr, camera);
	if (!layout) return;

	DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet(mName + " PerObject DescriptorSet", shader->mDescriptorSetLayouts[PER_OBJECT]);
	ds->CreateSampledTextureDescriptor(Font()->Texture(), BINDING_START + 0);
	ds->CreateStorageBufferDescriptor(mGlyphBuffers[frameContextIndex], 0, mGlyphBuffers[frameContextIndex]->Size(), BINDING_START + 2);
	ds->FlushWrites();

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

	
	float4x4 mt = Canvas()->ObjectToWorld();
	float2 bounds = Canvas()->Extent();
	float3 normal = Canvas()->WorldRotation().forward();

	commandBuffer->PushConstant(shader, "ObjectToWorld", &mt);
	commandBuffer->PushConstant(shader, "WorldNormal", &normal);
	commandBuffer->PushConstant(shader, "Color", &mColor);
	commandBuffer->PushConstant(shader, "Offset", &offset);
	commandBuffer->PushConstant(shader, "Bounds", &bounds);

	VkDescriptorSet objds = *ds;
	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &objds, 0, nullptr);
	vkCmdDraw(*commandBuffer, mGlyphCount * 6, 1, 0, 0);
}