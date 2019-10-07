#pragma once

#include <Interface/UIElement.hpp>

#include <Content/Font.hpp>
#include <Content/Material.hpp>
#include <Core/Buffer.hpp>
#include <Core/DescriptorSet.hpp>

class TextButton : public UIElement {
public:
	ENGINE_EXPORT TextButton(const std::string& name);
	ENGINE_EXPORT ~TextButton();

	inline std::string Text() const { return mText; }
	ENGINE_EXPORT void Text(const std::string& text);

	inline TextAnchor HorizontalAnchor() const { return mHorizontalAnchor; }
	inline void HorizontalAnchor(TextAnchor anchor) { mHorizontalAnchor = anchor; for (auto& d : mDeviceData) memset(d.second.mDirty, true, d.first->MaxFramesInFlight() * sizeof(bool)); }

	inline TextAnchor VerticalAnchor() const { return mVerticalAnchor; }
	inline void VerticalAnchor(TextAnchor anchor) { mVerticalAnchor = anchor; for (auto& d : mDeviceData) memset(d.second.mDirty, true, d.first->MaxFramesInFlight() * sizeof(bool)); }

	inline ::Font* Font() const { return mFont.index() == 0 ? std::get<::Font*>(mFont) : std::get<std::shared_ptr<::Font>>(mFont).get(); }
	inline void Font(std::shared_ptr<::Font> f) { mFont = f; for (auto& d : mDeviceData) memset(d.second.mDirty, true, d.first->MaxFramesInFlight() * sizeof(bool)); }
	inline void Font(::Font* f) { mFont = f; for (auto& d : mDeviceData) memset(d.second.mDirty, true, d.first->MaxFramesInFlight() * sizeof(bool)); }

	inline float TextScale() const { return mTextScale; }
	inline void TextScale(float sc) { mTextScale = sc; for (auto& d : mDeviceData) memset(d.second.mDirty, true, d.first->MaxFramesInFlight() * sizeof(bool)); }

	inline std::shared_ptr<::Material> Material() const { return mMaterial; }
	inline void Material(std::shared_ptr<::Material> m) { mMaterial = m; }

	inline virtual bool Visible() override { return UIElement::Visible() && Font(); }
	inline virtual uint32_t RenderQueue() override { return mMaterial ? mMaterial->RenderQueue() : UIElement::RenderQueue(); }

	ENGINE_EXPORT virtual void Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, ::Material* materialOverride) override;

private:
	struct DeviceData {
		Buffer** mObjectBuffers;
		DescriptorSet** mDescriptorSets;
		Buffer** mGlyphBuffers;
		bool* mDirty;
		uint32_t mGlyphCount;
	};

	std::vector<TextGlyph> mTempGlyphs;
	uint32_t BuildText(Device* device, Buffer*& d);

	TextAnchor mHorizontalAnchor;
	TextAnchor mVerticalAnchor;
	float mTextScale;
	std::string mText;
	AABB mAABB;
	AABB mTextAABB;
	std::shared_ptr<::Material> mMaterial;

	std::variant<::Font*, std::shared_ptr<::Font>> mFont;

	std::unordered_map<Device*, DeviceData> mDeviceData;
};