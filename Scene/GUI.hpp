#pragma once

#include <stack>
#include <forward_list>

#include <Content/Font.hpp>
#include <Core/CommandBuffer.hpp>
#include <Scene/Camera.hpp>
#include <Util/Util.hpp>

class AssetManager;
class InputManager;

enum LayoutAxis {
	LAYOUT_HORIZONTAL = 0,
	LAYOUT_VERTICAL = 1
};

class GUI {
private:
	#pragma endregion
	struct GuiRect {
		float4x4 ObjectToWorld;
		float4 Color;
		float4 ScaleTranslate;
		fRect2D Bounds;

		float4 TextureST;
		uint32_t TextureIndex;
		float Depth;
		uint32_t pad[2];
	};
	#pragma endregion
	
	struct GuiLine {
		float4 mColor;
		float4 mScaleTranslate;
		fRect2D mBounds;
		uint32_t mIndex;
		uint32_t mCount;
		float mThickness;
		float mDepth;
	};
	struct GuiString {
		float4x4 mTransform;
		float4 mColor;
		fRect2D mBounds;
		float2 mOffset;
		Font* mFont;
		float mScale;
		float mDepth;
		TextAnchor mHorizontalAnchor;
		TextAnchor mVerticalAnchor;
		std::string mString;
	};
	struct GuiLayout {
		float4x4 mTransform;
		bool mScreenSpace;

		LayoutAxis mAxis;
		fRect2D mRect;
		fRect2D mClipRect;
		float mLayoutPosition;

		ENGINE_EXPORT fRect2D Get(float size, float padding = 2.f);
	};

	static std::vector<GuiString> mScreenStrings;
	static std::vector<GuiString> mWorldStrings;

	static std::vector<float2> mLinePoints;
	static std::vector<GuiLine> mScreenLines;

	static std::vector<Texture*> mTextureArray;
	static std::unordered_map<Texture*, uint32_t> mTextureMap;

	static std::vector<GuiRect> mScreenRects;
	static std::vector<GuiRect> mScreenTextureRects;
	static std::vector<GuiRect> mWorldRects;
	static std::vector<GuiRect> mWorldTextureRects;

	static std::unordered_map<uint32_t, std::variant<float, std::string>> mControlData;

	static uint32_t mHotControl;
	static uint32_t mLastHotControl;
	static uint32_t mNextControlId;
	static float mCurrentDepth;

	// mGlyphCache[hash] = (buffer, lifetime)
	struct BufferCache {
		std::unordered_map<std::size_t, std::pair<Buffer*, uint32_t>> mGlyphCache;
	 	std::list<std::pair<Buffer*, uint32_t>> mGlyphBufferCache;
	};
	static BufferCache* mCaches;

	static InputManager* mInputManager;

	static std::stack<GuiLayout> mLayoutStack;

	friend class Stratum;
	friend class Scene;
	ENGINE_EXPORT static void Initialize(Device* device, AssetManager* assetManager);
	ENGINE_EXPORT static void PreFrame(Scene* scene);
	ENGINE_EXPORT static void Draw(CommandBuffer* commandBuffer, PassType pass, Camera* camera);
	ENGINE_EXPORT static void Destroy(Device* device);

public:
	ENGINE_EXPORT static fRect2D BeginScreenLayout(LayoutAxis axis, const fRect2D& screenRect, const float4& backgroundColor, float insidePadding = 2.f);

	ENGINE_EXPORT static fRect2D BeginSubLayout(LayoutAxis axis, float size, const float4& backgroundColor, float insidePadding = 2.f, float padding = 2.f);
	ENGINE_EXPORT static fRect2D BeginScrollSubLayout(float size, float contentSize, const float4& backgroundColor, float insidePadding = 2.f, float padding = 2.f);
	ENGINE_EXPORT static void EndLayout();

	ENGINE_EXPORT static void LayoutSpace(float size);
	ENGINE_EXPORT static void LayoutSeparator(float thickness, const float4& color, float padding = 2.f);
	ENGINE_EXPORT static void LayoutRect(float size, const float4& color, Texture* texture = nullptr, const float4& TextureST = float4(1,1,0,0), float padding = 2.f);
	ENGINE_EXPORT static void LayoutLabel (Font* font, const std::string& text, float textHeight, float labelSize , const float4& color, const float4& textColor, float padding = 2.f, TextAnchor horizontalAnchor = TEXT_ANCHOR_MID, TextAnchor verticalAnchor = TEXT_ANCHOR_MID);
	ENGINE_EXPORT static bool LayoutButton(Font* font, const std::string& text, float textHeight, float buttonSize, const float4& color, const float4& textColor, float padding = 2.f, TextAnchor horizontalAnchor = TEXT_ANCHOR_MID, TextAnchor verticalAnchor = TEXT_ANCHOR_MID);
	ENGINE_EXPORT static bool LayoutSlider(float& value, float minimum, float maximum, float size, const float4& color, float padding = 2.f);


	/// Draws a string in the world
	ENGINE_EXPORT static void DrawString(Font* font, const std::string& str, const float4& color, const float4x4& objectToWorld, const float2& offset, float scale, TextAnchor horizontalAnchor = TEXT_ANCHOR_MIN, TextAnchor verticalAnchor = TEXT_ANCHOR_MIN, const fRect2D& clipRect = fRect2D(-1e10f, -1e10f, 1e20f, 1e20f));
	/// Draws a string on the screen, where screenPos is in pixels and (0,0) is the bottom-left of the screen
	ENGINE_EXPORT static void DrawString(Font* font, const std::string& str, const float4& color, const float2& screenPos, float scale, TextAnchor horizontalAnchor = TEXT_ANCHOR_MIN, TextAnchor verticalAnchor = TEXT_ANCHOR_MIN, const fRect2D& clipRect = fRect2D(-1e10f, -1e10f, 1e20f, 1e20f));

	/// Draw a rectangle on the screen, "size" pixels big with the bottom-left corner at screenPos
	ENGINE_EXPORT static void Rect(const fRect2D& screenRect, const float4& color, Texture* texture = nullptr, const float4& textureST = float4(1,1,0,0), const fRect2D& clipRect = fRect2D(-1e10f, -1e10f, 1e20f, 1e20f));
	/// Draw a rectangle in the world, "size" units big with the bottom-left corner at screenPos
	ENGINE_EXPORT static void Rect(const float4x4& transform, const fRect2D& rect, const float4& color, Texture* texture = nullptr, const float4& textureST = float4(1, 1, 0, 0), const fRect2D& clipRect = fRect2D(-1e10f, -1e10f, 1e20f, 1e20f));

	/// Draw a label on the screen, "size" pixels big with the bottom-left corner at screenPos
	ENGINE_EXPORT static void Label(Font* font, const std::string& text, float textScale,
		const fRect2D& screenRect, const float4& color, const float4& textColor,
		TextAnchor horizontalAnchor = TEXT_ANCHOR_MID, TextAnchor verticalAnchor = TEXT_ANCHOR_MID, const fRect2D& clipRect = fRect2D(-1e10f, -1e10f, 1e20f, 1e20f));
	/// Draw a label in the world, "size" units big with the bottom-left corner at screenPos
	ENGINE_EXPORT static void Label(Font* font, const std::string& text, float textScale,
		const float4x4& transform, const fRect2D& rect, const float4& color, const float4& textColor,
		TextAnchor horizontalAnchor = TEXT_ANCHOR_MID, TextAnchor verticalAnchor = TEXT_ANCHOR_MID, const fRect2D& clipRect = fRect2D(-1e10f, -1e10f, 1e20f, 1e20f));
	
	/// Draw a button on the screen, "size" pixels big with the bottom-left corner at screenPos
	ENGINE_EXPORT static bool Button(Font* font, const std::string& text, float textScale, const fRect2D& screenRect, const float4& color, const float4& textColor, TextAnchor horizontalAnchor = TEXT_ANCHOR_MID, TextAnchor verticalAnchor = TEXT_ANCHOR_MID, const fRect2D& clipRect = fRect2D(-1e10f, -1e10f, 1e20f, 1e20f));
	/// Draw a button in the world, "size" units big with the bottom-left corner at screenPos
	ENGINE_EXPORT static bool Button(Font* font, const std::string& text, float textScale, const float4x4& transform, const fRect2D& rect, const float4& color, const float4& textColor, TextAnchor horizontalAnchor = TEXT_ANCHOR_MID, TextAnchor verticalAnchor = TEXT_ANCHOR_MID, const fRect2D& clipRect = fRect2D(-1e10f, -1e10f, 1e20f, 1e20f));

	ENGINE_EXPORT static bool Slider(float& value, float minimum, float maximum, LayoutAxis axis, const fRect2D& screenRect, const float4& color, const fRect2D& clipRect = fRect2D(-1e10f, -1e10f, 1e20f, 1e20f));
	ENGINE_EXPORT static bool Slider(float& value, float minimum, float maximum, LayoutAxis axis, const float4x4& transform, const fRect2D& rect, const float4& color, const fRect2D& clipRect = fRect2D(-1e10f, -1e10f, 1e20f, 1e20f));


	ENGINE_EXPORT static void DrawScreenLine(const float2* points, size_t pointCount, float thickness, const float2& offset, const float2& scale, const float4& color);
};