#pragma once

#include <Content/Mesh.hpp>
#include <Content/Shader.hpp>
#include <Content/Texture.hpp>
#include <Core/CommandBuffer.hpp>
#include <Core/DescriptorSet.hpp>
#include <Input/InputDevice.hpp>

class AssetManager;

class Gizmos {
public:
	ENGINE_EXPORT static bool PositionHandle(const std::string& controlName, const InputPointer* input, const quaternion& plane, float3& position, float radius = .1f, const float4& color = float4(1));
	ENGINE_EXPORT static bool RotationHandle(const std::string& controlName, const InputPointer* input, const float3& center, quaternion& rotation, float radius = .125f, float sensitivity = .3f);
	
	ENGINE_EXPORT static void DrawBillboard(const float3& center, const float2& extent, const quaternion& rotation, const float4& color, Texture* texture, const float4& textureST = float4(1,1,0,0));
	
	ENGINE_EXPORT static void DrawLine(const float3& p0, const float3& p1, const float4& color);
	
	ENGINE_EXPORT static void DrawCube  (const float3& center, const float3& extents, const quaternion& rotation, const float4& color);
	ENGINE_EXPORT static void DrawWireCube  (const float3& center, const float3& extents, const quaternion& rotation, const float4& color);
	// Draw a circle facing in the Z direction
	ENGINE_EXPORT static void DrawWireCircle(const float3& center, float radius, const quaternion& rotation, const float4& color);
	ENGINE_EXPORT static void DrawWireSphere(const float3& center, float radius, const float4& color);

private:
	friend class Scene;
	friend class Stratum;
	ENGINE_EXPORT static void Initialize(Device* device, AssetManager* assetManager);
	ENGINE_EXPORT static void Destroy(Device* device);
	ENGINE_EXPORT static void PreFrame(Scene* scene);
	ENGINE_EXPORT static void Draw(CommandBuffer* commandBuffer, PassType pass, Camera* camera);

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

	static Buffer* mVertices;
	static Buffer* mIndices;

	static uint32_t* mBufferIndex;
	static std::vector<std::pair<DescriptorSet*, Buffer*>>* mInstanceBuffers;

	static Texture* mWhiteTexture;

	static std::vector<Texture*> mTextures;
	static std::unordered_map<Texture*, uint32_t> mTextureMap;

	static uint32_t mLineVertexCount;
	static uint32_t mTriVertexCount;
	
	static std::vector<Gizmo> mTriDrawList;
	static std::vector<Gizmo> mLineDrawList;

	static size_t mHotControl;
};