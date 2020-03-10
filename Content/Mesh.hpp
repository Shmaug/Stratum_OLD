#pragma once

#include <assimp/mesh.h>

#include <Content/Animation.hpp>
#include <Content/Asset.hpp>
#include <Core/Buffer.hpp>
#include <Math/Geometry.hpp>
#include <Core/Instance.hpp>
#include <Util/Util.hpp>
#include <Scene/Object.hpp>
#include <Scene/TriangleBvh2.hpp>

#include <Shaders/include/shadercompat.h>

#pragma pack(push)
#pragma pack(1)
struct StdVertex {
	float3 position;
	float3 normal;
	float4 tangent;
	float2 uv;

	ENGINE_EXPORT static const ::VertexInput VertexInput;
};
#pragma pack(pop)

class Bone : public virtual Object {
public:
	uint32_t mBoneIndex;
	float4x4 mInverseBind;
	inline Bone(const std::string& name, uint32_t index) : Object(name), mInverseBind(float4x4(1)), mBoneIndex(index) {}
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
	// Construct from existing vertex/index buffer
	ENGINE_EXPORT Mesh(const std::string& name, ::Device* device, const AABB& bounds, TriangleBvh2* bvh,
		std::shared_ptr<Buffer> vertexBuffer, std::shared_ptr<Buffer> indexBuffer, uint32_t baseVertex, uint32_t vertexCount, uint32_t baseIndex, uint32_t indexCount,
		const ::VertexInput* vertexInput, VkIndexType indexType, VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	// Construct from existing vertex/index/weight buffer
	ENGINE_EXPORT Mesh(const std::string& name, ::Device* device, const AABB& bounds, TriangleBvh2* bvh,
		std::shared_ptr<Buffer> vertexBuffer, std::shared_ptr<Buffer> indexBuffer, std::shared_ptr<Buffer> weightBuffer,
		uint32_t baseVertex, uint32_t vertexCount, uint32_t baseIndex, uint32_t indexCount,
		const ::VertexInput* vertexInput, VkIndexType indexType, VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	// Construct from vertices/indices
	ENGINE_EXPORT Mesh(const std::string& name, ::Device* device,
		const void* vertices, const void* indices, uint32_t vertexCount, uint32_t vertexSize, uint32_t indexCount,
		const ::VertexInput* vertexInput, VkIndexType indexType, VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	// Construct from vertices/indices/weights/shapekeys
	ENGINE_EXPORT Mesh(const std::string& name, ::Device* device,
		const void* vertices, const VertexWeight* weights, const std::vector<std::pair<std::string, const void*>>&  shapeKeys, 
		const void* indices, uint32_t vertexCount, uint32_t vertexSize, uint32_t indexCount,
		const ::VertexInput* vertexInput, VkIndexType indexType, VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	ENGINE_EXPORT ~Mesh() override;

	// Creates a cube, using float3 vertices
	ENGINE_EXPORT static Mesh* CreateCube(const std::string& name, Device* device, float radius = 1.f);
	// Creates a plane facing the positive z axis, using StdVertex vertices
	ENGINE_EXPORT static Mesh* CreatePlane(const std::string& name, Device* device, float size = 1.f);

	inline std::shared_ptr<Buffer> VertexBuffer() const { return mVertexBuffer; }
	inline std::shared_ptr<Buffer> IndexBuffer () const { return mIndexBuffer; }
	inline std::shared_ptr<Buffer> WeightBuffer() const { return mWeightBuffer; }
	inline std::shared_ptr<Buffer> ShapeKey(const std::string& name) const { return (mShapeKeys.count(name) == 0) ? nullptr : mShapeKeys.at(name); }

	inline VkPrimitiveTopology Topology() const { return mTopology; }
	inline uint32_t BaseVertex() const { return mBaseVertex; }
	inline uint32_t VertexCount() const { return mVertexCount; }
	inline uint32_t VertexSize() const { return mVertexSize; }
	inline uint32_t BaseIndex() const { return mBaseIndex; }
	inline uint32_t IndexCount() const { return mIndexCount; }
	inline VkIndexType IndexType() const { return mIndexType; }

	inline TriangleBvh2* BVH() const { return mBvh; }
	ENGINE_EXPORT bool Intersect(const Ray& ray, float* t, bool any);

	inline const ::VertexInput* VertexInput() const { return mVertexInput; }

	inline AABB Bounds() const { return mBounds; }
	inline void Bounds(const AABB& b) { mBounds = b; }

private:
	friend class AssetManager;
	ENGINE_EXPORT Mesh(const std::string& name, ::Device* device, const std::string& filename, float scale = 1.f);

	TriangleBvh2* mBvh;

	const ::VertexInput* mVertexInput;
	uint32_t mBaseVertex;
	uint32_t mVertexCount;
	VkDeviceSize mVertexSize;
	uint32_t mBaseIndex;
	uint32_t mIndexCount;
	VkIndexType mIndexType;
	VkPrimitiveTopology mTopology;
	
	std::unordered_map<std::string, Animation*> mAnimations;

	AABB mBounds;
	std::shared_ptr<Buffer> mWeightBuffer;
	std::shared_ptr<Buffer> mIndexBuffer;

	std::shared_ptr<Buffer> mVertexBuffer;
	std::unordered_map<std::string, std::shared_ptr<Buffer>> mShapeKeys;
};