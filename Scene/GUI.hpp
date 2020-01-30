#pragma once

#include <Core/CommandBuffer.hpp>
#include <Scene/Camera.hpp>
#include <Util/Util.hpp>

class GUI {
public:
	/// Draw a rectangle on the screen, "scale" pixels big with the bottom-left corner at screenPos
	ENGINE_EXPORT static void DrawScreenRect(CommandBuffer* commandBuffer, Camera* camera, const float2& screenPos, const float2& scale, const float4& color);
	/// Draw a rectangle in the world, "scale" units big with the bottom-left corner at screenPos
	ENGINE_EXPORT static void DrawWorldRect(CommandBuffer* commandBuffer, Camera* camera, const float4x4& objectToWorld, const float2& offset, const float2& scale, const float4& color);


	ENGINE_EXPORT static void DrawScreenLine(CommandBuffer* commandBuffer, Camera* camera, const float2* points, size_t pointCount, const float2& pos, const float2& size, const float4& color);
};