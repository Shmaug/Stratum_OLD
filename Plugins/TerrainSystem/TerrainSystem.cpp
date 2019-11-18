#include <Core/EnginePlugin.hpp>

#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Util/Profiler.hpp>
#include <Math/Noise.hpp>

#include "TerrainRenderer.hpp"

#include <Plugins/Environment/Environment.hpp>

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

	Environment* env = mScene->PluginManager()->GetPlugin<Environment>();

	shared_ptr<Material> mat = make_shared<Material>("Terrain", mScene->AssetManager()->LoadShader("Shaders/terrain.shader"));
	mat->SetParameter("ReflectionTexture", env->ReflectionMap());
	mat->SetParameter("ReflectionStrength", env->ReflectionMapStrength());

	shared_ptr<TerrainRenderer> terrain = make_shared<TerrainRenderer>("Terrain", 4096.f, 512.f);
	mScene->AddObject(terrain);
	terrain->Material(mat);
	mTerrain = terrain.get();
	mObjects.push_back(mTerrain);

	return true;
}

void TerrainSystem::Update() {

}

void TerrainSystem::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
	
}