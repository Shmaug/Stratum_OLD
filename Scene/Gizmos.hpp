#pragma once

#include <Content/Mesh.hpp>
#include <Content/Shader.hpp>
#include <Content/Texture.hpp>
#include <Core/CommandBuffer.hpp>
#include <Core/DescriptorSet.hpp>

class Scene;

class Gizmos {
public:
	ENGINE_EXPORT Gizmos(Scene* scene);
	ENGINE_EXPORT ~Gizmos();

	ENGINE_EXPORT bool PositionHandle(CommandBuffer* commandBuffer, uint32_t backBufferIndex, const InputPointer* input, const quaternion& plane, float3& position);
	ENGINE_EXPORT bool RotationHandle(CommandBuffer* commandBuffer, uint32_t backBufferIndex, const InputPointer* input, const float3& center, quaternion& rotation);
	
	ENGINE_EXPORT void DrawBillboard(CommandBuffer* commandBuffer, uint32_t backBufferIndex, const float3& center, const float3& extents, const float4& color, Texture* texture, const float4& textureST = float4(1,1,0,0));
	
	ENGINE_EXPORT void DrawCube  (CommandBuffer* commandBuffer, uint32_t backBufferIndex, const float3& center, const float3& extents, const quaternion& rotation, const float4& color);
	ENGINE_EXPORT void DrawWireCube  (CommandBuffer* commandBuffer, uint32_t backBufferIndex, const float3& center, const float3& extents, const quaternion& rotation, const float4& color);
	// Draw a circle facing in the Z direction
	ENGINE_EXPORT void DrawWireCircle(CommandBuffer* commandBuffer, uint32_t backBufferIndex, const float3& center, float radius, const quaternion& rotation, const float4& color);
	ENGINE_EXPORT void DrawWireSphere(CommandBuffer* commandBuffer, uint32_t backBufferIndex, const float3& center, float radius, const float4& color);

private:
	std::unordered_map<Texture*, DescriptorSet*> mDescriptorSets;
	Shader* mGizmoShader;
	Mesh* mCubeMesh;
	Mesh* mWireCubeMesh;
	Mesh* mWireCircleMesh;
	Mesh* mWireSphereMesh;
};