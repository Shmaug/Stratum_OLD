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

#include <cstring>

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
	inline void HorizontalAnchor(TextAnchor anchor) { mHorizontalAnchor = anchor; for (auto& d : mDeviceData) memset(d.second.mDirty, true, d.first->MaxFramesInFlight() * sizeof(bool)); }
	inline TextAnchor VerticalAnchor() const { return mVerticalAnchor; }
	inline void VerticalAnchor(TextAnchor anchor) { mVerticalAnchor = anchor; for (auto& d : mDeviceData) memset(d.second.mDirty, true, d.first->MaxFramesInFlight() * sizeof(bool)); }

	inline ::Font* Font() const { return mFont.index() == 0 ? std::get<::Font*>(mFont) : std::get<std::shared_ptr<::Font>>(mFont).get(); }
	inline void Font(std::shared_ptr<::Font> f) { mFont = f; for (auto& d : mDeviceData) memset(d.second.mDirty, true, d.first->MaxFramesInFlight() * sizeof(bool)); }
	inline void Font(::Font* f) { mFont = f; for (auto& d : mDeviceData) memset(d.second.mDirty, true, d.first->MaxFramesInFlight() * sizeof(bool)); }

	inline float TextScale() const { return mTextScale; }
	inline void TextScale(float sc) { mTextScale = sc; for (auto& d : mDeviceData) memset(d.second.mDirty, true, d.first->MaxFramesInFlight() * sizeof(bool)); }

	inline virtual bool Visible() override { return mVisible && Font() && EnabledHeirarchy(); }
	inline virtual uint32_t RenderQueue() override { return mShader ? mShader->RenderQueue() : 5000; }

	ENGINE_EXPORT virtual void Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, ::Material* materialOverride) override;

	inline virtual AABB Bounds() override { UpdateTransform(); return mAABB; }

private:
	struct DeviceData {
		Buffer** mObjectBuffers;
		DescriptorSet** mDescriptorSets;
		Buffer** mGlyphBuffers;
		bool* mDirty;
		uint32_t mGlyphCount;
		bool* mUniformDirty;
	};

	std::vector<TextGlyph> mTempGlyphs;
	uint32_t BuildText(Device* device, Buffer*& buffer);

	Shader* mShader;
	float4 mColor;
	uint32_t mRenderQueue;
	TextAnchor mHorizontalAnchor;
	TextAnchor mVerticalAnchor;
	float mTextScale;
	std::string mText;
	AABB mAABB;
	AABB mTextAABB;

	std::variant<::Font*, std::shared_ptr<::Font>> mFont;

	std::unordered_map<Device*, DeviceData> mDeviceData;
protected:
	ENGINE_EXPORT virtual bool UpdateTransform() override;
};