#pragma once

#include <Content/AssetManager.hpp>
#include <Content/Material.hpp>
#include <Core/Instance.hpp>
#include <Core/DescriptorSet.hpp>
#include <Core/PluginManager.hpp>
#include <Input/InputManager.hpp>
#include <Scene/Camera.hpp>
#include <Scene/Gizmos.hpp>
#include <Scene/Environment.hpp>
#include <Scene/Light.hpp>
#include <Scene/Object.hpp>
#include <Scene/Collider.hpp>
#include <Util/Util.hpp>

class VkCAVE;
class Renderer;

/*
Holds scene Objects. In general, plugins will add objects during their lifetime,
and remove objects during or at the end of their lifetime.
This makes the shared_ptr destroy when the plugin removes the object, allowing the plugin's module
to free the memory.
*/
class Scene {
public:
	ENGINE_EXPORT ~Scene();
	ENGINE_EXPORT void AddObject(std::shared_ptr<Object> object);
	ENGINE_EXPORT void RemoveObject(Object* object);

	ENGINE_EXPORT void Update();
	ENGINE_EXPORT void PreFrame(CommandBuffer* commandBuffer);
	ENGINE_EXPORT void Render(Camera* camera, CommandBuffer* commandBuffer, PassType pass = PassType::Main, bool startRenderPass = true);

	ENGINE_EXPORT Collider* Raycast(const Ray& ray, float& hitT, uint32_t mask = 0xFFFFFFFF);

	inline Buffer* LightBuffer(Device* device) const { return mDeviceData.at(device).mLightBuffers[device->FrameContextIndex()]; }
	inline Buffer* ShadowBuffer(Device* device) const { return mDeviceData.at(device).mShadowBuffers[device->FrameContextIndex()]; }
	inline Texture* ShadowAtlas(Device* device) const { return mDeviceData.at(device).mShadowAtlasFramebuffer->ColorBuffer(0); }
	inline const std::vector<Light*>& ActiveLights() const { return mActiveLights; }
	inline const std::vector<Camera*>& Cameras() const { return mCameras; }

	inline float2 ShadowTexelSize() const { return mShadowTexelSize; }

	inline void DrawGizmos(bool g) { mDrawGizmos = g; }
	inline bool DrawGizmos() const { return mDrawGizmos; }
	inline ::AssetManager* AssetManager() const { return mAssetManager; }
	inline ::Instance* Instance() const { return mInstance; }
	inline ::Gizmos* Gizmos() const { return mGizmos; }
	inline ::InputManager* InputManager() const { return mInputManager; }
	inline ::PluginManager* PluginManager() const { return mPluginManager; }
	inline ::Environment* Environment() const { return mEnvironment; }

private:
	friend class VkCAVE;
	ENGINE_EXPORT Scene(::Instance* instance, ::AssetManager* assetManager, ::InputManager* inputManager, ::PluginManager* pluginManager);

	float2 mShadowTexelSize;
	float4 mCascadeSplits;

	struct DeviceData {
		Buffer** mLightBuffers; // resource maintained by the Device, don't need to delete
		Buffer** mShadowBuffers; // resource maintained by the Device, don't need to delete
		std::vector<Camera*> mShadowCameras;
		Framebuffer* mShadowAtlasFramebuffer;
	};
	std::unordered_map<Device*, DeviceData> mDeviceData;

	std::vector<Light*> mActiveLights;

	::AssetManager* mAssetManager;
	::Instance* mInstance;
	::Gizmos* mGizmos;
	::InputManager* mInputManager;
	::PluginManager* mPluginManager;
	::Environment* mEnvironment;
	std::vector<std::shared_ptr<Object>> mObjects;
	std::vector<Light*> mLights;
	std::vector<Camera*> mCameras;
	std::vector<Renderer*> mRenderers;
	std::vector<Renderer*> mRenderList;
	bool mDrawGizmos;

	void AddShadowCamera(DeviceData* dd, uint32_t si, ShadowData* sd, bool ortho, float size, const float3& pos, const quaternion& rot, float near, float far);
};