#include "Environment.hpp"

#include <Scene/MeshRenderer.hpp>

using namespace std;

ENGINE_PLUGIN(Environment)

Environment::Environment() : mEnvironmentStrength(1.f), mScene(nullptr), mSkybox(nullptr), mEnvironmentTexture(nullptr) {
	mEnabled = true;
}
Environment::~Environment() {
	mScene->RemoveObject(mSkybox);
}

bool Environment::Init(Scene* scene) {
	mScene = scene;
	
	mEnvironmentTexture = mScene->AssetManager()->LoadTexture("Assets/sky.hdr", false);
	mEnvironmentStrength = .5f;

	shared_ptr<Material> skyboxMat = make_shared<Material>("Skybox", mScene->AssetManager()->LoadShader("Shaders/skybox.shader"));
	skyboxMat->SetParameter("EnvironmentTexture", mEnvironmentTexture);
	shared_ptr<MeshRenderer> skybox = make_shared<MeshRenderer>("SkyCube");
	skybox->LocalScale(1e23f);
	skybox->Mesh(shared_ptr<Mesh>(Mesh::CreateCube("Cube", mScene->Instance())));
	skybox->Material(skyboxMat);
	skybox->CastShadows(false);
	mSkybox = skybox.get();
	mScene->AddObject(skybox);

	return true;
}