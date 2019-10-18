#pragma once

#include <Content/Mesh.hpp>
#include <Core/CommandBuffer.hpp>

class Scene;

class Gizmos {
public:
	ENGINE_EXPORT Gizmos(Scene* scene);
	ENGINE_EXPORT ~Gizmos();

	ENGINE_EXPORT void DrawCube(CommandBuffer* commandBuffer, const float3& center, const float3& extents, const quaternion& rotation, const float4& color);
	ENGINE_EXPORT void DrawSphere(CommandBuffer* commandBuffer, const float3& center, const float radius, const float4& color);

private:
	Material* mColorMaterial;
	Mesh* mSphereMesh;
	Mesh* mCubeMesh;
};