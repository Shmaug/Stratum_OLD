#pragma once

#include <Interface/UIElement.hpp>

#include <Content/Font.hpp>
#include <Content/Material.hpp>
#include <Core/Buffer.hpp>
#include <Core/DescriptorSet.hpp>

class TextButton : public UIElement {
public:
	ENGINE_EXPORT TextButton(const std::string& name, UICanvas* canvas);
	ENGINE_EXPORT ~TextButton();

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

	inline virtual bool Visible() override { return UIElement::Visible() && Font(); }
	inline virtual uint32_t RenderQueue() override { return mMaterial ? mMaterial->RenderQueue() : UIElement::RenderQueue(); }

	ENGINE_EXPORT virtual void Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, ::Material* materialOverride) override;

private:
	struct DeviceData {
		Buffer** mObjectBuffers;
		DescriptorSet** mDescriptorSets;
		Buffer* mGlyphBuffer;
		uint32_t mGlyphCount;
		bool mDirty;
	};

	void BuildText(Device* device, DeviceData& d);

	HorizontalTextAnchor mHorizontalAnchor;
	VerticalTextAnchor mVerticalAnchor;
	float mTextScale;
	std::string mText;
	AABB mAABB;
	AABB mTextAABB;
	float mVerticalOffset;
	std::shared_ptr<::Material> mMaterial;

	std::variant<::Font*, std::shared_ptr<::Font>> mFont;

	std::unordered_map<Device*, DeviceData> mDeviceData;
};