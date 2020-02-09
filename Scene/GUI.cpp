#include <Scene/GUI.hpp>
#include <Scene/Scene.hpp>

using namespace std;

size_t GUI::mHotControl = -1;
unordered_map<size_t, pair<Buffer*, uint32_t>> GUI::mGlyphCache;

void GUI::Initialize(Device* device, AssetManager* assetManager) {
	mHotControl = -1;
}
void GUI::Destroy(Device* device){
	for (auto& i : mGlyphCache)
		safe_delete(i.second.first);
}

void GUI::DrawString(CommandBuffer* commandBuffer, Camera* camera, Font* font, const string& str, const float4& color, const float2& screenPos, float scale, TextAnchor horizontalAnchor, TextAnchor verticalAnchor, const float4& clipRect) {
	if (str.length() == 0) return;

	GraphicsShader* shader = camera->Scene()->AssetManager()->LoadShader("Shaders/font.stm")->GetGraphics(PASS_MAIN, { "SCREEN_SPACE" });
	if (!shader) return;
	VkPipelineLayout layout = commandBuffer->BindShader(shader, PASS_MAIN, nullptr);
	if (!layout) return;

	Buffer* glyphBuffer;
	char hashstr[256];
	sprintf(hashstr, "%s%f%d%d", str.c_str(), scale, horizontalAnchor, verticalAnchor);
	size_t key = 0;
	hash_combine(key, font);
	hash_combine(key, string(hashstr));
	if (mGlyphCache.count(key)) {
		auto& b = mGlyphCache.at(key);
		b.second = 8;
		glyphBuffer = b.first;
	} else {
		vector<TextGlyph> glyphs(str.length());
		uint32_t glyphCount = font->GenerateGlyphs(str, scale, nullptr, glyphs, horizontalAnchor, verticalAnchor);
		if (glyphCount == 0) return;
		glyphBuffer = new Buffer("Glyph Buffer", commandBuffer->Device(), glyphCount * sizeof(TextGlyph), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		glyphBuffer->Upload(glyphs.data(), glyphCount * sizeof(TextGlyph));
		mGlyphCache.emplace(key, make_pair(glyphBuffer, 8u));
	}

	DescriptorSet* descriptorSet = commandBuffer->Device()->GetTempDescriptorSet(font->mName + " DescriptorSet", shader->mDescriptorSetLayouts[PER_OBJECT]);
	descriptorSet->CreateSampledTextureDescriptor(font->Texture(), BINDING_START + 0);
	descriptorSet->CreateStorageBufferDescriptor(glyphBuffer, 0, glyphBuffer->Size(), BINDING_START + 2);
	descriptorSet->FlushWrites();

	float2 s(camera->FramebufferWidth(), camera->FramebufferHeight());

	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, *descriptorSet, 0, nullptr);
	commandBuffer->PushConstant(shader, "Color", &color);
	commandBuffer->PushConstant(shader, "Offset", &screenPos);
	commandBuffer->PushConstant(shader, "ScreenSize", &s);
	commandBuffer->PushConstant(shader, "Bounds", &clipRect);
	vkCmdDraw(*commandBuffer, (glyphBuffer->Size() / sizeof(TextGlyph)) * 6, 1, 0, 0);
}
void GUI::DrawString(CommandBuffer* commandBuffer, Camera* camera, Font* font, const string& str, const float4& color, const float4x4& objectToWorld, const float2& offset, float scale, TextAnchor horizontalAnchor, TextAnchor verticalAnchor, const float4& clipRect) {
	if (str.length() == 0) return;

	GraphicsShader* shader = camera->Scene()->AssetManager()->LoadShader("Shaders/font.stm")->GetGraphics(PASS_MAIN, {});
	if (!shader) return;
	VkPipelineLayout layout = commandBuffer->BindShader(shader, PASS_MAIN, nullptr, camera);
	if (!layout) return;

	Buffer* glyphBuffer;
	char hashstr[256];
	sprintf(hashstr, "%s%f%d%d", str.c_str(), scale, horizontalAnchor, verticalAnchor);
	size_t key = 0;
	hash_combine(key, font);
	hash_combine(key, string(hashstr));
	if (mGlyphCache.count(key)) {
		auto& b = mGlyphCache.at(key);
		b.second = 8;
		glyphBuffer = b.first;
	} else {
		vector<TextGlyph> glyphs(str.length());
		uint32_t glyphCount = font->GenerateGlyphs(str, scale, nullptr, glyphs, horizontalAnchor, verticalAnchor);
		if (glyphCount == 0) return;
		glyphBuffer = new Buffer("Glyph Buffer", commandBuffer->Device(), glyphCount * sizeof(TextGlyph), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		glyphBuffer->Upload(glyphs.data(), glyphCount * sizeof(TextGlyph));
		mGlyphCache.emplace(key, make_pair(glyphBuffer, 8u));
	}

	DescriptorSet* descriptorSet = commandBuffer->Device()->GetTempDescriptorSet(font->mName + " DescriptorSet", shader->mDescriptorSetLayouts[PER_OBJECT]);
	descriptorSet->CreateSampledTextureDescriptor(font->Texture(), BINDING_START + 0);
	descriptorSet->CreateStorageBufferDescriptor(glyphBuffer, 0, glyphBuffer->Size(), BINDING_START + 2);
	descriptorSet->FlushWrites();

	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, *descriptorSet, 0, nullptr);
	commandBuffer->PushConstant(shader, "ObjectToWorld", &objectToWorld);
	commandBuffer->PushConstant(shader, "Color", &color);
	commandBuffer->PushConstant(shader, "Offset", &offset);
	commandBuffer->PushConstant(shader, "Bounds", &clipRect);
	vkCmdDraw(*commandBuffer, (glyphBuffer->Size() / sizeof(TextGlyph)) * 6, 1, 0, 0);
}

void GUI::Rect(CommandBuffer* commandBuffer, Camera* camera, const float2& screenPos, const float2& scale, const float4& color, const float4& clipRect) {
	if (screenPos.x + scale.x < clipRect.x || screenPos.x > clipRect.x + clipRect.z ||
		screenPos.y + scale.y < clipRect.y || screenPos.y > clipRect.y + clipRect.w) return;

	GraphicsShader* shader = camera->Scene()->AssetManager()->LoadShader("Shaders/ui.stm")->GetGraphics(PASS_MAIN, { "SCREEN_SPACE" });
	if (!shader) return;

	VkPipelineLayout layout = commandBuffer->BindShader(shader, PASS_MAIN, nullptr);
	if (!layout) return;

	float2 s((float)camera->FramebufferWidth(), (float)camera->FramebufferHeight());
	float4 st(scale, screenPos);

	commandBuffer->PushConstant(shader, "Color", &color);
	commandBuffer->PushConstant(shader, "Bounds", &clipRect);
	commandBuffer->PushConstant(shader, "ScreenSize", &s);
	commandBuffer->PushConstant(shader, "ScaleTranslate", &st);
	vkCmdDraw(*commandBuffer, 6, 1, 0, 0);
}
void GUI::Rect(CommandBuffer * commandBuffer, Camera * camera, const float4x4& objectToWorld, const float2& offset, const float2& scale, const float4& color, const float4& clipRect) {
	GraphicsShader* shader = camera->Scene()->AssetManager()->LoadShader("Shaders/ui.stm")->GetGraphics(PASS_MAIN, {});
	if (!shader) return;

	VkPipelineLayout layout = commandBuffer->BindShader(shader, PASS_MAIN, nullptr, camera);
	if (!layout) return;

	float4 st(scale, offset);

	commandBuffer->PushConstant(shader, "ObjectToWorld", &objectToWorld);
	commandBuffer->PushConstant(shader, "Color", &color);
	commandBuffer->PushConstant(shader, "Bounds", &clipRect);
	commandBuffer->PushConstant(shader, "ScaleTranslate", &st);
	vkCmdDraw(*commandBuffer, 6, 1, 0, 0);
}

void GUI::Label(CommandBuffer* commandBuffer, Camera* camera, Font* font, const string& text, float textScale, const float2& screenPos, const float2& scale, const float4& color, const float4& textColor, TextAnchor horizontalAnchor, TextAnchor verticalAnchor, const float4& clipRect){
	if (screenPos.x + scale.x < clipRect.x || screenPos.x > clipRect.x + clipRect.z ||
		screenPos.y + scale.y < clipRect.y || screenPos.y > clipRect.y + clipRect.w) return;

	if (color.a > 0) Rect(commandBuffer, camera, screenPos, scale, color, clipRect);
	if (textColor.a > 0 && text.length()){
		float2 o = 0;
		if (horizontalAnchor == TEXT_ANCHOR_MID) o.x = scale.x * .5f;
		if (horizontalAnchor == TEXT_ANCHOR_MAX) o.x = scale.x;
		if (verticalAnchor == TEXT_ANCHOR_MID) o.y = scale.y * .5f;
		if (verticalAnchor == TEXT_ANCHOR_MAX) o.y = scale.y;
		DrawString(commandBuffer, camera, font, text, textColor, screenPos + o, textScale, horizontalAnchor, verticalAnchor, clipRect);
	}
}
void GUI::Label(CommandBuffer* commandBuffer, Camera* camera, Font* font, const string& text, float textScale, const float4x4& transform, const float2& offset, const float2& scale, const float4& color, const float4& textColor, TextAnchor horizontalAnchor, TextAnchor verticalAnchor, const float4& clipRect){
	if (color.a > 0) Rect(commandBuffer, camera, transform, offset, scale, color, clipRect);
	if (textColor.a > 0 && text.length()){
		float2 o = 0;
		if (horizontalAnchor == TEXT_ANCHOR_MID) o.x = scale.x * .5f;
		if (horizontalAnchor == TEXT_ANCHOR_MAX) o.x = scale.x;
		if (verticalAnchor == TEXT_ANCHOR_MID) o.y = scale.y * .5f;
		if (verticalAnchor == TEXT_ANCHOR_MAX) o.y = scale.y;
		DrawString(commandBuffer, camera, font, text, textColor, transform, offset + o, textScale, horizontalAnchor, verticalAnchor, clipRect);
	}
}

bool GUI::Button(CommandBuffer* commandBuffer, Camera* camera, Font* font, const string& text, float textScale, const float2& screenPos, const float2& scale, const float4& color, const float4& textColor, TextAnchor horizontalAnchor, TextAnchor verticalAnchor, const float4& clipRect){
	if (screenPos.x + scale.x < clipRect.x || screenPos.x > clipRect.x + clipRect.z ||
		screenPos.y + scale.y < clipRect.y || screenPos.y > clipRect.y + clipRect.w) return false;
	
	MouseKeyboardInput* i = camera->Scene()->InputManager()->GetFirst<MouseKeyboardInput>();
	float2 c = i->CursorPos();
	c.y = camera->FramebufferHeight() - c.y;
	bool hvr = c.x > screenPos.x && c.y > screenPos.y && c.x < screenPos.x + scale.x && c.y < screenPos.y + scale.y &&
		c.x > clipRect.x && c.y > clipRect.y&& c.x < clipRect.x + clipRect.z && c.y < clipRect.y + clipRect.w;
	bool clk = hvr && i->KeyDown(MOUSE_LEFT);

	if (color.a > 0) {
		float m = 1.f;
		if (hvr) m = 1.2f;
		if (clk) m = 1.5f;
		Rect(commandBuffer, camera, screenPos, scale, float4(color.rgb * m, color.a), clipRect);
	}
	if (textColor.a > 0 && text.length()){
		float2 o = 0;
		if (horizontalAnchor == TEXT_ANCHOR_MID) o.x = scale.x * .5f;
		if (horizontalAnchor == TEXT_ANCHOR_MAX) o.x = scale.x;
		if (verticalAnchor == TEXT_ANCHOR_MID) o.y = scale.y * .5f;
		if (verticalAnchor == TEXT_ANCHOR_MAX) o.y = scale.y;
		DrawString(commandBuffer, camera, font, text, textColor, screenPos + o, textScale, horizontalAnchor, verticalAnchor, clipRect);
	}
	return hvr && i->KeyDownFirst(MOUSE_LEFT);
}
bool GUI::Button(CommandBuffer* commandBuffer, Camera* camera, Font* font, const string& text, float textScale, const float4x4& transform, const float2& offset, const float2& scale, const float4& color, const float4& textColor, TextAnchor horizontalAnchor, TextAnchor verticalAnchor, const float4& clipRect){
	if (color.a > 0) Rect(commandBuffer, camera, transform, offset, scale, color, clipRect);
	if (textColor.a > 0 && text.length()){
		float2 o = 0;
		if (horizontalAnchor == TEXT_ANCHOR_MID) o.x = scale.x * .5f;
		if (horizontalAnchor == TEXT_ANCHOR_MAX) o.x = scale.x;
		if (verticalAnchor == TEXT_ANCHOR_MID) o.y = scale.y * .5f;
		if (verticalAnchor == TEXT_ANCHOR_MAX) o.y = scale.y;
		DrawString(commandBuffer, camera, font, text, textColor, transform, offset + o, textScale, horizontalAnchor, verticalAnchor, clipRect);
	}
	return false;
}

void GUI::DrawScreenLine(CommandBuffer* commandBuffer, Camera* camera, const float2* points, size_t pointCount, const float2& pos, const float2& size, const float4& color) {
	GraphicsShader* shader = camera->Scene()->AssetManager()->LoadShader("Shaders/line.stm")->GetGraphics(PASS_MAIN, { "SCREEN_SPACE" });
	if (!shader) return;
	VkPipelineLayout layout = commandBuffer->BindShader(shader, PASS_MAIN, nullptr, nullptr, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP);
	if (!layout) return;

	float4 st(size, pos);
	float4 sz(0, 0, camera->FramebufferWidth(), camera->FramebufferHeight());

	Buffer* b = commandBuffer->Device()->GetTempBuffer("Perf Graph Pts", sizeof(float2) * pointCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	memcpy(b->MappedData(), points, sizeof(float2) * pointCount);
	DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet("Perf Graph DS", shader->mDescriptorSetLayouts[PER_OBJECT]);
	ds->CreateStorageBufferDescriptor(b, 0, sizeof(float2) * pointCount, INSTANCE_BUFFER_BINDING);
	ds->FlushWrites();

	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, *ds, 0, nullptr);

	commandBuffer->PushConstant(shader, "Color", &color);
	commandBuffer->PushConstant(shader, "ScaleTranslate", &st);
	commandBuffer->PushConstant(shader, "Bounds", &sz);
	commandBuffer->PushConstant(shader, "ScreenSize", &sz.z);
	vkCmdDraw(*commandBuffer, pointCount, 1, 0, 0);
}