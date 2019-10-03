#include <Core/EnginePlugin.hpp>
#include <iterator>
#include <sstream>

#include <Util/Profiler.hpp>

#include "PointRenderer.hpp"

using namespace std;

class PointCloud : public EnginePlugin {
public:
	PLUGIN_EXPORT PointCloud();
	PLUGIN_EXPORT ~PointCloud();

	PLUGIN_EXPORT bool Init(Scene* scene, DeviceManager* deviceManager) override;

private:
	vector<Object*> mObjects;
	Scene* mScene;
};

ENGINE_PLUGIN(PointCloud)

PointCloud::PointCloud() : mScene(nullptr) {}
PointCloud::~PointCloud() {
	for (Object* obj : mObjects)
		mScene->RemoveObject(obj);
	for (Object* obj : mObjects)
		safe_delete(obj);
}

PointRenderer* LoadPoints(const string& filename) {
	string objfile;
	if (!ReadFile(filename, objfile)) return nullptr;
	istringstream srcstream(objfile);

	vector<PointRenderer::Point> points;

	string line;
	while (getline(srcstream, line)) {
		istringstream linestream(line);
		vector<string> words{ istream_iterator<string>{linestream}, istream_iterator<string>{} };
		if (words.size() < 4 || words[0] != "v") continue;

		vec3 point = vec3(atof(words[1].c_str()), atof(words[2].c_str()), atof(words[3].c_str()));
		vec3 col(1);
		if (words.size() >= 7)
			col = vec3(atof(words[4].c_str()), atof(words[5].c_str()), atof(words[6].c_str()));

		points.push_back({ point, col });
	}

	printf("Loaded %s (%d points)\n", filename.c_str(), (int)points.size());

	PointRenderer* renderer = new PointRenderer(filename);
	renderer->Points(points);
	return renderer;
}

bool PointCloud::Init(Scene* scene, DeviceManager* deviceManager) {
	mScene = scene;

	Shader* pointShader = deviceManager->AssetDatabase()->LoadShader("Shaders/points.shader");
	shared_ptr<Material> pointMaterial = make_shared<Material>("PointCloud", pointShader);

	PointRenderer* bunny = LoadPoints("Assets/bunny.obj");
	if (!bunny) return false;

	bunny->Material(pointMaterial);
	scene->AddObject(bunny);
	mObjects.push_back(bunny);

	pointMaterial->SetParameter("Time", 0.f);
	pointMaterial->SetParameter("PointSize", 0.01f);
	pointMaterial->SetParameter("Extents", bunny->Bounds().mExtents);

	return true;
}