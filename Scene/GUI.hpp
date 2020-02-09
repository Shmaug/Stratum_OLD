#pragma once

#include <Content/Font.hpp>
#include <Core/CommandBuffer.hpp>
#include <Scene/Camera.hpp>
#include <Util/Util.hpp>

class AssetManager;

class GUI {
private:
	static std::unordered_map<std::size_t, std::pair<Buffer*, uint32_t>> mGlyphCache;
	static std::size_t mHotControl;

	friend class Stratum;
	ENGINE_EXPORT static void Initialize(Device* device, AssetManager* assetManager);
	ENGINE_EXPORT static void Destroy(Device* device);

public:
	/// Draws a string in the world
	ENGINE_EXPORT static void DrawString(CommandBuffer* commandBuffer, Camera* camera, Font* font, const std::string& str, const float4& color, const float4x4& objectToWorld, const float2& offset, float scale, TextAnchor horizontalAnchor = TEXT_ANCHOR_MIN, TextAnchor verticalAnchor = TEXT_ANCHOR_MIN, const float4& clipRect = float4(-1e10f, -1e10f, 1e20f, 1e20f));
	/// Draws a string on the screen, where screenPos is in pixels and (0,0) is the bottom-left of the screen
	ENGINE_EXPORT static void DrawString(CommandBuffer* commandBuffer, Camera* camera, Font* font, const std::string& str, const float4& color, const float2& screenPos, float scale, TextAnchor horizontalAnchor = TEXT_ANCHOR_MIN, TextAnchor verticalAnchor = TEXT_ANCHOR_MIN, const float4& clipRect = float4(-1e10f, -1e10f, 1e20f, 1e20f));

	/// Draw a rectangle on the screen, "scale" pixels big with the bottom-left corner at screenPos
	ENGINE_EXPORT static void Rect(CommandBuffer* commandBuffer, Camera* camera,
		const float2& screenPos, const float2& scale, const float4& color, const float4& clipRect = float4(-1e10f, -1e10f, 1e20f, 1e20f));
	/// Draw a rectangle in the world, "scale" units big with the bottom-left corner at screenPos
	ENGINE_EXPORT static void Rect(CommandBuffer* commandBuffer, Camera* camera,
		const float4x4& transform, const float2& offset, const float2& scale, const float4& color, const float4& clipRect = float4(-1e10f, -1e10f, 1e20f, 1e20f));

	/// Draw a label on the screen, "scale" pixels big with the bottom-left corner at screenPos
	ENGINE_EXPORT static void Label(CommandBuffer* commandBuffer, Camera* camera, 
		Font* font, const std::string& text, float textScale,
		const float2& screenPos, const float2& scale, const float4& color, const float4& textColor,
		TextAnchor horizontalAnchor = TEXT_ANCHOR_MID, TextAnchor verticalAnchor = TEXT_ANCHOR_MID, const float4& clipRect = float4(-1e10f, -1e10f, 1e20f, 1e20f));
	/// Draw a label in the world, "scale" units big with the bottom-left corner at screenPos
	ENGINE_EXPORT static void Label(CommandBuffer* commandBuffer, Camera* camera,
		Font* font, const std::string& text, float textScale,
		const float4x4& transform, const float2& offset, const float2& scale, const float4& color, const float4& textColor,
		TextAnchor horizontalAnchor = TEXT_ANCHOR_MID, TextAnchor verticalAnchor = TEXT_ANCHOR_MID, const float4& clipRect = float4(-1e10f, -1e10f, 1e20f, 1e20f));
	

	/// Draw a button on the screen, "scale" pixels big with the bottom-left corner at screenPos
	ENGINE_EXPORT static bool Button(CommandBuffer* commandBuffer, Camera* camera, Font* font, const std::string& text, float textScale, const float2& screenPos, const float2& scale, const float4& color, const float4& textColor, TextAnchor horizontalAnchor = TEXT_ANCHOR_MID, TextAnchor verticalAnchor = TEXT_ANCHOR_MID, const float4& clipRect = float4(-1e10f, -1e10f, 1e20f, 1e20f));
	/// Draw a button in the world, "scale" units big with the bottom-left corner at screenPos
	ENGINE_EXPORT static bool Button(CommandBuffer* commandBuffer, Camera* camera, Font* font, const std::string& text, float textScale, const float4x4& transform, const float2& offset, const float2& scale, const float4& color, const float4& textColor, TextAnchor horizontalAnchor = TEXT_ANCHOR_MID, TextAnchor verticalAnchor = TEXT_ANCHOR_MID, const float4& clipRect = float4(-1e10f, -1e10f, 1e20f, 1e20f));


	ENGINE_EXPORT static void DrawScreenLine(CommandBuffer* commandBuffer, Camera* camera, const float2* points, size_t pointCount, const float2& pos, const float2& size, const float4& color);
};