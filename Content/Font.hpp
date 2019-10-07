#pragma once

#include <Content/Asset.hpp>
#include <Content/Texture.hpp>
#include <Util/Util.hpp>
#include <Util/Geometry.hpp>

struct FontGlyph {
	char mCharacter;
	float mAdvance;
	float mVertOffset;
	vec2 mSize;

	vec2 mUV;
	vec2 mUVSize;

	float mKerning[0xFF];
};
struct TextGlyph {
	vec2 mPosition;
	vec2 mSize;
	vec2 mUV;
	vec2 mUVSize;
};
enum TextAnchor {
	Minimum, Middle, Maximum
};

class AssetDatabase;

class Font : public Asset {
public:

	const std::string mName;

	ENGINE_EXPORT ~Font() override;

	inline ::Texture* Texture() const { return mTexture; };

	ENGINE_EXPORT const FontGlyph* Glyph(uint32_t c) const;
	ENGINE_EXPORT float Kerning(uint32_t from, uint32_t to) const;

	ENGINE_EXPORT uint32_t GenerateGlyphs(const std::string& str, float scale, AABB& aabb, std::vector<TextGlyph>& glyph, TextAnchor horizontalAnchor = Minimum, TextAnchor verticalAnchor = Minimum) const;

	inline float PixelSize() const { return mPixelSize; };
	inline float LineSpacing() const { return mLineSpace; };
	inline float Ascender() const { return mAscender; };
	inline float Descender() const { return mDescender; };

private:
	friend class AssetDatabase;
	ENGINE_EXPORT Font(const std::string& name, DeviceManager* deviceManager, const std::string& filename, float pixelSize, float scale);

	float mPixelSize;
	float mAscender;
	float mDescender;
	float mLineSpace;

	FontGlyph mGlyphs[0xFF];
	::Texture* mTexture;
};

