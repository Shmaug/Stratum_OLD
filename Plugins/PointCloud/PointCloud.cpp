#include <Core/EnginePlugin.hpp>
#include <iterator>
#include <filesystem>

#include <Interface/UICanvas.hpp>
#include <Util/Profiler.hpp>

#include <assimp/scene.h>
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include <assimp/pbrmaterial.h>

#include <Plugins/CameraControl/CameraControl.hpp>
#include "PointRenderer.hpp"

using namespace std;

class PointCloud : public EnginePlugin {
public:
	PLUGIN_EXPORT PointCloud();
	PLUGIN_EXPORT ~PointCloud();

	PLUGIN_EXPORT bool Init(Scene* scene) override;
	PLUGIN_EXPORT void Update(const FrameTime& frameTime) override;

private:
	CameraControl* mCameraControl;
	vector<Object*> mObjects;
	vector<Object*> mSceneRoots;

	void LoadScene(const filesystem::path& filename, float scale = .05f);

	Scene* mScene;
	float mPointSize;
	float mAnimStart;
	shared_ptr<Material> mPointMaterial;
};

ENGINE_PLUGIN(PointCloud)

PointCloud::PointCloud() : mScene(nullptr), mCameraControl(nullptr), mPointSize(0.005f), mAnimStart(0) {}
PointCloud::~PointCloud() {
	for (Object* p : mObjects)
		mScene->RemoveObject(p);
}

void PointCloud::LoadScene(const filesystem::path& filename, float scale) {
	unordered_map<uint32_t, vector<PointRenderer::Point>> points;

	const aiScene* aiscene = aiImportFile(filename.string().c_str(), aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_FlipUVs | aiProcess_MakeLeftHanded);
	if (!aiscene) return;

	queue<pair<aiNode*, Object*>> nodes;
	nodes.push(make_pair(aiscene->mRootNode, nullptr));

	while (!nodes.empty()) {
		pair<aiNode*, Object*> nodepair = nodes.front();
		nodes.pop();

		aiNode* node = nodepair.first;

		aiVector3D s;
		aiQuaternion r;
		aiVector3D t;
		node->mTransformation.Decompose(s, r, t);

		t *= scale;

		shared_ptr<Object> nodeobj = make_shared<Object>(node->mName.C_Str());
		nodeobj->Parent(nodepair.second);
		nodeobj->LocalPosition(t.x, t.y, t.z);
		nodeobj->LocalRotation(quat(r.w, r.x, r.y, r.z));
		nodeobj->LocalScale(s.x, s.y, s.z);

		mScene->AddObject(nodeobj);
		mObjects.push_back(nodeobj.get());

		if (nodepair.first == aiscene->mRootNode)
			mSceneRoots.push_back(nodeobj.get());

		for (uint32_t i = 0; i < node->mNumMeshes; i++) {
			aiMesh* aimesh = aiscene->mMeshes[node->mMeshes[i]];

			vector<PointRenderer::Point>& pts = points[node->mMeshes[i]];
			if (pts.size() != aimesh->mNumVertices) {
				pts.resize(aimesh->mNumVertices);
				for (uint32_t i = 0; i < aimesh->mNumVertices; i++) {
					memcpy(&pts[i].mPosition, &aimesh->mVertices[i], sizeof(vec3));
					if (aimesh->HasVertexColors(0))
						memcpy(&pts[i].mColor, &aimesh->mColors[0][i], sizeof(vec3));
					else
						memset(&pts[i].mColor, 1.f, sizeof(vec3));
				}
			}

			shared_ptr<PointRenderer> renderer = make_shared<PointRenderer>(node->mName.C_Str() + string(".") + aimesh->mName.C_Str());
			renderer->Parent(nodeobj.get());
			renderer->Points(pts);
			renderer->Material(mPointMaterial);

			mScene->AddObject(renderer);
			mObjects.push_back(renderer.get());
		}

		for (uint32_t i = 0; i < node->mNumChildren; i++)
			nodes.push(make_pair(node->mChildren[i], nodeobj.get()));
	}
}

bool PointCloud::Init(Scene* scene) {
	mScene = scene;

	Shader* pointShader = scene->DeviceManager()->AssetDatabase()->LoadShader("Shaders/points.shader");
	mPointMaterial = make_shared<Material>("PointCloud", pointShader);

	LoadScene("Assets/bunny.obj");
	//LoadScene("Assets/dragon.obj");
	//LoadScene("Assets/bear.obj");
	//LoadScene("Assets/island.obj");

	for (Object* o : mSceneRoots)
		o->mEnabled = false;
	mSceneRoots[0]->mEnabled = true;
	AABB aabb = mSceneRoots[0]->BoundsHeirarchy();

	mPointMaterial->SetParameter("Noise", scene->DeviceManager()->AssetDatabase()->LoadTexture("Assets/rgbanoise.png", false));
	mPointMaterial->SetParameter("Time", 0.f);
	mPointMaterial->SetParameter("PointSize", mPointSize);
	mPointMaterial->SetParameter("Extents", aabb.mExtents);

	mCameraControl = mScene->GetPlugin<CameraControl>();
	mCameraControl->CameraDistance(aabb.mExtents.y / tanf(mScene->Cameras()[0]->FieldOfView() * .5f));
	mCameraControl->CameraPivot()->LocalPosition(aabb.mCenter);

	shared_ptr<UICanvas> canvas = make_shared<UICanvas>("PointCloudUI", vec2(1, 1));

	mScene->AddObject(canvas);
	mObjects.push_back(canvas.get());

	return true;
}

void PointCloud::Update(const FrameTime& frameTime) {
	int idx = -1;

	if (idx >= 0) {
		for (Object* o : mSceneRoots)
			o->mEnabled = false;
		mSceneRoots[idx]->mEnabled = true;
		AABB aabb = mSceneRoots[idx]->BoundsHeirarchy();

		mPointMaterial->SetParameter("Extents", aabb.mExtents);
		mCameraControl->CameraDistance(1.5f * aabb.mExtents.y / tanf(mScene->Cameras()[0]->FieldOfView() * .5f));
		mCameraControl->CameraPivot()->LocalPosition(aabb.mCenter);
		mAnimStart = frameTime.mTotalTime;
	}

	mPointMaterial->SetParameter("Time", .4f * (frameTime.mTotalTime - mAnimStart));
}