#pragma once

#include <memory>
#include <variant>

#include <Content/Font.hpp>
#include <Content/Material.hpp>
#include <Content/Shader.hpp>
#include <Core/Buffer.hpp>
#include <Core/DescriptorSet.hpp>
#include <Scene/Renderer.hpp>
#include <Util/Util.hpp>

class TextRenderer : public Renderer {
public:
	bool mVisible;

	ENGINE_EXPORT TextRenderer(const std::string& name);
	ENGINE_EXPORT ~TextRenderer();

	inline float4 Color() const { return mColor; }
	inline void Color(const float4& c) { mColor = c; }

	inline std::string Text() const { return mText; }
	ENGINE_EXPORT void Text(const std::string& text);

	inline TextAnchor HorizontalAnchor() const { return mHorizontalAnchor; }
	inline void HorizontalAnchor(TextAnchor anchor) { mHorizontalAnchor = anchor; }
	inline TextAnchor VerticalAnchor() const { return mVerticalAnchor; }
	inline void VerticalAnchor(TextAnchor anchor) { mVerticalAnchor = anchor; }

	inline ::Font* Font() const { return mFont.index() == 0 ? std::get<::Font*>(mFont) : std::get<std::shared_ptr<::Font>>(mFont).get(); }
	inline void Font(std::shared_ptr<::Font> f) { mFont = f; }
	inline void Font(::Font* f) { mFont = f;  }

	inline float TextScale() const { return mTextScale; }
	inline void TextScale(float sc) { mTextScale = sc; }

	inline virtual bool Visible() override { return mVisible && Font() && EnabledHierarchy(); }
	inline virtual uint32_t RenderQueue() override { return mShader ? mShader->RenderQueue() : 5000; }

	ENGINE_EXPORT virtual void Draw(CommandBuffer* commandBuffer, Camera* camera, Scene::PassType pass) override;

	inline virtual AABB Bounds() override { UpdateTransform(); return mAABB; }

private:
	std::vector<TextGlyph> mTempGlyphs;
	uint32_t BuildText(Device* device, Buffer*& buffer);

	Shader* mShader;
	float4 mColor;
	TextAnchor mHorizontalAnchor;
	TextAnchor mVerticalAnchor;
	float mTextScale;
	std::string mText;
	AABB mAABB;
	AABB mTextAABB;

	std::variant<::Font*, std::shared_ptr<::Font>> mFont;

protected:
	ENGINE_EXPORT virtual bool UpdateTransform() override;
};