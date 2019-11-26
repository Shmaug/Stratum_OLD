#include "Environment.hpp"

#include <Scene/Scene.hpp>
#include <Scene/MeshRenderer.hpp>

using namespace std;

ENGINE_PLUGIN(Environment)

Environment::Environment() : 
	mScene(nullptr), mSkybox(nullptr),
	mSkyboxLUTSize(float3(32, 128, 32)),
	mInscatteringLUTSize(float3(8, 8, 64)),
	mLightLUTSize(128),
	mSampleCount(16),
	mMaxRayLength(400),
	mIncomingLight(float4(4, 4, 4, 4)),
	mRayleighScatterCoef(1),
	mRayleighExtinctionCoef(1),
	mMieScatterCoef(1),
	mMieExtinctionCoef(1),
	mMieG(0.76f),
	mDistanceScale(1),
	mLightColorIntensity(1),
	mAmbientColorIntensity(1),
	mSunIntensity(1),
	mAtmosphereHeight(80000.0f),
	mPlanetRadius(6371000.0f),
	mDensityScale(float4(7994, 1200, 0, 0)),
	mRayleighSct(float4(5.8f, 13.5f, 33.1f, 0) * 0.000001f),
	mMieSct(float4(2, 2, 2, 0) * 0.00001f) {
	mEnabled = true;
}
Environment::~Environment() {
	for (auto t : mSkyTextures)
		safe_delete_array(t.second);

	mScene->RemoveObject(mSkybox);
}

bool Environment::Init(Scene* scene) {
	mScene = scene;
	
	shared_ptr<Material> skyboxMat = make_shared<Material>("Skybox", mScene->AssetManager()->LoadShader("Shaders/skybox.shader"));
	shared_ptr<MeshRenderer> skybox = make_shared<MeshRenderer>("SkyCube");
	skybox->LocalScale(1e23f);
	skybox->Mesh(shared_ptr<Mesh>(Mesh::CreateCube("Cube", mScene->Instance())));
	skybox->Material(skyboxMat);
	skybox->CastShadows(false);
	mSkybox = skybox.get();
	mScene->AddObject(skybox);

	mSkyboxMaterial = skyboxMat.get();

	return true;
}

void Environment::PreRender(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {
	if (pass != Main) return;

	if (mSkyTextures.count(commandBuffer->Device()) == 0) {
		Texture**& t = mSkyTextures[commandBuffer->Device()];
		t = new Texture*[commandBuffer->Device()->MaxFramesInFlight()];
		for (uint32_t i = 0; i < commandBuffer->Device()->MaxFramesInFlight(); i++)
			t[i] = mScene->AssetManager()->LoadTexture("Assets/sky.hdr", false);
	}
	mSkyboxMaterial->SetParameter("EnvironmentTexture", EnvironmentTexture(commandBuffer->Device()));

}