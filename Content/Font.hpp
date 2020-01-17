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
	Minimum, Middle, Maximum
};

class Font : public Asset {
public:

	const std::string mName;

	ENGINE_EXPORT ~Font() override;

	inline ::Texture* Texture() const { return mTexture; };

	ENGINE_EXPORT const FontGlyph* Glyph(uint32_t c) const;
	ENGINE_EXPORT float Kerning(uint32_t from, uint32_t to) const;

	/// Draws str relative to the screen
	ENGINE_EXPORT void Draw(CommandBuffer* commandBuffer, Camera* camera, const std::string& str, const float4& color, const float2& screenPos, float scale, TextAnchor horizontalAnchor = Minimum, TextAnchor verticalAnchor = Minimum);
	ENGINE_EXPORT uint32_t GenerateGlyphs(const std::string& str, float scale, AABB* aabb, std::vector<TextGlyph>& glyph, TextAnchor horizontalAnchor = Minimum, TextAnchor verticalAnchor = Minimum) const;

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

