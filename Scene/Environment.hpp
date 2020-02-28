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

	inline float3 AmbientLight() const { return mAmbientLight; }
	inline void AmbientLight(const float3& t) { mAmbientLight = t; }

	inline Texture* EnvironmentTexture() const { return mEnvironmentTexture; }
	inline void EnvironmentTexture(Texture* t) { mEnvironmentTexture = t; }

	inline bool EnableScattering() const { return mEnableScattering; }
	inline void EnableScattering(bool t) { mEnableScattering = t; }

	inline bool EnableCelestials() const { return mEnableCelestials; }
	inline void EnableCelestials(bool t) { mEnableCelestials = t; }

	inline float TimeOfDay() const { return mTimeOfDay; }
	inline void TimeOfDay(float t) { mTimeOfDay = t; }

	ENGINE_EXPORT void SetEnvironment(Camera* camera, Material* material);
	
private:
	friend class Scene;
	ENGINE_EXPORT void PreRender(CommandBuffer* commandBuffer, Camera* camera);

	ENGINE_EXPORT void InitializeAtmosphere();

	bool mAtmosphereInitialized;

	bool mEnableCelestials;
	bool mEnableScattering;

	Texture* mEnvironmentTexture;

	// Scattering settings

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

	Texture* mMoonTexture;
	Texture* mStarTexture;

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

	Buffer* mScatterParamBuffer;

	// global settings

	float3 mAmbientLight;
	Light* mSun;
	Light* mMoon;

    Scene* mScene;

	Shader* mShader;
	std::shared_ptr<Material> mSkyboxMaterial;


	float mMoonSize;
};