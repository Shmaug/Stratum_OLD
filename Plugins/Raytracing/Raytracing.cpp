#include <Core/EnginePlugin.hpp>

#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Util/Profiler.hpp>

#ifdef __GNUC__
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif

using namespace std;

class Raytracing : public EnginePlugin {
public:
	PLUGIN_EXPORT Raytracing();
	PLUGIN_EXPORT ~Raytracing();

	PLUGIN_EXPORT bool Init(Scene* scene) override;
	PLUGIN_EXPORT void Update() override;

private:
	vector<Object*> mObjects;
	Scene* mScene;
};

ENGINE_PLUGIN(Raytracing)

Raytracing::Raytracing()
	: mScene(nullptr) {
	mEnabled = true;
}
Raytracing::~Raytracing() {
	for (Object* obj : mObjects)
		mScene->RemoveObject(obj);
}

bool Raytracing::Init(Scene* scene) {
	mScene = scene;

	return true;
}

void Raytracing::Update() {
	MouseKeyboardInput* input = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

}
