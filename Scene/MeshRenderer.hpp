#pragma once

#include <Content/Material.hpp>
#include <Content/Mesh.hpp>
#include <Core/DescriptorSet.hpp>
#include <Scene/Renderer.hpp>
#include <Util/Util.hpp>

class MeshRenderer : public Renderer {
public:
	union PushConstantValue {
		int32_t intValue;
		int2 int2Value;
		int3 int3Value;
		int4 int4Value;
		uint32_t uintValue;
		uint2 uint2Value;
		uint3 uint3Value;
		uint4 uint4Value;
		float  floatValue;
		float2 float2Value;
		float3 float3Value;
		float4 float4Value;
		inline PushConstantValue(int32_t v) : intValue(v) {}
		inline PushConstantValue(const int2& v) : int2Value(v) {}
		inline PushConstantValue(const int3& v) : int3Value(v) {}
		inline PushConstantValue(const int4& v) : int4Value(v) {}
		inline PushConstantValue(uint32_t v) : uintValue(v) {}
		inline PushConstantValue(const uint2& v) : uint2Value(v) {}
		inline PushConstantValue(const uint3& v) : uint3Value(v) {}
		inline PushConstantValue(const uint4& v) : uint4Value(v) {}
		inline PushConstantValue(float v) : floatValue(v) {}
		inline PushConstantValue(const float2& v) : float2Value(v) {}
		inline PushConstantValue(const float3& v) : float3Value(v) {}
		inline PushConstantValue(const float4& v) : float4Value(v) {}
	};

	bool mVisible;

	ENGINE_EXPORT MeshRenderer(const std::string& name);
	ENGINE_EXPORT ~MeshRenderer();

	inline virtual PassType PassMask() override { return (PassType)(mMaterial ? mMaterial->PassMask() : (PassType)0); }

	inline virtual void Mesh(::Mesh* m) { mMesh = m; Dirty(); }
	inline virtual void Mesh(std::shared_ptr<::Mesh> m) { mMesh = m; Dirty(); }
	inline virtual ::Mesh* Mesh() const { return mMesh.index() == 0 ? std::get<::Mesh*>(mMesh) : std::get<std::shared_ptr<::Mesh>>(mMesh).get(); }

	inline virtual ::Material* Material() { return mMaterial.get(); }
	ENGINE_EXPORT virtual void Material(std::shared_ptr<::Material> m) { mMaterial = m; }

	template<typename T>
	inline void PushConstant(const std::string& name, const T& value) { mPushConstants.emplace(name, PushConstantValue(value)); }
	inline PushConstantValue PushConstant(const std::string& name) { return mPushConstants.at(name); }

	inline virtual bool Visible() override { return mVisible && Mesh() && mMaterial && EnabledHierarchy(); }
	inline virtual uint32_t RenderQueue() override { return mMaterial ? mMaterial->RenderQueue() : Renderer::RenderQueue(); }
	ENGINE_EXPORT virtual void Draw(CommandBuffer* commandBuffer, Camera* camera, PassType pass) override;

	ENGINE_EXPORT virtual void PreRender(CommandBuffer* commandBuffer, Camera* camera, PassType pass) override;
	ENGINE_EXPORT virtual void DrawInstanced(CommandBuffer* commandBuffer, Camera* camera, uint32_t instanceCount, VkDescriptorSet instanceDS, PassType pass);

	ENGINE_EXPORT virtual bool Intersect(const Ray& ray, float* t, bool any) override;
	inline virtual AABB Bounds() override { UpdateTransform(); return mAABB; }

private:
	uint32_t mRayMask;

protected:
	std::shared_ptr<::Material> mMaterial;
	std::unordered_map<std::string, PushConstantValue> mPushConstants;

	AABB mAABB;
	std::variant<::Mesh*, std::shared_ptr<::Mesh>> mMesh;
	ENGINE_EXPORT virtual bool UpdateTransform() override;
};