#pragma once

#include <Content/Material.hpp>
#include <Content/Mesh.hpp>
#include <Core/DescriptorSet.hpp>
#include <Scene/Renderer.hpp>
#include <Util/Util.hpp>

class ClothRenderer : public Renderer {
public:
	bool mVisible;

	ENGINE_EXPORT ClothRenderer(const std::string& name);
	ENGINE_EXPORT ~ClothRenderer();

	inline virtual PassType PassMask() override { return (PassType)(mMaterial ? mMaterial->PassMask() : (PassType)0); }

	inline virtual ::Material* Material() { return mMaterial.get(); }
	ENGINE_EXPORT virtual void Material(std::shared_ptr<::Material> m);

	inline virtual bool Visible() override { return mVisible && mMaterial && EnabledHierarchy(); }
	inline virtual uint32_t RenderQueue() override { return mMaterial ? mMaterial->RenderQueue() : Renderer::RenderQueue(); }
	ENGINE_EXPORT virtual void Draw(CommandBuffer* commandBuffer, Camera* camera, PassType pass) override;

	ENGINE_EXPORT virtual void PreRender(CommandBuffer* commandBuffer, Camera* camera, PassType pass);
	ENGINE_EXPORT virtual void DrawInstanced(CommandBuffer* commandBuffer, Camera* camera, uint32_t instanceCount, VkDescriptorSet instanceDS, PassType pass);

	ENGINE_EXPORT virtual void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera);

	inline virtual AABB Bounds() override { UpdateTransform(); return mAABB; }

private:
	uint32_t mRayMask;

protected:
    AABB mClothAABB;
    uint32_t mResolution;
	std::shared_ptr<::Material> mMaterial;

	AABB mAABB;
	std::variant<::Mesh*, std::shared_ptr<::Mesh>> mMesh;
	ENGINE_EXPORT virtual bool UpdateTransform() override;
};