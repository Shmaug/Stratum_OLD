#pragma once

#include <Content/AssetManager.hpp>
#include <Content/Material.hpp>
#include <Core/Instance.hpp>
#include <Core/DescriptorSet.hpp>
#include <Core/PluginManager.hpp>
#include <Input/InputManager.hpp>
#include <Scene/ObjectBvh2.hpp>
#include <Scene/Camera.hpp>
#include <Scene/Gizmos.hpp>
#include <Scene/Environment.hpp>
#include <Scene/Light.hpp>
#include <Scene/Object.hpp>
#include <Util/Util.hpp>

#include <functional>

class Renderer;

/// Holds scene Objects. In general, plugins will add objects during their lifetime,
/// and remove objects during or at the end of their lifetime.
/// This makes the shared_ptr destroy when the plugin removes the object, allowing the plugin's module
/// to free the memory.
class Scene {
public:
	ENGINE_EXPORT ~Scene();

	ENGINE_EXPORT void AddObject(std::shared_ptr<Object> object);
	ENGINE_EXPORT void RemoveObject(Object* object);
	
	/// Loads a 3d scene from a file, separating all meshes with different topologies/materials into separate MeshRenderers and 
	/// replicating the heirarchy stored in the file, and creating new materials using the specified shader.
	/// Calls materialSetupFunc for every aiMaterial in the file, to create a corresponding Material
	ENGINE_EXPORT Object* LoadModelScene(const std::string& filename,
		std::function<std::shared_ptr<Material>(Scene*, aiMaterial*)> materialSetupFunc,
		std::function<void(Scene*, Object*, aiMaterial*)> objectSetupFunc,
		float scale, float directionalLightIntensity, float spotLightIntensity, float pointLightIntensity);

	inline float FPS() const { return mFps; }
	inline float TotalTime() const { return mTotalTime; }
	inline float DeltaTime() const { return mDeltaTime; }

	inline float FixedTimeStep() const { return mFixedTimeStep; }
	inline void FixedTimeStep(float step) { mFixedTimeStep = step; }
	inline float PhysicsTimeLimitPerFrame() const { return mPhysicsTimeLimitPerFrame; }
	inline void PhysicsTimeLimitPerFrame(float t) { mPhysicsTimeLimitPerFrame = t; }

	// Render to a camera
	// Note: this is called automatically on all cameras added to the scene via Scene->AddObject()
	ENGINE_EXPORT void Render(CommandBuffer* commandBuffer, Camera* camera, Framebuffer* framebuffer = nullptr, PassType pass = PASS_MAIN, bool clear = true);

	inline Object* Raycast(const Ray& worldRay, float* t = nullptr, bool any = false, uint32_t mask = 0xFFFFFFFF) { return BVH()->Intersect(worldRay, t, any, mask); }


	/// Buffer of GPULight structs (defined in shadercompat.h)
	inline Buffer* LightBuffer() const { return mLightBuffers[mInstance->Device()->FrameContextIndex()]; }
	/// Buffer of ShadowData structs (defined in shadercompat.h)
	inline Buffer* ShadowBuffer() const { return mShadowBuffers[mInstance->Device()->FrameContextIndex()]; }
	/// Shadow atlas of multiple shadowmaps
	inline Texture* ShadowAtlas() const { return mShadowAtlases[mInstance->Device()->FrameContextIndex()]; }
	inline const std::vector<Light*>& ActiveLights() const { return mActiveLights; }
	inline const std::vector<Camera*>& Cameras() const { return mCameras; }

	/// Size in UV coordinates of the size of one texel in the shadow atlas
	inline float2 ShadowTexelSize() const { return mShadowTexelSize; }

	inline ::AssetManager* AssetManager() const { return mAssetManager; }
	inline ::InputManager* InputManager() const { return mInputManager; }
	inline ::PluginManager* PluginManager() const { return mPluginManager; }
	inline ::Environment* Environment() const { return mEnvironment; }
	inline ::Instance* Instance() const { return mInstance; }

	inline void DrawGizmos(bool g) { mDrawGizmos = g; }
	inline bool DrawGizmos() const { return mDrawGizmos; }

	// All objects, in order off insertion
	ENGINE_EXPORT std::vector<Object*> Objects() const;

	ENGINE_EXPORT ObjectBvh2* BVH();
	inline void BvhDirty(Object* reason) { mBvhDirty = true; }
	// Frame id of the last bvh build
	inline uint64_t LastBvhBuild() { return mLastBvhBuild; }

private:
	friend class Stratum;
	ENGINE_EXPORT void Update(CommandBuffer* commandBuffer);
	ENGINE_EXPORT void PreFrame(CommandBuffer* commandBuffer);
	ENGINE_EXPORT void PrePresent();
	ENGINE_EXPORT Scene(::Instance* instance, ::AssetManager* assetManager, ::InputManager* inputManager, ::PluginManager* pluginManager);
	
	/// Used in PreFrame() to add a shadow camera to mShadowCameras
	ENGINE_EXPORT void AddShadowCamera(uint32_t si, ShadowData* sd, bool ortho, float size, const float3& pos, const quaternion& rot, float near, float far);

	ENGINE_EXPORT void Render(CommandBuffer* commandBuffer, Camera* camera, Framebuffer* framebuffer, PassType pass, bool clear, std::vector<Object*>& renderList);

	float mFixedAccumulator;
	float mFixedTimeStep;

	std::chrono::high_resolution_clock mClock;
	std::chrono::high_resolution_clock::time_point mStartTime;
	std::chrono::high_resolution_clock::time_point mLastFrame;
	float mTotalTime;
	float mDeltaTime;
	float mFrameTimeAccum;
	float mPhysicsTimeLimitPerFrame;
	uint32_t mFrameCount;
	float mFps;

	Mesh* mSkyboxCube;

	ObjectBvh2* mBvh;
	uint64_t mLastBvhBuild;
	bool mBvhDirty;

	float2 mShadowTexelSize;

	uint32_t mShadowCount;

	Buffer** mLightBuffers;
	Buffer** mShadowBuffers;
	std::vector<Camera*> mShadowCameras;
	Framebuffer* mShadowAtlasFramebuffer;

	Texture** mShadowAtlases;

	std::vector<Light*> mActiveLights;

	::AssetManager* mAssetManager;
	::Instance* mInstance;
	::InputManager* mInputManager;
	::PluginManager* mPluginManager;
	::Environment* mEnvironment;
	std::vector<std::shared_ptr<Object>> mObjects;
	std::vector<Light*> mLights;
	std::vector<Camera*> mCameras;
	std::vector<Renderer*> mRenderers;
	std::vector<Object*> mRenderList;
	bool mDrawGizmos;
};