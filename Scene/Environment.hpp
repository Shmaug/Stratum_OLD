#pragma once

#include <Content/Material.hpp>
#include <Content/Texture.hpp>
#include <Scene/Object.hpp>
#include <Util/Util.hpp>

class Scene;

class Environment {
public:
	ENGINE_EXPORT Environment(Scene* scene);
	ENGINE_EXPORT ~Environment();

	ENGINE_EXPORT void SetEnvironment(Camera* camera, Material* material);
	
private:
	friend class Scene;
	ENGINE_EXPORT void PreRender(CommandBuffer* commandBuffer, Camera* camera);

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

	struct DevLUT {
		Texture* mParticleDensityLUT;
		Texture* mSkyboxLUT;
		Texture* mSkyboxLUT2;
	};
	struct CamLUT {
		Texture* mInscatterLUT;
		Texture* mOutscatterLUT;
	};

	std::unordered_map<Device*, DevLUT> mDeviceLUTs;
	std::unordered_map<Camera*, CamLUT*> mCameraLUTs;

    Scene* mScene;
	Object* mSkybox;

	Shader* mShader;
	Material* mSkyboxMaterial;

	Texture* mMoonTexture;
	Texture* mStarTexture;

	float mMoonSize;
};