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

	ENGINE_EXPORT bool PositionHandle(const InputPointer* input, const quaternion& plane, float3& position);
	ENGINE_EXPORT bool RotationHandle(const InputPointer* input, const float3& center, quaternion& rotation);
	
	ENGINE_EXPORT void DrawBillboard(const float3& center, const float2& extent, const quaternion& rotation, const float4& color, Texture* texture, const float4& textureST = float4(1,1,0,0));
	
	ENGINE_EXPORT void DrawLine(const float3& p0, const float3& p1, const float4& color);
	
	ENGINE_EXPORT void DrawCube  (const float3& center, const float3& extents, const quaternion& rotation, const float4& color);
	ENGINE_EXPORT void DrawWireCube  (const float3& center, const float3& extents, const quaternion& rotation, const float4& color);
	// Draw a circle facing in the Z direction
	ENGINE_EXPORT void DrawWireCircle(const float3& center, float radius, const quaternion& rotation, const float4& color);
	ENGINE_EXPORT void DrawWireSphere(const float3& center, float radius, const float4& color);

private:
	friend class Scene;
	ENGINE_EXPORT void PreFrame(Device* device, uint32_t backBufferIndex);
	ENGINE_EXPORT void Draw(CommandBuffer* commandBuffer, uint32_t backBufferIndex, Camera* camera);

	struct DeviceData {
		Buffer* mVertices;
		Buffer* mIndices;

		uint32_t* mBufferIndex;
		std::vector<std::pair<DescriptorSet*, Buffer*>>* mInstanceBuffers;
	};
	std::unordered_map<Device*, DeviceData> mDeviceData;

	Texture* mWhiteTexture;

	std::vector<Texture*> mTextures;
	std::unordered_map<Texture*, uint32_t> mTextureMap;

	uint32_t mLineVertexCount;
	uint32_t mTriVertexCount;

	enum GizmoType{
		Billboard,
		Cube,
		Circle,
	};
	struct Gizmo {
		float4 Color;
		quaternion Rotation;
		float4 TextureST;
		float3 Position;
		uint32_t TextureIndex;
		float3 Scale;
		uint32_t Type;
	};

	std::vector<Gizmo> mTriDrawList;
	std::vector<Gizmo> mLineDrawList;

	Shader* mGizmoShader;
};