#include <Content/Texture.hpp>
#include <Core/DescriptorSet.hpp>
#include <Scene/TextRenderer.hpp>
#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Util/Profiler.hpp>

using namespace std;

TextRenderer::TextRenderer(const string& name)
	: Object(name), mVisible(true), mColor(float4(1)), mTextScale(1.f), mHorizontalAnchor(Middle), mVerticalAnchor(Middle), mShader(nullptr) {}
TextRenderer::~TextRenderer() {}

bool TextRenderer::UpdateTransform() {
	if (!Object::UpdateTransform()) return false;
	mAABB = mTextAABB * ObjectToWorld();
	return true;
}

uint32_t TextRenderer::BuildText(Device* device, Buffer*& buffer) {
	mTempGlyphs.clear();
	mTempGlyphs.reserve(mText.length());
	uint32_t glyphCount = Font()->GenerateGlyphs(mText, mTextScale, &mTextAABB, mTempGlyphs, mHorizontalAnchor, mVerticalAnchor);
	mAABB = mTextAABB * ObjectToWorld();

	if (glyphCount == 0) return 0;

	buffer = device->GetTempBuffer(mName + " Glyph Buffer", glyphCount * sizeof(TextGlyph), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	buffer->Upload(mTempGlyphs.data(), glyphCount * sizeof(TextGlyph));
	return glyphCount;
}

void TextRenderer::Text(const string& text) {
	mText = text;
}

void TextRenderer::Draw(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {
	if (pass != PASS_MAIN) return;

	uint32_t frameContextIndex = commandBuffer->Device()->FrameContextIndex();

	Buffer* glyphBuffer;
	uint32_t glyphCount = BuildText(commandBuffer->Device(), glyphBuffer);
	if (!glyphCount) return;

	if (!mShader) mShader = Scene()->AssetManager()->LoadShader("Shaders/font.stm");
	GraphicsShader* shader = mShader->GetGraphics(pass, {});
	if (!shader) return;

	VkPipelineLayout layout = commandBuffer->BindShader(shader, pass, nullptr, camera);
	if (!layout) return;

	DescriptorSet* descriptorSet = commandBuffer->Device()->GetTempDescriptorSet(mName + " DescriptorSet", shader->mDescriptorSetLayouts[PER_OBJECT]);
	descriptorSet->CreateSampledTextureDescriptor(Font()->Texture(), BINDING_START + 0);
	descriptorSet->CreateStorageBufferDescriptor(glyphBuffer, 0, glyphBuffer->Size(), BINDING_START + 2);
	descriptorSet->FlushWrites();

	float4x4 o2w = ObjectToWorld();
	float3 normal = WorldRotation().forward();
	float2 offset(0);

	VkPushConstantRange nrm = shader->mPushConstants.at("WorldNormal");
	VkPushConstantRange colorRange = shader->mPushConstants.at("Color");
	VkPushConstantRange offsetRange = shader->mPushConstants.at("Offset");

	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, *descriptorSet, 0, nullptr);
	commandBuffer->PushConstant(shader, "ObjectToWorld", &o2w);
	commandBuffer->PushConstant(shader, "WorldNormal", &normal);
	commandBuffer->PushConstant(shader, "Offset", &offset);
	commandBuffer->PushConstant(shader, "Color", &mColor);
	vkCmdDraw(*commandBuffer, glyphCount * 6, 1, 0, 0);
	commandBuffer->mTriangleCount += glyphCount * 2;
}