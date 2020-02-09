#pragma once

#include <Content/Asset.hpp>
#include <Content/Texture.hpp>
#include <Util/Util.hpp>
#include <Math/Geometry.hpp>

class Camera;

struct FontGlyph {
	char mCharacter;
	float mAdvance;
	float mVertOffset;
	float2 mSize;

	float2 mUV;
	float2 mUVSize;

	float mKerning[0xFF];
};
struct TextGlyph {
	float2 mPosition;
	float2 mSize;
	float2 mUV;
	float2 mUVSize;
};
enum TextAnchor {
	TEXT_ANCHOR_MIN, TEXT_ANCHOR_MID, TEXT_ANCHOR_MAX
};

class Font : public Asset {
public:
	const std::string mName;

	ENGINE_EXPORT ~Font() override;

	inline ::Texture* Texture() const { return mTexture; };

	ENGINE_EXPORT const FontGlyph* Glyph(uint32_t c) const;
	ENGINE_EXPORT float Kerning(uint32_t from, uint32_t to) const;

	ENGINE_EXPORT uint32_t GenerateGlyphs(const std::string& str, float scale, AABB* aabb, std::vector<TextGlyph>& glyph, TextAnchor horizontalAnchor = TEXT_ANCHOR_MIN, TextAnchor verticalAnchor = TEXT_ANCHOR_MIN) const;

	inline float PixelSize() const { return mPixelSize; };
	inline float LineSpacing() const { return mLineSpace; };
	inline float Ascender() const { return mAscender; };
	inline float Descender() const { return mDescender; };

private:
	friend class AssetManager;
	ENGINE_EXPORT Font(const std::string& name, Device* device, const std::string& filename, float pixelSize, float scale);

	float mPixelSize;
	float mAscender;
	float mDescender;
	float mLineSpace;

	FontGlyph mGlyphs[0xFF];
	::Texture* mTexture;
};

