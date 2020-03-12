#include <Scene/GUI.hpp>
#include <Scene/Scene.hpp>

using namespace std;

#define START_DEPTH  0.01f
#define DEPTH_DELTA -0.0001f;

uint32_t GUI::mHotControl = -1u;
uint32_t GUI::mLastHotControl = -1u;
uint32_t GUI::mNextControlId = 0;
float GUI::mCurrentDepth = START_DEPTH;
InputManager* GUI::mInputManager;
vector<Texture*> GUI::mTextureArray;
unordered_map<Texture*, uint32_t> GUI::mTextureMap;
vector<GUI::GuiRect> GUI::mScreenRects;
vector<GUI::GuiRect> GUI::mScreenTextureRects;
vector<GUI::GuiRect> GUI::mWorldRects;
vector<GUI::GuiRect> GUI::mWorldTextureRects;
vector<GUI::GuiLine> GUI::mScreenLines;
vector<float2> GUI::mLinePoints;
vector<GUI::GuiString> GUI::mScreenStrings;
vector<GUI::GuiString> GUI::mWorldStrings;
unordered_map<uint32_t, std::variant<float, std::string>> GUI::mControlData;
stack<GUI::GuiLayout> GUI::mLayoutStack;

GUI::BufferCache* GUI::mCaches;


void GUI::Initialize(Device* device, AssetManager* assetManager) {
	mHotControl = -1;
	mLastHotControl = -1;
	mCaches = new BufferCache[device->MaxFramesInFlight()];
}
void GUI::Destroy(Device* device){
	for (uint32_t i = 0; i < device->MaxFramesInFlight(); i++){
		for (auto& j : mCaches[i].mGlyphCache)
			safe_delete(j.second.first);
		for (auto& j : mCaches[i].mGlyphBufferCache)
			safe_delete(j.first);
	}
	safe_delete_array(mCaches);
}

void GUI::PreFrame(Scene* scene) {
	mTextureArray.clear();
	mTextureMap.clear();
	mScreenRects.clear();
	mWorldRects.clear();
	mScreenLines.clear();
	mLinePoints.clear();
	mScreenStrings.clear();
	mWorldStrings.clear();
	mCurrentDepth = START_DEPTH;
	mNextControlId = 0;

	mLastHotControl = mHotControl;
	mHotControl = -1;

	while (mLayoutStack.size()) mLayoutStack.pop();

	mInputManager = scene->InputManager();

	BufferCache& c = mCaches[scene->Instance()->Device()->FrameContextIndex()];

	for (auto it = c.mGlyphBufferCache.begin(); it != c.mGlyphBufferCache.end();) {
		if (it->second == 1) {
			safe_delete(it->first);
			it = c.mGlyphBufferCache.erase(it);
		} else {
			it->second--;
			it++;
		}
	}

	for (auto it = c.mGlyphCache.begin(); it != c.mGlyphCache.end();) {
		if (it->second.second == 1) {
			c.mGlyphBufferCache.push_back(make_pair(it->second.first, 8u));
			it = c.mGlyphCache.erase(it);
		} else {
			it->second.second--;
			it++;
		}
	}
}
void GUI::Draw(CommandBuffer* commandBuffer, PassType pass, Camera* camera) {
	BufferCache& bc = mCaches[commandBuffer->Device()->FrameContextIndex()];
	
	if (mWorldRects.size()) {
		GraphicsShader* shader = camera->Scene()->AssetManager()->LoadShader("Shaders/ui.stm")->GetGraphics(pass, {});
		if (!shader) return;
		VkPipelineLayout layout = commandBuffer->BindShader(shader, pass, nullptr);
		if (!layout) return;

		Buffer* screenRects = commandBuffer->Device()->GetTempBuffer("WorldRects", mWorldRects.size() * sizeof(GuiRect), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
		memcpy(screenRects->MappedData(), mWorldRects.data(), mWorldRects.size() * sizeof(GuiRect));

		DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet("WorldRects", shader->mDescriptorSetLayouts[PER_OBJECT]);
		ds->CreateStorageBufferDescriptor(screenRects, 0, mWorldRects.size() * sizeof(GuiRect), shader->mDescriptorBindings.at("Rects").second.binding);
		ds->FlushWrites();
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, *ds, 0, nullptr);

		vkCmdDraw(*commandBuffer, 6, (uint32_t)mWorldRects.size(), 0, 0);
	}
	if (mWorldTextureRects.size()) {
		GraphicsShader* shader = camera->Scene()->AssetManager()->LoadShader("Shaders/ui.stm")->GetGraphics(pass, { "TEXTURED" });
		if (!shader) return;
		VkPipelineLayout layout = commandBuffer->BindShader(shader, pass, nullptr);
		if (!layout) return;

		Buffer* screenRects = commandBuffer->Device()->GetTempBuffer("WorldRects", mWorldTextureRects.size() * sizeof(GuiRect), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
		memcpy(screenRects->MappedData(), mWorldTextureRects.data(), mWorldTextureRects.size() * sizeof(GuiRect));

		DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet("WorldRects", shader->mDescriptorSetLayouts[PER_OBJECT]);
		ds->CreateStorageBufferDescriptor(screenRects, 0, mWorldTextureRects.size() * sizeof(GuiRect), shader->mDescriptorBindings.at("Rects").second.binding);
		for (uint32_t i = 0; i < mTextureArray.size(); i++)
			ds->CreateSampledTextureDescriptor(mTextureArray[i], i, shader->mDescriptorBindings.at("Textures").second.binding);
		ds->FlushWrites();
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, *ds, 0, nullptr);

		vkCmdDraw(*commandBuffer, 6, (uint32_t)mWorldTextureRects.size(), 0, 0);
	}
	if (mWorldStrings.size()) {
		GraphicsShader* shader = camera->Scene()->AssetManager()->LoadShader("Shaders/font.stm")->GetGraphics(PASS_MAIN, { "SCREEN_SPACE" });
		if (!shader) return;
		VkPipelineLayout layout = commandBuffer->BindShader(shader, PASS_MAIN, nullptr);
		if (!layout) return;
		float2 s(camera->FramebufferWidth(), camera->FramebufferHeight());
		commandBuffer->PushConstant(shader, "ScreenSize", &s);

		Buffer* transforms = commandBuffer->Device()->GetTempBuffer("Transforms", sizeof(float4x4) * mWorldStrings.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		float4x4* m = (float4x4*)transforms->MappedData();
		for (const GuiString& s : mWorldStrings) {
			*m = s.mTransform;
			m++;
		}
		uint32_t idx = 0;

		for (const GuiString& s : mWorldStrings) {
			Buffer* glyphBuffer = nullptr;
			char hashstr[256];
			sprintf(hashstr, "%s%f%d%d", s.mString.c_str(), s.mScale, s.mHorizontalAnchor, s.mVerticalAnchor);
			size_t key = 0;
			hash_combine(key, s.mFont);
			hash_combine(key, string(hashstr));
			if (bc.mGlyphCache.count(key)) {
				auto& b = bc.mGlyphCache.at(key);
				b.second = 8;
				glyphBuffer = b.first;
			} else {
				vector<TextGlyph> glyphs(s.mString.length());
				uint32_t glyphCount = s.mFont->GenerateGlyphs(s.mString, s.mScale, nullptr, glyphs, s.mHorizontalAnchor, s.mVerticalAnchor);
				if (glyphCount == 0) { idx++; return; }
				
				for (auto it = bc.mGlyphBufferCache.begin(); it != bc.mGlyphBufferCache.end();) {
					if (it->first->Size() == glyphCount * sizeof(TextGlyph)) {
						glyphBuffer = it->first;
						bc.mGlyphBufferCache.erase(it);
						break;
					}
					it++;
				}
				if (!glyphBuffer)
					glyphBuffer = new Buffer("Glyph Buffer", commandBuffer->Device(), glyphCount * sizeof(TextGlyph), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
				
				glyphBuffer->Upload(glyphs.data(), glyphCount * sizeof(TextGlyph));
				
				bc.mGlyphCache.emplace(key, make_pair(glyphBuffer, 8u));
			}

			DescriptorSet* descriptorSet = commandBuffer->Device()->GetTempDescriptorSet(s.mFont->mName + " DescriptorSet", shader->mDescriptorSetLayouts[PER_OBJECT]);
			descriptorSet->CreateSampledTextureDescriptor(s.mFont->Texture(), BINDING_START + 0);
			descriptorSet->CreateStorageBufferDescriptor(transforms, 0, transforms->Size(), BINDING_START + 1);
			descriptorSet->CreateStorageBufferDescriptor(glyphBuffer, 0, glyphBuffer->Size(), BINDING_START + 2);
			descriptorSet->FlushWrites();
			vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, *descriptorSet, 0, nullptr);
			commandBuffer->PushConstant(shader, "Color", &s.mColor);
			commandBuffer->PushConstant(shader, "Offset", &s.mOffset);
			commandBuffer->PushConstant(shader, "Bounds", &s.mBounds);
			commandBuffer->PushConstant(shader, "Depth", &s.mDepth);
			vkCmdDraw(*commandBuffer, (glyphBuffer->Size() / sizeof(TextGlyph)) * 6, 1, 0, idx);

			idx++;
		}
	}

	camera->Set(commandBuffer);

	if (mScreenRects.size()) {
		GraphicsShader* shader = camera->Scene()->AssetManager()->LoadShader("Shaders/ui.stm")->GetGraphics(pass, { "SCREEN_SPACE" });
		if (!shader) return;
		VkPipelineLayout layout = commandBuffer->BindShader(shader, pass, nullptr);
		if (!layout) return;

		Buffer* screenRects = commandBuffer->Device()->GetTempBuffer("ScreenRects", mScreenRects.size() * sizeof(GuiRect), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
		memcpy(screenRects->MappedData(), mScreenRects.data(), mScreenRects.size() * sizeof(GuiRect));

		DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet("ScreenRects", shader->mDescriptorSetLayouts[PER_OBJECT]);
		ds->CreateStorageBufferDescriptor(screenRects, 0, mScreenRects.size() * sizeof(GuiRect), shader->mDescriptorBindings.at("Rects").second.binding);
		ds->FlushWrites();
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, *ds, 0, nullptr);

		float2 s(camera->FramebufferWidth(), camera->FramebufferHeight());
		commandBuffer->PushConstant(shader, "ScreenSize", &s);

		vkCmdDraw(*commandBuffer, 6, (uint32_t)mScreenRects.size(), 0, 0);
	}
	if (mScreenTextureRects.size()) {
		GraphicsShader* shader = camera->Scene()->AssetManager()->LoadShader("Shaders/ui.stm")->GetGraphics(pass, { "TEXTURED" });
		if (!shader) return;
		VkPipelineLayout layout = commandBuffer->BindShader(shader, pass, nullptr);
		if (!layout) return;

		Buffer* screenRects = commandBuffer->Device()->GetTempBuffer("WorldRects", mScreenTextureRects.size() * sizeof(GuiRect), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
		memcpy(screenRects->MappedData(), mScreenTextureRects.data(), mScreenTextureRects.size() * sizeof(GuiRect));

		DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet("WorldRects", shader->mDescriptorSetLayouts[PER_OBJECT]);
		ds->CreateStorageBufferDescriptor(screenRects, 0, mScreenTextureRects.size() * sizeof(GuiRect), shader->mDescriptorBindings.at("Rects").second.binding);
		for (uint32_t i = 0; i < mTextureArray.size(); i++)
			ds->CreateSampledTextureDescriptor(mTextureArray[i], i, shader->mDescriptorBindings.at("Textures").second.binding);
		ds->FlushWrites();
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, *ds, 0, nullptr);

		vkCmdDraw(*commandBuffer, 6, (uint32_t)mScreenTextureRects.size(), 0, 0);
	}
	if (mScreenStrings.size()) {
		GraphicsShader* shader = camera->Scene()->AssetManager()->LoadShader("Shaders/font.stm")->GetGraphics(PASS_MAIN, { "SCREEN_SPACE" });
		if (!shader) return;
		VkPipelineLayout layout = commandBuffer->BindShader(shader, PASS_MAIN, nullptr);
		if (!layout) return;
		float2 s(camera->FramebufferWidth(), camera->FramebufferHeight());
		commandBuffer->PushConstant(shader, "ScreenSize", &s);

		for (const GuiString& s : mScreenStrings) {
			Buffer* glyphBuffer = nullptr;
			char hashstr[256];
			sprintf(hashstr, "%s%f%d%d", s.mString.c_str(), s.mScale, s.mHorizontalAnchor, s.mVerticalAnchor);
			size_t key = 0;
			hash_combine(key, s.mFont);
			hash_combine(key, string(hashstr));
			if (bc.mGlyphCache.count(key)) {
				auto& b = bc.mGlyphCache.at(key);
				b.second = 8u;
				glyphBuffer = b.first;
			} else {
				vector<TextGlyph> glyphs(s.mString.length());
				uint32_t glyphCount = s.mFont->GenerateGlyphs(s.mString, s.mScale, nullptr, glyphs, s.mHorizontalAnchor, s.mVerticalAnchor);
				if (glyphCount == 0) return;

				for (auto it = bc.mGlyphBufferCache.begin(); it != bc.mGlyphBufferCache.end();) {
					if (it->first->Size() == glyphCount * sizeof(TextGlyph)) {
						glyphBuffer = it->first;
						bc.mGlyphBufferCache.erase(it);
						break;
					}
					it++;
				}
				if (!glyphBuffer)
					glyphBuffer = new Buffer("Glyph Buffer", commandBuffer->Device(), glyphCount * sizeof(TextGlyph), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
				
				glyphBuffer->Upload(glyphs.data(), glyphCount * sizeof(TextGlyph));
				bc.mGlyphCache.emplace(key, make_pair(glyphBuffer, 8u));
			}

			DescriptorSet* descriptorSet = commandBuffer->Device()->GetTempDescriptorSet(s.mFont->mName + " DescriptorSet", shader->mDescriptorSetLayouts[PER_OBJECT]);
			descriptorSet->CreateSampledTextureDescriptor(s.mFont->Texture(), BINDING_START + 0);
			descriptorSet->CreateStorageBufferDescriptor(glyphBuffer, 0, glyphBuffer->Size(), BINDING_START + 2);
			descriptorSet->FlushWrites();
			vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, *descriptorSet, 0, nullptr);

			commandBuffer->PushConstant(shader, "Color", &s.mColor);
			commandBuffer->PushConstant(shader, "Offset", &s.mOffset);
			commandBuffer->PushConstant(shader, "Bounds", &s.mBounds);
			commandBuffer->PushConstant(shader, "Depth", &s.mDepth);
			vkCmdDraw(*commandBuffer, (glyphBuffer->Size() / sizeof(TextGlyph)) * 6, 1, 0, 0);
		}
	}

	if (mScreenLines.size()) {
		GraphicsShader* shader = camera->Scene()->AssetManager()->LoadShader("Shaders/line.stm")->GetGraphics(PASS_MAIN, { "SCREEN_SPACE" });
		if (!shader) return;
		VkPipelineLayout layout = commandBuffer->BindShader(shader, pass, nullptr, nullptr, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP);
		if (!layout) return;

		Buffer* b = commandBuffer->Device()->GetTempBuffer("Perf Graph Pts", sizeof(float2) * mLinePoints.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
		memcpy(b->MappedData(), mLinePoints.data(), sizeof(float2) * mLinePoints.size());
		
		DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet("Perf Graph DS", shader->mDescriptorSetLayouts[PER_OBJECT]);
		ds->CreateStorageBufferDescriptor(b, 0, sizeof(float2) * mLinePoints.size(), INSTANCE_BUFFER_BINDING);
		ds->FlushWrites();

		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, *ds, 0, nullptr);

		float4 sz(0, 0, camera->FramebufferWidth(), camera->FramebufferHeight());
		commandBuffer->PushConstant(shader, "ScreenSize", &sz.z);

		for (const GuiLine& l : mScreenLines) {
			vkCmdSetLineWidth(*commandBuffer, l.mThickness);
			commandBuffer->PushConstant(shader, "Color", &l.mColor);
			commandBuffer->PushConstant(shader, "ScaleTranslate", &l.mScaleTranslate);
			commandBuffer->PushConstant(shader, "Bounds", &l.mBounds);
			commandBuffer->PushConstant(shader, "Depth", &l.mDepth);
			vkCmdDraw(*commandBuffer, l.mCount, 1, l.mIndex, 0);
		}
	}

	mTextureArray.clear();
	mTextureMap.clear();
}

void GUI::DrawScreenLine(const float2* points, size_t pointCount, float thickness, const float2& offset, const float2& scale, const float4& color) {
	GuiLine l;
	l.mColor = color;
	l.mScaleTranslate = float4(scale, offset);
	l.mBounds = fRect2D(0, 0, 1e10f, 1e10f);
	l.mCount = pointCount;
	l.mIndex = mLinePoints.size();
	l.mThickness = thickness;
	l.mDepth = mCurrentDepth;
	mScreenLines.push_back(l);

	mLinePoints.resize(mLinePoints.size() + pointCount);
	memcpy(mLinePoints.data() + l.mIndex, points, pointCount * sizeof(float2));

	mCurrentDepth += DEPTH_DELTA;
}

void GUI::DrawString(Font* font, const string& str, const float4& color, const float2& screenPos, float scale, TextAnchor horizontalAnchor, TextAnchor verticalAnchor, const fRect2D& clipRect) {
	if (str.length() == 0) return;
	GuiString s;
	s.mFont = font;
	s.mString = str;
	s.mColor = color;
	s.mOffset = screenPos;
	s.mScale = scale;
	s.mVerticalAnchor = verticalAnchor;
	s.mHorizontalAnchor = horizontalAnchor;
	s.mBounds = clipRect;
	s.mDepth = mCurrentDepth;
	mScreenStrings.push_back(s);

	mCurrentDepth += DEPTH_DELTA;
}
void GUI::DrawString(Font* font, const string& str, const float4& color, const float4x4& objectToWorld, const float2& offset, float scale, TextAnchor horizontalAnchor, TextAnchor verticalAnchor, const fRect2D& clipRect) {
	if (str.length() == 0) return;
	if (str.length() == 0) return;
	GuiString s;
	s.mTransform = objectToWorld;
	s.mFont = font;
	s.mString = str;
	s.mColor = color;
	s.mOffset = offset;
	s.mScale = scale;
	s.mVerticalAnchor = verticalAnchor;
	s.mHorizontalAnchor = horizontalAnchor;
	s.mBounds = clipRect;
	mScreenStrings.push_back(s);
}

void GUI::Rect(const fRect2D& screenRect, const float4& color, Texture* texture, const float4& textureST, const fRect2D& clipRect) {
	if (!clipRect.Intersects(screenRect)) return;

	MouseKeyboardInput* i = mInputManager->GetFirst<MouseKeyboardInput>();
	float2 c = i->CursorPos();
	c.y = i->WindowHeight() - c.y;
	if (screenRect.Contains(c) && clipRect.Contains(c)) i->mMousePointer.mGuiHitT = 0.f;

	GuiRect r = {};
	r.ScaleTranslate = float4(screenRect.mExtent, screenRect.mOffset);
	r.Color = color;
	r.Bounds = clipRect;
	r.Depth = mCurrentDepth;
	r.TextureST = textureST;

	if (texture) {
		if (mTextureMap.count(texture))
			r.TextureIndex = mTextureMap.at(texture);
		else {
			r.TextureIndex = mTextureArray.size();
			mTextureMap.emplace(texture, mTextureArray.size());
			mTextureArray.push_back(texture);
		}
		mScreenTextureRects.push_back(r);
	} else
		mScreenRects.push_back(r);

	mCurrentDepth += DEPTH_DELTA;
}
void GUI::Rect(const float4x4& transform, const fRect2D& rect, const float4& color, Texture* texture, const float4& textureST, const fRect2D& clipRect) {
	if (!clipRect.Intersects(rect)) return;
	GuiRect r = {};
	r.ObjectToWorld = transform;
	r.ScaleTranslate = float4(rect.mExtent, rect.mOffset);
	r.Color = color;
	r.Bounds = clipRect;
	r.TextureST = textureST;

	if (texture) {
		if (mTextureMap.count(texture))
			r.TextureIndex = mTextureMap.at(texture);
		else {
			r.TextureIndex = mTextureArray.size();
			mTextureMap.emplace(texture, mTextureArray.size());
			mTextureArray.push_back(texture);
		}
		mWorldTextureRects.push_back(r);
	} else
		mWorldRects.push_back(r);
}

void GUI::Label(Font* font, const string& text, float textScale, const fRect2D& screenRect, const float4& color, const float4& textColor, TextAnchor horizontalAnchor, TextAnchor verticalAnchor, const fRect2D& clipRect){
	if (!clipRect.Intersects(screenRect)) return;
	if (color.a > 0) Rect(screenRect, color, nullptr, 0, clipRect);
	if (textColor.a > 0 && text.length()){
		float2 o = 0;
		if (horizontalAnchor == TEXT_ANCHOR_MID) o.x = screenRect.mExtent.x * .5f;
		if (horizontalAnchor == TEXT_ANCHOR_MAX) o.x = screenRect.mExtent.x;
		if (verticalAnchor == TEXT_ANCHOR_MID) o.y = screenRect.mExtent.y * .5f;
		if (verticalAnchor == TEXT_ANCHOR_MAX) o.y = screenRect.mExtent.y;
		DrawString(font, text, textColor, screenRect.mOffset + o, textScale, horizontalAnchor, verticalAnchor, clipRect);
	}
}
void GUI::Label(Font* font, const string& text, float textScale, const float4x4& transform, const fRect2D& rect, const float4& color, const float4& textColor, TextAnchor horizontalAnchor, TextAnchor verticalAnchor, const fRect2D& clipRect){
	if (!clipRect.Intersects(rect)) return;
	if (color.a > 0) Rect(transform,rect, color, nullptr, 0, clipRect);
	if (textColor.a > 0 && text.length()){
		float2 o = 0;
		if (horizontalAnchor == TEXT_ANCHOR_MID) o.x = rect.mExtent.x * .5f;
		if (horizontalAnchor == TEXT_ANCHOR_MAX) o.x = rect.mExtent.x;
		if (verticalAnchor == TEXT_ANCHOR_MID) o.y = rect.mExtent.y * .5f;
		if (verticalAnchor == TEXT_ANCHOR_MAX) o.y = rect.mExtent.y;
		DrawString(font, text, textColor, transform, rect.mOffset + o, textScale, horizontalAnchor, verticalAnchor, clipRect);
	}
}

bool GUI::Button(Font* font, const string& text, float textScale, const fRect2D& screenRect, const float4& color, const float4& textColor, TextAnchor horizontalAnchor, TextAnchor verticalAnchor, const fRect2D& clipRect){
	uint32_t controlId = mNextControlId++;
	if (!clipRect.Intersects(screenRect)) return false;

	MouseKeyboardInput* i = mInputManager->GetFirst<MouseKeyboardInput>();
	float2 c = i->CursorPos();
	c.y = i->WindowHeight() - c.y;

	bool hvr = screenRect.Contains(c) && clipRect.Contains(c);
	bool clk = i->KeyDown(MOUSE_LEFT) && (hvr || mLastHotControl == controlId);

	if (hvr || clk) i->mMousePointer.mGuiHitT = 0.f;
	if (clk) mHotControl = controlId;
	
	if (color.a > 0) {
		float m = 1.f;
		if (hvr) m = 1.2f;
		if (clk) m = 1.5f;
		Rect(screenRect, float4(color.rgb * m, color.a), nullptr, 0, clipRect);
	}
	if (textColor.a > 0 && text.length()){
		float2 o = 0;
		if (horizontalAnchor == TEXT_ANCHOR_MID) o.x = screenRect.mExtent.x * .5f;
		if (horizontalAnchor == TEXT_ANCHOR_MAX) o.x = screenRect.mExtent.x;
		if (verticalAnchor == TEXT_ANCHOR_MID) o.y = screenRect.mExtent.y * .5f;
		if (verticalAnchor == TEXT_ANCHOR_MAX) o.y = screenRect.mExtent.y;
		DrawString(font, text, textColor, screenRect.mOffset + o, textScale, horizontalAnchor, verticalAnchor, clipRect);
	}
	return hvr && i->KeyDownFirst(MOUSE_LEFT);
}
bool GUI::Button(Font* font, const string& text, float textScale, const float4x4& transform, const fRect2D& rect, const float4& color, const float4& textColor, TextAnchor horizontalAnchor, TextAnchor verticalAnchor, const fRect2D& clipRect){
	uint32_t controlId = mNextControlId++;
	if (!clipRect.Intersects(rect)) return false;

	// TODO: world-space input

	if (color.a > 0) Rect(transform, rect, color, nullptr, 0, clipRect);
	if (textColor.a > 0 && text.length()){
		float2 o = 0;
		if (horizontalAnchor == TEXT_ANCHOR_MID) o.x = rect.mExtent.x * .5f;
		if (horizontalAnchor == TEXT_ANCHOR_MAX) o.x = rect.mExtent.x;
		if (verticalAnchor == TEXT_ANCHOR_MID) o.y = rect.mExtent.y * .5f;
		if (verticalAnchor == TEXT_ANCHOR_MAX) o.y = rect.mExtent.y;
		DrawString(font, text, textColor, transform, rect.mOffset + o, textScale, horizontalAnchor, verticalAnchor, clipRect);
	}
	return false;
}


bool GUI::Slider(float& value, float minimum, float maximum, LayoutAxis axis, const fRect2D& screenRect, const float4& color, const fRect2D& clipRect) {
	uint32_t controlId = mNextControlId++;
	if (!clipRect.Intersects(screenRect)) return false;

	MouseKeyboardInput* i = mInputManager->GetFirst<MouseKeyboardInput>();
	float2 c = i->CursorPos();
	c.y = i->WindowHeight() - c.y;

	fRect2D bgRect = screenRect;
	fRect2D barRect = screenRect;

	bool ret = false;
	float vo;

	switch (axis) {
	case LAYOUT_HORIZONTAL:
		bgRect.mOffset.y += screenRect.mExtent.y * .25f;
		bgRect.mExtent.y *= 0.5f;

		barRect.mExtent.x = screenRect.mExtent.x * .1f;
		vo = barRect.mOffset.x + ((value - minimum) / (maximum - minimum)) * (screenRect.mExtent.x - barRect.mExtent.x);
		if (mLastHotControl == controlId) {
			vo += i->CursorDelta().x;
			value = ((vo - screenRect.mOffset.x) / (screenRect.mExtent.x - barRect.mExtent.x)) * (maximum - minimum) + minimum;
			value = clamp(value, minimum, maximum);
			ret = true;
		}

		vo = barRect.mOffset.x + ((value - minimum) / (maximum - minimum)) * (screenRect.mExtent.x - barRect.mExtent.x);
		barRect.mOffset.x = vo;
		break;
	case LAYOUT_VERTICAL:
		bgRect.mOffset.x += screenRect.mExtent.y * .25f;
		bgRect.mExtent.x *= 0.5f;

		barRect.mExtent.y = screenRect.mExtent.y * .1f;
		vo = barRect.mOffset.x + ((value - minimum) / (maximum - minimum)) * (screenRect.mExtent.x - barRect.mExtent.x);
		if (mHotControl == controlId) {
			vo += i->CursorDelta().x;
			value = (vo - barRect.mOffset.y) / (screenRect.mExtent.y - barRect.mExtent.y) * (maximum - minimum) - 1.f + minimum;
			value = clamp(value, minimum, maximum);
			ret = true;
		}

		vo = barRect.mOffset.x + ((value - minimum) / (maximum - minimum)) * (screenRect.mExtent.x - barRect.mExtent.x);
		barRect.mOffset.y = vo;
		break;
	}

	bool hvr = barRect.Contains(c) && clipRect.Contains(c);
	bool clk = i->KeyDown(MOUSE_LEFT) && (hvr || mLastHotControl == controlId);

	if (hvr || clk) i->mMousePointer.mGuiHitT = 0.f;
	if (clk) mHotControl = controlId;

	float m = 1.25f;
	if (hvr) m *= 1.2f;
	if (clk) m *= 1.5f;

	Rect(bgRect, color, nullptr, 0, clipRect);
	Rect(barRect, float4(color.rgb * m, color.a), nullptr, 0, clipRect);

	return ret;
}
bool GUI::Slider(float& value, float minimum, float maximum, LayoutAxis axis, const float4x4& transform, const fRect2D& rect, const float4& color, const fRect2D& clipRect) {
	uint32_t controlId = mNextControlId++;
	if (!clipRect.Intersects(rect)) return false;

	Rect(transform, rect, color, nullptr, 0, clipRect);


	return false;
}


fRect2D GUI::GuiLayout::Get(float size, float padding) {
	fRect2D layoutRect = mRect;
	switch (mAxis) {
	case LAYOUT_VERTICAL:
		layoutRect.mExtent.y = size;
		layoutRect.mOffset.y += mRect.mExtent.y - (mLayoutPosition + padding + size);
		break;
	case LAYOUT_HORIZONTAL:
		layoutRect.mOffset.x += mLayoutPosition + padding;
		layoutRect.mExtent.x = size;
		break;
	}
	mLayoutPosition += size + padding * 2;
	return layoutRect;
}

fRect2D GUI::BeginScreenLayout(LayoutAxis axis, const fRect2D& screenRect, const float4& backgroundColor, float insidePadding) {
	fRect2D layoutRect(screenRect.mOffset + insidePadding, screenRect.mExtent - insidePadding * 2);
	mLayoutStack.push({ float4x4(1), true, axis, layoutRect, layoutRect, 0 });
	if (backgroundColor.a > 0) Rect(screenRect, backgroundColor);
	return layoutRect;
}
fRect2D GUI::BeginSubLayout(LayoutAxis axis, float size, const float4& backgroundColor, float insidePadding, float padding) {
	GuiLayout& l = mLayoutStack.top();
	fRect2D layoutRect = l.Get(size, padding);

	if (backgroundColor.a > 0) {
		if (l.mScreenSpace)
			Rect(layoutRect, backgroundColor, nullptr, 0, l.mClipRect);
		else
			Rect(l.mTransform, layoutRect, backgroundColor, nullptr, 0, l.mClipRect);
	}

	layoutRect.mOffset += insidePadding;
	layoutRect.mExtent -= insidePadding * 2;

	mLayoutStack.push({ l.mTransform, l.mScreenSpace, axis, layoutRect, l.mClipRect, 0 });

	return layoutRect;
}
fRect2D GUI::BeginScrollSubLayout(float size, float contentSize, const float4& backgroundColor, float insidePadding, float padding) {
	GuiLayout& l = mLayoutStack.top();
	fRect2D layoutRect = l.Get(size, padding);
	uint32_t controlId = mNextControlId++;

	if (backgroundColor.a > 0) {
		if (l.mScreenSpace)
			Rect(layoutRect, backgroundColor, nullptr, 0, l.mClipRect);
		else
			Rect(l.mTransform, layoutRect, backgroundColor, nullptr, 0, l.mClipRect);
	}
	
	float scrollAmount = 0;
	if (mControlData.count(controlId)) {
		const auto& v = mControlData.at(controlId);
		if (v.index() == 0) scrollAmount = get<float>(v);
	}

	if (l.mScreenSpace) {
		MouseKeyboardInput* i = mInputManager->GetFirst<MouseKeyboardInput>();
		float2 c = i->CursorPos();
		c.y = i->WindowHeight() - c.y;
		if (layoutRect.Contains(c)) {
			scrollAmount -= i->ScrollDelta() * 60;
			i->mMousePointer.mGuiHitT = 0.f;
		}
	} else {
		// TODO: world-space input
	}

	float scrollMax = max(0.f, contentSize - layoutRect.mExtent.y);
	scrollAmount = clamp(scrollAmount, 0.f, scrollMax);

	mControlData[controlId] = scrollAmount;

	fRect2D contentRect = layoutRect;
	contentRect.mOffset += insidePadding;
	contentRect.mExtent -= insidePadding * 2;
	switch (l.mAxis) {
	case LAYOUT_HORIZONTAL:
		contentRect.mOffset.x -= scrollAmount + (layoutRect.mExtent.x - contentSize);
		contentRect.mExtent.x = contentSize - insidePadding * 2;
		break;
	case LAYOUT_VERTICAL:
		contentRect.mOffset.y += (layoutRect.mExtent.y - contentSize) + scrollAmount;
		contentRect.mExtent.y = contentSize - insidePadding * 2;
		break;
	}
	
	// scroll bar slider
	if (scrollMax > 0) {
		fRect2D slider;
		fRect2D sliderbg;

		switch (l.mAxis) {
		case LAYOUT_HORIZONTAL:
			slider.mExtent = float2(20 * layoutRect.mExtent.x * (layoutRect.mExtent.x / contentSize), 6);
			slider.mOffset = layoutRect.mOffset + float2((layoutRect.mExtent.x - slider.mExtent.x) * (scrollAmount / scrollMax), 0);
			sliderbg.mOffset = layoutRect.mOffset;
			sliderbg.mExtent = float2(layoutRect.mExtent.x, slider.mExtent.y);

			layoutRect.mOffset.y += slider.mExtent.y;
			layoutRect.mExtent.y -= slider.mExtent.y;
			layoutRect.mOffset.y += slider.mExtent.y;
			layoutRect.mExtent.y -= slider.mExtent.y;
			break;

		case LAYOUT_VERTICAL:
			slider.mExtent = float2(6, layoutRect.mExtent.y * (layoutRect.mExtent.y / contentSize));
			slider.mOffset = layoutRect.mOffset + float2(layoutRect.mExtent.x - slider.mExtent.x, (layoutRect.mExtent.y - slider.mExtent.y) * (1 - scrollAmount / scrollMax));
			sliderbg.mOffset = layoutRect.mOffset + float2(layoutRect.mExtent.x - slider.mExtent.x, 0);
			sliderbg.mExtent = float2(slider.mExtent.x, layoutRect.mExtent.y);

			layoutRect.mExtent.x -= slider.mExtent.x;
			break;
		}

		if (l.mScreenSpace) {
			GUI::Rect(sliderbg, float4(.4f, .4f, .4f, 1));
			GUI::Rect(slider, float4(.8f, .8f, .8f, 1));
		} else {
			GUI::Rect(l.mTransform, sliderbg, float4(.4f, .4f, .4f, 1));
			GUI::Rect(l.mTransform, slider, float4(.8f, .8f, .8f, 1));
		}
	}

	mLayoutStack.push({ l.mTransform, l.mScreenSpace, l.mAxis, contentRect, layoutRect, 0 });

	return contentRect;
}
void GUI::EndLayout() {
	mLayoutStack.pop();
}

void GUI::LayoutSpace(float size) {
	mLayoutStack.top().mLayoutPosition += size;
}
void GUI::LayoutSeparator(float thickness, const float4& color, float padding) {
	GuiLayout& l = mLayoutStack.top();
	if (l.mScreenSpace)
		GUI::Rect(l.Get(thickness, padding), color, nullptr, 0, l.mClipRect);
	else
		GUI::Rect(l.mTransform, l.Get(thickness, padding), color, nullptr, 0, l.mClipRect);
}
void GUI::LayoutRect(float size, const float4& color, Texture* texture, const float4& textureST, float padding) {
	GuiLayout& l = mLayoutStack.top();
	fRect2D layoutRect = l.Get(size, padding);

	if (l.mScreenSpace)
		Rect(layoutRect, color, texture, textureST, l.mClipRect);
	else
		Rect(l.mTransform, layoutRect, color, texture, textureST, l.mClipRect);
}
void GUI::LayoutLabel(Font* font, const string& text, float textHeight, float labelSize, const float4& color, const float4& textColor, float padding, TextAnchor horizontalAnchor, TextAnchor verticalAnchor) {
	GuiLayout& l = mLayoutStack.top();
	fRect2D layoutRect = l.Get(labelSize, padding);

	if (l.mScreenSpace)
		Label(font, text, textHeight, layoutRect, color, textColor, horizontalAnchor, verticalAnchor, l.mClipRect);
	else
		Label(font, text, textHeight, l.mTransform, layoutRect, color, textColor, horizontalAnchor, verticalAnchor, l.mClipRect);
}
bool GUI::LayoutButton(Font* font, const string& text, float textHeight, float buttonSize, const float4& color, const float4& textColor, float padding, TextAnchor horizontalAnchor, TextAnchor verticalAnchor) {
	GuiLayout& l = mLayoutStack.top();
	fRect2D layoutRect = l.Get(buttonSize, padding);

	if (l.mScreenSpace)
		return Button(font, text, textHeight, layoutRect, color, textColor, horizontalAnchor, verticalAnchor, l.mClipRect);
	else
		return Button(font, text, textHeight, l.mTransform, layoutRect, color, textColor, horizontalAnchor, verticalAnchor, l.mClipRect);
}
bool GUI::LayoutSlider(float& value, float minimum, float maximum, float size, const float4& color, float padding) {
	GuiLayout& l = mLayoutStack.top();
	fRect2D layoutRect = l.Get(size, padding);
	LayoutAxis a = LAYOUT_HORIZONTAL;
	switch (l.mAxis) {
	case LAYOUT_HORIZONTAL:
		a = LAYOUT_VERTICAL;
		break;
	case LAYOUT_VERTICAL:
		a = LAYOUT_HORIZONTAL;
		break;
	}
	if (l.mScreenSpace)
		return Slider(value, minimum, maximum, a, layoutRect, color, l.mClipRect);
	else
		return Slider(value, minimum, maximum, a, l.mTransform, layoutRect, color, l.mClipRect);
}