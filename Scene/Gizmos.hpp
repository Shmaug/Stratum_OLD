#pragma once

#include <Content/Mesh.hpp>
#include <Core/CommandBuffer.hpp>

class Scene;

class Gizmos {
public:
	ENGINE_EXPORT Gizmos(Scene* scene);
	ENGINE_EXPORT ~Gizmos();

	ENGINE_EXPORT void DrawCube  (CommandBuffer* commandBuffer, uint32_t backBufferIndex, const float3& center, const float3& extents, const quaternion& rotation, const float4& color);
	ENGINE_EXPORT void DrawWireCube  (CommandBuffer* commandBuffer, uint32_t backBufferIndex, const float3& center, const float3& extents, const quaternion& rotation, const float4& color);
	// Draw a circle facing in the Z direction
	ENGINE_EXPORT void DrawWireCircle(CommandBuffer* commandBuffer, uint32_t backBufferIndex, const float3& center, float radius, const quaternion& rotation, const float4& color);

private:
	Material* mGizmoMaterial;
	Mesh* mCubeMesh;
	Mesh* mWireCubeMesh;
	Mesh* mWireCircleMesh;
};