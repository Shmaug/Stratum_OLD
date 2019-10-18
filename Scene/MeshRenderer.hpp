#pragma once

#include <memory>
#include <variant>
#include <unordered_map>

#include <Content/Material.hpp>
#include <Content/Mesh.hpp>
#include <Core/DescriptorSet.hpp>
#include <Scene/Renderer.hpp>
#include <Util/Util.hpp>

class MeshRenderer : public Renderer {
public:
	bool mVisible;

	ENGINE_EXPORT MeshRenderer(const std::string& name);
	ENGINE_EXPORT ~MeshRenderer();

	inline void Mesh(::Mesh* m) { mMesh = m; }
	inline void Mesh(std::shared_ptr<::Mesh> m) { mMesh = m; Dirty(); }
	inline ::Mesh* Mesh() const { return mMesh.index() == 0 ? std::get<::Mesh*>(mMesh) : std::get<std::shared_ptr<::Mesh>>(mMesh).get(); }

	inline std::shared_ptr<::Material> Material() const { return mMaterial; }
	ENGINE_EXPORT void Material(std::shared_ptr<::Material> m);

	inline virtual bool Visible() override { return mVisible && Mesh() && EnabledHeirarchy(); }
	inline virtual uint32_t RenderQueue() override { return mMaterial ? mMaterial->RenderQueue() : Renderer::RenderQueue(); }
	ENGINE_EXPORT virtual void Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, ::Material* materialOverride) override;
	
	inline virtual AABB Bounds() override { UpdateTransform(); return mAABB; }
	ENGINE_EXPORT virtual AABB BoundsHeirarchy() override;

	ENGINE_EXPORT virtual void DrawGizmos(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, ::Material* materialOverride) override;

private:
	struct DeviceData {
		Buffer** mObjectBuffers;
		DescriptorSet** mDescriptorSets;
		bool* mUniformDirty;
		Buffer** mBoundLightBuffers;
	};

	uint8_t mNeedsLightData;
	VkPushConstantRange mLightCountRange;

	AABB mAABB;

	std::variant<::Mesh*, std::shared_ptr<::Mesh>> mMesh;
	std::shared_ptr<::Material> mMaterial;

	std::unordered_map<Device*, DeviceData> mDeviceData;
protected:
	ENGINE_EXPORT virtual bool UpdateTransform() override;
};