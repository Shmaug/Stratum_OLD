#include <Core/EnginePlugin.hpp>
#include <iterator>
#include <sstream>

#include <Util/Profiler.hpp>

#include <Plugins/CameraControl/CameraControl.hpp>
#include "PointRenderer.hpp"

using namespace std;

class PointCloud : public EnginePlugin {
public:
	PLUGIN_EXPORT PointCloud();
	PLUGIN_EXPORT ~PointCloud();

	PLUGIN_EXPORT bool Init(Scene* scene, DeviceManager* deviceManager) override;
	PLUGIN_EXPORT void Update(const FrameTime& frameTime) override;

private:
	CameraControl* mCameraControl;
	PointRenderer* mBunny;
	PointRenderer* mBear;
	PointRenderer* mDragon;

	Scene* mScene;
	float mPointSize;
	float mAnimStart;
	shared_ptr<Material> mPointMaterial;
};

ENGINE_PLUGIN(PointCloud)

PointCloud::PointCloud() : mScene(nullptr), mCameraControl(nullptr), mBunny(nullptr), mBear(nullptr), mDragon(nullptr), mPointSize(0.0025f), mAnimStart(0) {}
PointCloud::~PointCloud() {
	mScene->RemoveObject(mBunny);
	mScene->RemoveObject(mBear);
	mScene->RemoveObject(mDragon);
	safe_delete(mBunny);
	safe_delete(mBear);
	safe_delete(mDragon);
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

		points.push_back({ vec4(point, 1), vec4(col, 1) });
	}

	printf("Loaded %s (%d points)\n", filename.c_str(), (int)points.size());

	PointRenderer* renderer = new PointRenderer(filename);
	renderer->Points(points);
	return renderer;
}

bool PointCloud::Init(Scene* scene, DeviceManager* deviceManager) {
	mScene = scene;

	Shader* pointShader = deviceManager->AssetDatabase()->LoadShader("Shaders/points.shader");
	mPointMaterial = make_shared<Material>("PointCloud", pointShader);

	thread th0([&]() {
		mBunny = LoadPoints("Assets/bunny.obj");
	});
	thread th1([&]() {
		mBear = LoadPoints("Assets/bear.obj");
	});
	thread th2([&]() {
		mDragon = LoadPoints("Assets/dragon.obj");
	});
	
	th0.join();
	th1.join();
	th2.join();

	if (!mBunny || !mBear || !mDragon) return false;
	
	mBunny->Material(mPointMaterial);
	mBear->Material(mPointMaterial);
	mDragon->Material(mPointMaterial);
	scene->AddObject(mBunny);
	scene->AddObject(mBear);
	scene->AddObject(mDragon);

	mPointMaterial->SetParameter("Time", 0.f);
	mPointMaterial->SetParameter("PointSize", mPointSize);
	mPointMaterial->SetParameter("Extents", mBunny->Bounds().mExtents);

	mCameraControl = mScene->GetPlugin<CameraControl>();
	float dist = mBunny->Bounds().mExtents.y / tanf(mScene->Cameras()[0]->FieldOfView() * .5f);
	mCameraControl->CameraPivot()->LocalPosition(mBunny->Bounds().mCenter - vec3(0, 0, dist));

	mBear->mVisible = false;
	mDragon->mVisible = false;

	return true;
}

void PointCloud::Update(const FrameTime& frameTime) {
	if (Input::KeyDown(GLFW_KEY_UP)) {
		mPointSize += frameTime.mDeltaTime * 0.0025f;
		mPointMaterial->SetParameter("PointSize", mPointSize);
	}
	if (Input::KeyDown(GLFW_KEY_DOWN)) {
		mPointSize -= frameTime.mDeltaTime * 0.0025f;
		mPointMaterial->SetParameter("PointSize", mPointSize);
	}

	if (Input::KeyDownFirst(GLFW_KEY_F1)) {
		mBunny->mVisible = true;
		mBear->mVisible = false;
		mDragon->mVisible = false;
		mAnimStart = frameTime.mTotalTime;
		mPointMaterial->SetParameter("Extents", mBunny->Bounds().mExtents);

		float dist = mBunny->Bounds().mExtents.y / tanf(mScene->Cameras()[0]->FieldOfView() * .5f);
		mCameraControl->CameraPivot()->LocalPosition(mBunny->Bounds().mCenter - vec3(0, 0, dist));
	}
	if (Input::KeyDownFirst(GLFW_KEY_F2)) {
		mBunny->mVisible = false;
		mBear->mVisible = true;
		mDragon->mVisible = false;
		mAnimStart = frameTime.mTotalTime;
		mPointMaterial->SetParameter("Extents", mBear->Bounds().mExtents);

		float dist = mBear->Bounds().mExtents.y / tanf(mScene->Cameras()[0]->FieldOfView() * .5f);
		mCameraControl->CameraPivot()->LocalPosition(mBear->Bounds().mCenter - vec3(0, 0, dist));
	}
	if (Input::KeyDownFirst(GLFW_KEY_F3)) {
		mBunny->mVisible = false;
		mBear->mVisible = false;
		mDragon->mVisible = true;
		mAnimStart = frameTime.mTotalTime;
		mPointMaterial->SetParameter("Extents", mDragon->Bounds().mExtents);

		float dist = mDragon->Bounds().mExtents.y / tanf(mScene->Cameras()[0]->FieldOfView() * .5f);
		mCameraControl->CameraPivot()->LocalPosition(mDragon->Bounds().mCenter - vec3(0, 0, dist));
	}

	mPointMaterial->SetParameter("Time", frameTime.mTotalTime - mAnimStart);
}