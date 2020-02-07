#pragma once

#include <Content/Font.hpp>
#include <Core/CommandBuffer.hpp>
#include <Scene/Camera.hpp>
#include <Util/Util.hpp>

class AssetManager;

class GUI {
private:
	static std::size_t mHotControl;
	friend class Stratum;
	ENGINE_EXPORT static void Initialize(Device* device, AssetManager* assetManager);
	ENGINE_EXPORT static void Destroy(Device* device);

public:
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