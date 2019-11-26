#pragma once

#include <Content/Texture.hpp>
#include <Core/EnginePlugin.hpp>
#include <Scene/Scene.hpp>

class Environment : public EnginePlugin {
public:
	PLUGIN_EXPORT Environment();
	PLUGIN_EXPORT ~Environment();

	PLUGIN_EXPORT bool Init(Scene* scene) override;

	inline int Priority() override { return 1000; }
;
	PLUGIN_EXPORT void PreRender(CommandBuffer* commandBuffer, Camera* camera, PassType pass);

	inline Texture* EnvironmentTexture(Device* device) { return mSkyTextures.at(device)[device->FrameContextIndex()]; }
	
private:
	float3 mSkyboxLUTSize;
	float3 mInscatteringLUTSize;
	uint32_t mLightLUTSize;

	uint32_t mSampleCount;
	float mMaxRayLength;

	float4 mIncomingLight;
	float mRayleighScatterCoef;
	float mRayleighExtinctionCoef;
	float mMieScatterCoef;
	float mMieExtinctionCoef;
	float mMieG;
	float mDistanceScale;

	float mLightColorIntensity;
	float mAmbientColorIntensity;

	float mSunIntensity;

	float mAtmosphereHeight;
	float mPlanetRadius;
	float4 mDensityScale;
	float4 mRayleighSct;
	float4 mMieSct;

	std::unordered_map<Device*, Texture**> mOutscatteringLUTs;
	std::unordered_map<Device*, Texture**> mInscatteringLUTs;
	std::unordered_map<Device*, Texture**> mSkyTextures;

    Scene* mScene;
	Object* mSkybox;
	Material* mSkyboxMaterial;
};