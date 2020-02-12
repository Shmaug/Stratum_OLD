#include <Content/Font.hpp>
#include <Content/AssetManager.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Camera.hpp>
#include <Content/Shader.hpp>
#include <Core/Buffer.hpp>

#include <ThirdParty/stb_truetype.h>

#include <cstring>

using namespace std;

Font::Font(const string& name, Device* device, const string& filename, float pixelSize, float scale)
	: mName(name), mTexture(nullptr), mPixelSize(pixelSize), mAscender(0), mDescender(0), mLineSpace(0) {

	memset(mGlyphs, 0, sizeof(FontGlyph) * 0xFF);

	string file;
	ReadFile(filename, file);

	stbtt_fontinfo font;
	stbtt_InitFont(&font, (const unsigned char*)file.data(), 0);

	float fontScale = stbtt_ScaleForPixelHeight(&font, pixelSize);

	int ascend, descend, space;
	stbtt_GetFontVMetrics(&font, &ascend, &descend, &space);

	mAscender = ascend * fontScale * scale;
	mDescender = descend * fontScale * scale;
	mLineSpace = space * fontScale * scale;

	struct GlyphBitmap {
		unsigned char* data;
		uint32_t glyph;
		VkRect2D rect;
	};
	vector<GlyphBitmap> bitmaps;

	const uint32_t PADDING = 1;

	uint32_t area = 0;
	uint32_t maxWidth = 0;

	for (uint32_t c = 0; c < 0xFF; c++) {
		FontGlyph& g = mGlyphs[c];
		g.mCharacter = c;

		int advance, lsb;
		stbtt_GetCodepointHMetrics(&font, c, &advance, &lsb);
		int ymin, ymax;
		stbtt_GetCodepointBitmapBox(&font, c, fontScale, fontScale, 0, &ymin, 0, &ymax);

		int w, h, x, y;
		unsigned char* data = stbtt_GetCodepointBitmapSubpixel(&font, fontScale, fontScale, 0, 0, c, &w, &h, &x, &y);
		
		g.mVertOffset = -ymin * scale;
		g.mSize = float2((float)w, -(float)h) * scale;
		g.mAdvance = advance * fontScale * scale;

		w += PADDING;
		h += PADDING;
		bitmaps.push_back({ data, (uint32_t)c, {{ 0, 0 }, { (uint32_t)w, (uint32_t)h } } });

		area += w * h;
		maxWidth = max(maxWidth, (uint32_t)w);

		for (uint32_t c2 = 0; c2 < 0xFF; c2++)
			g.mKerning[c2] = stbtt_GetCodepointKernAdvance(&font, c, c2) * fontScale * scale;
	}

	// Pack glyph bitmaps

	// sort the boxes for insertion by height, descending
	sort(bitmaps.begin(), bitmaps.end(), [](const GlyphBitmap& a, const GlyphBitmap& b) {
		return b.rect.extent.height < a.rect.extent.height;
	});

	// aim for a squarish resulting container,
	// slightly adjusted for sub-100% space utilization
	// start with a single empty space, unbounded at the bottom
	deque<VkRect2D> spaces {
		{ { PADDING, PADDING }, { max((uint32_t)ceilf(sqrtf(area / 0.95f)), maxWidth), 0xFFFFFFFF } }
	};

	uint2 packedSize(0);

	for (GlyphBitmap& box : bitmaps) {
		// look through spaces backwards so that we check smaller spaces first
		for (int i = (int)spaces.size() - 1; i >= 0; i--) {
			VkRect2D& space = spaces[i];

			// look for empty spaces that can accommodate the current box
			if (box.rect.extent.width > space.extent.width || box.rect.extent.height > space.extent.height) continue;

			// found the space; add the box to its top-left corner
			// |-------|-------|
			// |  box  |       |
			// |_______|       |
			// |         space |
			// |_______________|
			box.rect.offset = space.offset;
			packedSize = max(packedSize, uint2(space.offset.x + box.rect.extent.width, space.offset.y + box.rect.extent.height));

			if (box.rect.extent.width == space.extent.width && box.rect.extent.height == space.extent.height) {
				spaces.erase(spaces.begin() + i);

			} else if (box.rect.extent.height == space.extent.height) {
				// space matches the box height; update it accordingly
				// |-------|---------------|
				// |  box  | updated space |
				// |_______|_______________|
				space.offset.x += box.rect.extent.width;
				space.extent.width -= box.rect.extent.width;

			} else if (box.rect.extent.width == space.extent.width) {
				// space matches the box width; update it accordingly
				// |---------------|
				// |      box      |
				// |_______________|
				// | updated space |
				// |_______________|
				space.offset.y += box.rect.extent.height;
				space.extent.height -= box.rect.extent.height;

			} else {
				// otherwise the box splits the space into two spaces
				// |-------|-----------|
				// |  box  | new space |
				// |_______|___________|
				// | updated space     |
				// |___________________|
				spaces.push_back({
					{ (int32_t)(space.offset.x + box.rect.extent.width), space.offset.y },
					{ space.extent.width - box.rect.extent.width, box.rect.extent.height }
				});
				space.offset.y += box.rect.extent.height;
				space.extent.height -= box.rect.extent.height;
			}
			break;
		}
	}

	// round size up to power of 2
	packedSize.x--;
	packedSize.y--;
	packedSize.x |= packedSize.x >> 1;
	packedSize.y |= packedSize.y >> 1;
	packedSize.x |= packedSize.x >> 2;
	packedSize.y |= packedSize.y >> 2;
	packedSize.x |= packedSize.x >> 4;
	packedSize.y |= packedSize.y >> 4;
	packedSize.x |= packedSize.x >> 8;
	packedSize.y |= packedSize.y >> 8;
	packedSize.x |= packedSize.x >> 16;
	packedSize.y |= packedSize.y >> 16;
	packedSize.x++;
	packedSize.y++;
	
	VkDeviceSize imageSize = packedSize.x * packedSize.y * 4;
	uint8_t* pixels = new uint8_t[imageSize];
	memset(pixels, 0xFF, imageSize);

	// zero alpha channel
	for (uint32_t x = 0; x < packedSize.x; x++)
		for (uint32_t y = 0; y < packedSize.y; y++)
			pixels[4 * (x + y * packedSize.x) + 3] = 0;

	// copy glyph bitmaps
	for (GlyphBitmap& p : bitmaps) {
		p.rect.extent.width -= PADDING;
		p.rect.extent.height -= PADDING;

		mGlyphs[p.glyph].mUV = float2((float)p.rect.offset.x, (float)p.rect.offset.y) / float2(packedSize);
		mGlyphs[p.glyph].mUVSize = float2((float)p.rect.extent.width, (float)p.rect.extent.height) / float2(packedSize);

		for (uint32_t x = 0; x < p.rect.extent.width; x++)
			for (uint32_t y = 0; y < p.rect.extent.height; y++) {
				size_t pixel = 4 * (p.rect.offset.x + x + (p.rect.offset.y + y) * packedSize.x);
				pixels[pixel + 3] = p.data[x + y * p.rect.extent.width];
			}
	}

	mTexture = new ::Texture(mName + " Texture", device, pixels, imageSize, packedSize.x, packedSize.y, 1, VK_FORMAT_R8G8B8A8_UNORM, 0);

	delete[] pixels;

	for (auto& g : bitmaps)
		stbtt_FreeBitmap(g.data, font.userdata);
}
Font::~Font() {
	safe_delete(mTexture)
}

const FontGlyph* Font::Glyph(uint32_t c) const {
	return mGlyphs[c].mAdvance ? &mGlyphs[c] : nullptr;
}
float Font::Kerning(uint32_t from, uint32_t to) const {
	return mGlyphs[from].mKerning[to];
};

uint32_t Font::GenerateGlyphs(const string& str, float scale, AABB* aabb, std::vector<TextGlyph>& glyphs, TextAnchor horizontalAnchor, TextAnchor verticalAnchor) const {
	glyphs.resize(str.size());

	float2 p(0);
	uint32_t lc = 0;

	const FontGlyph* prev = nullptr;

	float lineMin = 0;
	float lineMax = 0;

	uint32_t lineStart = 0;
	uint32_t glyphCount = 0;
	float ly = (mAscender - mDescender) + mLineSpace;

	auto newLine = [&]() {
		p.x = 0;
		p.y -= ly;
		prev = nullptr;
		lc++;

		float x = 0.f;
		switch (horizontalAnchor) {
		case TEXT_ANCHOR_MIN:
			lineMin = 0;
			lineMax = 0;
			return;
		case TEXT_ANCHOR_MID:
			x = (lineMax + lineMin) * .5f;
			break;
		case TEXT_ANCHOR_MAX:
			x = lineMax;
			break;
		}
		for (uint32_t v = lineStart; v < glyphCount; v++)
			glyphs[v].mPosition.x -= x;

		lineMin = 0;
		lineMax = 0;
	};

	for (uint32_t i = 0; i < str.length(); i++) {
		if (str[i] == '\n') {
			newLine();
			lineStart = glyphCount;
			continue;
		}

		if (str[i] < 0) continue;

		const FontGlyph* glyph = Glyph(str[i]);
		if (!glyph) { prev = glyph; continue; }

		if (prev) p.x += prev->mKerning[str[i]];

		glyphs[glyphCount].mPosition = (p + float2(0, glyph->mVertOffset)) * scale;
		glyphs[glyphCount].mSize = glyph->mSize * scale;
		glyphs[glyphCount].mUV = glyph->mUV;
		glyphs[glyphCount].mUVSize = glyph->mUVSize;

		lineMin = fminf(lineMin, glyphs[glyphCount].mPosition.x);
		lineMax = fmaxf(lineMax, glyphs[glyphCount].mPosition.x + glyphs[glyphCount].mSize.x);

		glyphCount++;
		p.x += glyph->mAdvance;

		prev = glyph;
	}

	newLine();
	p.y += ly;

	float verticalOffset = 0;
	switch (verticalAnchor) {
	case TEXT_ANCHOR_MIN:
		verticalOffset = -p.y  * scale;
		break;
	case TEXT_ANCHOR_MID:
		verticalOffset = (lc * (-mDescender - (mAscender - mDescender) * .5f) + (lc - 1) * mLineSpace) * scale * .5f;
		break;
	case TEXT_ANCHOR_MAX:
		verticalOffset = 0;
		break;
	}

	for (uint32_t i = 0; i < glyphCount; i++)
		glyphs[i].mPosition.y += verticalOffset;

	if (aabb) {
		float2 mn = glyphs[0].mPosition;
		float2 mx = glyphs[0].mPosition + glyphs[0].mSize;
		for (uint32_t i = 1; i < glyphCount; i++) {
			mn = min(mn, glyphs[i].mPosition);
			mx = max(mx, glyphs[i].mPosition + glyphs[i].mSize);
		}
		*aabb = AABB(float3(mn, 0), float3(mx, 0));
	}
	return glyphCount;
}