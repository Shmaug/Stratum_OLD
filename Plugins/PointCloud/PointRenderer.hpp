#pragma once

#include <Scene/Renderer.hpp>
#include <Core/EnginePlugin.hpp>


class PointRenderer : public Renderer {
public:
	#pragma pack(push)
	#pragma pack(1)
	struct Point {
		vec3 mPosition;
		vec3 mColor;
	};
	#pragma pack(pop)

	bool mVisible;

	PLUGIN_EXPORT PointRenderer(const std::string& name);
	PLUGIN_EXPORT ~PointRenderer();

	PLUGIN_EXPORT void Points(const std::vector<Point>& points);

	inline std::shared_ptr<::Material> Material() const { return mMaterial; }
	inline void Material(std::shared_ptr<::Material> m) { mMaterial = m; }

	inline virtual bool Visible() override { return mVisible && mPoints.size(); }
	inline virtual uint32_t RenderQueue() override { return mMaterial ? mMaterial->RenderQueue() : Renderer::RenderQueue(); }

	PLUGIN_EXPORT virtual void Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, ::Material* materialOverride) override;

	inline AABB Bounds() override { UpdateTransform(); return mAABB; }

private:
	struct DeviceData {
		bool mPointsDirty;
		Buffer* mPointBuffer;
		Buffer** mObjectBuffers;
		DescriptorSet** mDescriptorSets;
		bool* mDescriptorDirty;
	};

	AABB mPointAABB;
	AABB mAABB;
	std::vector<Point> mPoints;
	std::shared_ptr<::Material> mMaterial;

	std::unordered_map<Device*, DeviceData> mDeviceData;
protected:
	ENGINE_EXPORT virtual bool UpdateTransform() override;
};