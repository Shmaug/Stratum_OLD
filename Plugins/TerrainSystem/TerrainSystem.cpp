#include <Core/EnginePlugin.hpp>

#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Util/Profiler.hpp>
#include <Math/Noise.hpp>

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
	vector<Object*> mObjects;
};

ENGINE_PLUGIN(TerrainSystem)

TerrainSystem::TerrainSystem() : mScene(nullptr) {
	mEnabled = true;
}
TerrainSystem::~TerrainSystem() {
	for (Object* obj : mObjects)
		mScene->RemoveObject(obj);
}

bool TerrainSystem::Init(Scene* scene) {
	mScene = scene;

	return true;
}

void TerrainSystem::Update() {

}

void TerrainSystem::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
	
}