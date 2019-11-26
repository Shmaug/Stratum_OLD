#include <Core/EnginePlugin.hpp>

#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Util/Profiler.hpp>
#include <Math/Noise.hpp>

#include "TerrainRenderer.hpp"

using namespace std;

class TerrainSystem : public EnginePlugin {
public:
	PLUGIN_EXPORT TerrainSystem();
	PLUGIN_EXPORT ~TerrainSystem();

	PLUGIN_EXPORT bool Init(Scene* scene) override;
	PLUGIN_EXPORT void Update() override;
	PLUGIN_EXPORT void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera);

private:
	Scene* mScene;
	TerrainRenderer* mTerrain;
	vector<Object*> mObjects;
};

ENGINE_PLUGIN(TerrainSystem)

TerrainSystem::TerrainSystem() : mScene(nullptr), mTerrain(nullptr) {
	mEnabled = true;
}
TerrainSystem::~TerrainSystem() {
	for (Object* obj : mObjects)
		mScene->RemoveObject(obj);
}

bool TerrainSystem::Init(Scene* scene) {
	mScene = scene;


	shared_ptr<Material> mat = make_shared<Material>("Terrain", mScene->AssetManager()->LoadShader("Shaders/terrain.shader"));

	mat->SetParameter("MainTextures", 0, mScene->AssetManager()->LoadTexture("Assets/grass/grass1_col.png"));
	mat->SetParameter("NormalTextures", 0, mScene->AssetManager()->LoadTexture("Assets/grass/grass1_nrm.png", false));
	mat->SetParameter("MaskTextures", 0, mScene->AssetManager()->LoadTexture("Assets/grass/grass1_msk.png", false));

	mat->SetParameter("MainTextures", 1, mScene->AssetManager()->LoadTexture("Assets/ground/ground3_col.jpg"));
	mat->SetParameter("NormalTextures", 1, mScene->AssetManager()->LoadTexture("Assets/ground/ground3_nrm.jpg", false));
	mat->SetParameter("MaskTextures", 1, mScene->AssetManager()->LoadTexture("Assets/ground/ground3_msk.jpg", false));

	mat->SetParameter("MainTextures", 2, mScene->AssetManager()->LoadTexture("Assets/rock/rock13_col.jpg"));
	mat->SetParameter("NormalTextures", 2, mScene->AssetManager()->LoadTexture("Assets/rock/rock13_nrm.jpg", false));
	mat->SetParameter("MaskTextures", 2, mScene->AssetManager()->LoadTexture("Assets/rock/rock13_msk.png", false));

	shared_ptr<TerrainRenderer> terrain = make_shared<TerrainRenderer>("Terrain", 10000.f, 256.f);
	mScene->AddObject(terrain);
	terrain->Material(mat);
	terrain->LocalPosition(0, -128.f, 0);
	mTerrain = terrain.get();
	mObjects.push_back(mTerrain);

	shared_ptr<Light> sun = make_shared<Light>("Sun");
	mScene->AddObject(sun);
	sun->CastShadows(true);
	sun->Color(float3(1, .99f, .95f));
	sun->LocalRotation(quaternion(float3(PI / 4, PI / 4, 0)));
	sun->Type(Sun);
	mObjects.push_back(sun.get());

	return true;
}

void TerrainSystem::Update() {
}

void TerrainSystem::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
	
}