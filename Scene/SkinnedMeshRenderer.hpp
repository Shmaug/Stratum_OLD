#pragma once

#include <Content/Animation.hpp>
#include <Scene/MeshRenderer.hpp>

class SkinnedMeshRenderer : public MeshRenderer {
public:
	ENGINE_EXPORT SkinnedMeshRenderer(const std::string& name);
	ENGINE_EXPORT ~SkinnedMeshRenderer();

	ENGINE_EXPORT virtual void Mesh(::Mesh* mesh, Object* rigRoot);
	ENGINE_EXPORT virtual void Mesh(std::shared_ptr<::Mesh> mesh, Object* rigRoot);
	inline virtual ::Mesh* Mesh() const { return mMesh.index() == 0 ? std::get<::Mesh*>(mMesh) : std::get<std::shared_ptr<::Mesh>>(mMesh).get(); }

	ENGINE_EXPORT virtual Bone* GetBone(const std::string& name) const;
	
	ENGINE_EXPORT virtual void Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, ::Material* materialOverride) override;

protected:
	struct DeviceData {
		Buffer** mPoseBuffers;
		Buffer** mVertices;

		Buffer** mObjectBuffers;
		DescriptorSet** mDescriptorSets;
		bool* mUniformDirty;
		Buffer** mBoundLightBuffers;

	    ::Mesh* mMesh;
	};
	std::unordered_map<Device*, DeviceData> mDeviceData;
	std::unordered_map<std::string, Bone*> mBoneMap;
	AnimationRig mRig;
	SkinnedMeshRenderer* mCopyRig;
	Object* mRigRoot;
};