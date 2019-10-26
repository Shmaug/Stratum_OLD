#pragma once

#include <assimp/mesh.h>

#include <Content/Animation.hpp>
#include <Content/Asset.hpp>
#include <Core/Buffer.hpp>
#include <Math/Geometry.hpp>
#include <Core/DeviceManager.hpp>
#include <Util/Util.hpp>
#include <Scene/Object.hpp>

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
	ENGINE_EXPORT Mesh(const std::string& name, ::DeviceManager* devices, const void* vertices, const void* indices, uint32_t vertexCount, uint32_t vertexSize, uint32_t indexCount, const ::VertexInput* vertexInput, VkIndexType indexType, VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	ENGINE_EXPORT Mesh(const std::string& name, ::Device* device, const void* vertices, const void* indices, uint32_t vertexCount, uint32_t vertexSize, uint32_t indexCount, const ::VertexInput* vertexInput, VkIndexType indexType, VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	ENGINE_EXPORT ~Mesh() override;

	ENGINE_EXPORT static Mesh* CreateCube(const std::string& name, DeviceManager* devices, float radius = 1.f);
	// Creates a plane facing the positive z axis
	ENGINE_EXPORT static Mesh* CreatePlane(const std::string& name, DeviceManager* devices, float size = 1.f);

	inline std::shared_ptr<Buffer> VertexBuffer(Device* device) const { return mDeviceData.at(device).mVertexBuffer; }
	inline std::shared_ptr<Buffer> IndexBuffer(Device* device) const { return mDeviceData.at(device).mIndexBuffer; }
	inline std::shared_ptr<Buffer> WeightBuffer(Device* device) const { return mDeviceData.at(device).mWeightBuffer; }

	inline VkPrimitiveTopology Topology() const { return mTopology; }
	inline uint32_t IndexCount() const { return mIndexCount; }
	inline uint32_t VertexCount() const { return mVertexCount; }
	inline VkIndexType IndexType() const { return mIndexType; }

	inline std::shared_ptr<AnimationRig> Rig() const { return mRig; }

	inline const ::VertexInput* VertexInput() const { return mVertexInput; }

	inline AABB Bounds() const { return mBounds; }
	inline void Bounds(const AABB& b) { mBounds = b; }

private:
	friend class AssetManager;
	ENGINE_EXPORT Mesh(const std::string& name, ::DeviceManager* devices, const std::string& filename, float scale = 1.f);

	const ::VertexInput* mVertexInput;
	uint32_t mIndexCount;
	uint32_t mVertexCount;
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