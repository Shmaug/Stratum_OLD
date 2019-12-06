#pragma once

#include <Content/Material.hpp>
#include <Content/Texture.hpp>
#include <Scene/Object.hpp>
#include <Scene/Light.hpp>
#include <Util/Util.hpp>

class Scene;

class Environment {
public:
	ENGINE_EXPORT Environment(Scene* scene);
	ENGINE_EXPORT ~Environment();

	ENGINE_EXPORT void Update();

	inline float TimeOfDay() const { return mTimeOfDay; }
	inline void TimeOfDay(float t) { mTimeOfDay = t; }

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

	float mSunIntensity;

	float mAtmosphereHeight;
	float mPlanetRadius;
	float4 mDensityScale;
	float4 mRayleighSct;
	float4 mMieSct;

	float4 mAmbientLUT[128];
	float4 mDirectionalLUT[128];

	struct DevLUT {
		Texture* mParticleDensityLUT;
		Texture* mSkyboxLUTR;
		Texture* mSkyboxLUTM;
	};
	struct CamLUT {
		Texture* mInscatterLUT;
		Texture* mOutscatterLUT;
		Texture* mLightShaftLUT;
	};

	std::unordered_map<Device*, DevLUT> mDeviceLUTs;
	std::unordered_map<Camera*, CamLUT*> mCameraLUTs;

	float mTimeOfDay; // 0-1

	float3 mAmbientLight;
	Light* mSun;
	Light* mMoon;

    Scene* mScene;
	Object* mSkybox;

	Shader* mShader;
	Material* mSkyboxMaterial;

	Texture* mMoonTexture;
	Texture* mStarTexture;

	float mMoonSize;
};