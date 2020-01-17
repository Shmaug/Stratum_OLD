#pragma once

#include <Interface/UIElement.hpp>

#include <Content/Font.hpp>
#include <Content/Material.hpp>
#include <Core/Buffer.hpp>
#include <Core/DescriptorSet.hpp>

#include <cstring>

class UILabel : public UIElement {
public:
	ENGINE_EXPORT UILabel(const std::string& name, UICanvas* canvas);
	ENGINE_EXPORT ~UILabel();

	inline float4 Color() const { return mColor; }
	inline void Color(const float4& c) { mColor = c; }
	
	inline std::string Text() const { return mText; }
	inline void Text(const std::string& text) { mText = text; mDirty.assign(mDirty.size(), true); }

	inline TextAnchor HorizontalAnchor() const { return mHorizontalAnchor; }
	inline void HorizontalAnchor(TextAnchor anchor) { mHorizontalAnchor = anchor; mDirty.assign(mDirty.size(), true); }

	inline TextAnchor VerticalAnchor() const { return mVerticalAnchor; }
	inline void VerticalAnchor(TextAnchor anchor) { mVerticalAnchor = anchor; mDirty.assign(mDirty.size(), true); }

	inline ::Font* Font() const { return mFont.index() == 0 ? std::get<::Font*>(mFont) : std::get<std::shared_ptr<::Font>>(mFont).get(); }
	inline void Font(std::shared_ptr<::Font> f) { mFont = f; mDirty.assign(mDirty.size(), true); }
	inline void Font(::Font* f) { mFont = f; mDirty.assign(mDirty.size(), true); }

	inline float TextScale() const { return mTextScale; }
	inline void TextScale(float sc) { mTextScale = sc; mDirty.assign(mDirty.size(), true); }
	
	ENGINE_EXPORT virtual void Draw(CommandBuffer* commandBuffer, Camera* camera, PassType pass) override;

private:
	std::vector<bool> mDirty;
	uint32_t mGlyphCount;
	std::vector<Buffer*> mGlyphBuffers;

	std::vector<TextGlyph> mTempGlyphs;
	uint32_t BuildText(Device* device, Buffer*& d);

	Shader* mShader;
	float4 mColor;
	TextAnchor mHorizontalAnchor;
	TextAnchor mVerticalAnchor;
	float mTextScale;
	std::string mText;
	AABB mAABB;
	AABB mTextAABB;
	std::variant<::Font*, std::shared_ptr<::Font>> mFont;
};