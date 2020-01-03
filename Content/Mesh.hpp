#pragma once

#include <assimp/mesh.h>

#include <Content/Animation.hpp>
#include <Content/Asset.hpp>
#include <Core/Buffer.hpp>
#include <Math/Geometry.hpp>
#include <Core/Instance.hpp>
#include <Util/Util.hpp>
#include <Scene/Object.hpp>

struct StdVertex {
	float3 position;
	float3 normal;
	float4 tangent;
	float2 uv;

	ENGINE_EXPORT static const ::VertexInput VertexInput;
};

class Bone : public Object {
public:
	uint32_t mBoneIndex;
	float4x4 mBindOffset;
	Bone(const std::string& name, uint32_t index) : Object(name), mBindOffset(float4x4(1)), mBoneIndex(index) {}
};

class Mesh : public Asset {
public:
	struct MaterialData {
		std::string mName;
		std::string mDiffuseTexture;
		std::string mNormalTexture;
	};

	const std::string mName;

	ENGINE_EXPORT Mesh(const std::string& name);
	ENGINE_EXPORT Mesh(const std::string& name, ::Instance* devices,
		std::shared_ptr<Buffer>* vertexBuffers, std::shared_ptr<Buffer>* indexBuffers, const AABB& bounds, uint32_t baseVertex, uint32_t vertexCount, uint32_t baseIndex, uint32_t indexCount,
		const ::VertexInput* vertexInput, VkIndexType indexType, VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	ENGINE_EXPORT Mesh(const std::string& name, ::Instance* devices,
		const void* vertices, const void* indices, uint32_t vertexCount, uint32_t vertexSize, uint32_t indexCount,
		const ::VertexInput* vertexInput, VkIndexType indexType, VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	ENGINE_EXPORT Mesh(const std::string& name, ::Device* device,
		const void* vertices, const void* indices, uint32_t vertexCount, uint32_t vertexSize, uint32_t indexCount,
		const ::VertexInput* vertexInput, VkIndexType indexType, VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	ENGINE_EXPORT ~Mesh() override;

	// Creates a cube, using float3 vertices
	ENGINE_EXPORT static Mesh* CreateCube(const std::string& name, Instance* devices, float radius = 1.f);
	// Creates a plane facing the positive z axis, using StdVertex vertices
	ENGINE_EXPORT static Mesh* CreatePlane(const std::string& name, Instance* devices, float size = 1.f);

	inline std::shared_ptr<Buffer> VertexBuffer(Device* device) const { return mDeviceData.at(device).mVertexBuffer; }
	inline std::shared_ptr<Buffer> IndexBuffer (Device* device) const { return mDeviceData.at(device).mIndexBuffer; }
	inline std::shared_ptr<Buffer> WeightBuffer(Device* device) const { return mDeviceData.at(device).mWeightBuffer; }

	inline VkPrimitiveTopology Topology() const { return mTopology; }
	inline uint32_t BaseVertex() const { return mBaseVertex; }
	inline uint32_t VertexCount() const { return mVertexCount; }
	inline uint32_t BaseIndex() const { return mBaseIndex; }
	inline uint32_t IndexCount() const { return mIndexCount; }
	inline VkIndexType IndexType() const { return mIndexType; }

	inline std::shared_ptr<AnimationRig> Rig() const { return mRig; }

	inline const ::VertexInput* VertexInput() const { return mVertexInput; }

	inline AABB Bounds() const { return mBounds; }
	inline void Bounds(const AABB& b) { mBounds = b; }

private:
	friend class AssetManager;
	ENGINE_EXPORT Mesh(const std::string& name, ::Instance* devices, const std::string& filename, float scale = 1.f);

	const ::VertexInput* mVertexInput;
	uint32_t mBaseVertex;
	uint32_t mVertexCount;
	uint32_t mBaseIndex;
	uint32_t mIndexCount;
	VkIndexType mIndexType;
	VkPrimitiveTopology mTopology;

	std::shared_ptr<AnimationRig> mRig;
	std::unordered_map<std::string, Animation*> mAnimations;

	AABB mBounds;
	struct DeviceData {
		std::shared_ptr<Buffer> mWeightBuffer;
		std::shared_ptr<Buffer> mVertexBuffer;
		std::shared_ptr<Buffer> mIndexBuffer;
	};
	std::unordered_map<Device*, DeviceData> mDeviceData;
};