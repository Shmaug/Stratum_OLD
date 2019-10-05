#pragma once

#include <memory>
#include <variant>

#include <Content/Font.hpp>
#include <Content/Material.hpp>
#include <Core/Buffer.hpp>
#include <Core/DescriptorSet.hpp>
#include <Scene/Renderer.hpp>
#include <Util/Util.hpp>

class TextRenderer : public Renderer {
public:
	bool mVisible;

	ENGINE_EXPORT TextRenderer(const std::string& name);
	ENGINE_EXPORT ~TextRenderer();

	inline std::string Text() const { return mText; }
	ENGINE_EXPORT void Text(const std::string& text);

	inline HorizontalTextAnchor HorizontalAnchor() const { return mHorizontalAnchor; }
	inline void HorizontalAnchor(HorizontalTextAnchor anchor) { mHorizontalAnchor = anchor; for (auto& d : mDeviceData) d.second.mDirty = true; }

	inline VerticalTextAnchor VerticalAnchor() const { return mVerticalAnchor; }
	inline void VerticalAnchor(VerticalTextAnchor anchor) { mVerticalAnchor = anchor; for (auto& d : mDeviceData) d.second.mDirty = true; }

	inline ::Font* Font() const { return mFont.index() == 0 ? std::get<::Font*>(mFont) : std::get<std::shared_ptr<::Font>>(mFont).get(); }
	inline void Font(std::shared_ptr<::Font> f) { mFont = f; for (auto& d : mDeviceData) d.second.mDirty = true; }
	inline void Font(::Font* f) { mFont = f; for (auto& d : mDeviceData) d.second.mDirty = true; }

	inline float TextScale() const { return mTextScale; }
	inline void TextScale(float sc) { mTextScale = sc; for (auto& d : mDeviceData) d.second.mDirty = true; }

	inline std::shared_ptr<::Material> Material() const { return mMaterial; }
	inline void Material(std::shared_ptr<::Material> m) { mMaterial = m; }

	inline virtual bool Visible() override { return mVisible && Font() && EnabledHeirarchy(); }
	inline virtual uint32_t RenderQueue() override { return mMaterial ? mMaterial->RenderQueue() : Renderer::RenderQueue(); }

	ENGINE_EXPORT virtual void Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, ::Material* materialOverride) override;

	inline virtual AABB Bounds() override { UpdateTransform(); return mAABB; }

private:
	struct DeviceData {
		Buffer** mObjectBuffers;
		DescriptorSet** mDescriptorSets;
		Buffer** mGlyphBuffers;
		uint32_t mGlyphCount;
		bool mDirty;
	};

	std::vector<TextGlyph> mTempGlyphs;
	uint32_t BuildText(Device* device, Buffer*& buffer);

	HorizontalTextAnchor mHorizontalAnchor;
	VerticalTextAnchor mVerticalAnchor;
	float mTextScale;
	std::string mText;
	AABB mAABB;
	AABB mTextAABB;
	std::shared_ptr<::Material> mMaterial;

	std::variant<::Font*, std::shared_ptr<::Font>> mFont;

	std::unordered_map<Device*, DeviceData> mDeviceData;
protected:
	ENGINE_EXPORT virtual bool UpdateTransform() override;
};