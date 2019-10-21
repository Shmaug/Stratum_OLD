#include <Core/EnginePlugin.hpp>
#include <iterator>

#ifdef __GNUC__
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif

#include <Interface/UICanvas.hpp>
#include <Interface/VerticalLayout.hpp>
#include <Interface/TextButton.hpp>
#include <Util/Profiler.hpp>

#include <assimp/scene.h>
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include <assimp/pbrmaterial.h>

#include <Scene/Scene.hpp>
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

	void SwitchScene(Object* s);
	Object* LoadScene(const fs::path& filename, float scale = .05f);

	Scene* mScene;
	float mPointSize;
	float mAnimStart;
	shared_ptr<Material> mPointMaterial;
};

ENGINE_PLUGIN(PointCloud)

PointCloud::PointCloud() : mScene(nullptr), mCameraControl(nullptr), mPointSize(0.005f), mAnimStart(0) {
	mEnabled = true;
}
PointCloud::~PointCloud() {
	for (Object* p : mObjects)
		mScene->RemoveObject(p);
}

Object* PointCloud::LoadScene(const fs::path& filename, float scale) {
	unordered_map<uint32_t, vector<PointRenderer::Point>> points;

	const aiScene* aiscene = aiImportFile(filename.string().c_str(), aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_FlipUVs | aiProcess_MakeLeftHanded);
	if (!aiscene) return nullptr;

	Object* root = nullptr;

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
		nodepair.second->AddChild(nodeobj.get());
		nodeobj->LocalPosition(t.x, t.y, t.z);
		nodeobj->LocalRotation(quaternion(r.x, r.y, r.z, r.w));
		nodeobj->LocalScale(s.x, s.y, s.z);

		mScene->AddObject(nodeobj);
		mObjects.push_back(nodeobj.get());

		if (nodepair.first == aiscene->mRootNode)
			root = nodeobj.get();

		for (uint32_t i = 0; i < node->mNumMeshes; i++) {
			aiMesh* aimesh = aiscene->mMeshes[node->mMeshes[i]];

			vector<PointRenderer::Point>& pts = points[node->mMeshes[i]];
			if (pts.size() != aimesh->mNumVertices) {
				pts.resize(aimesh->mNumVertices);
				for (uint32_t i = 0; i < aimesh->mNumVertices; i++) {
					memcpy(&pts[i].mPosition, &aimesh->mVertices[i], sizeof(float3));
					if (aimesh->HasVertexColors(0))
						memcpy(&pts[i].mColor, &aimesh->mColors[0][i], sizeof(float3));
					else
						pts[i].mColor = 1;
				}
			}

			shared_ptr<PointRenderer> renderer = make_shared<PointRenderer>(node->mName.C_Str() + string(".") + aimesh->mName.C_Str());
			nodeobj->AddChild(renderer.get());
			renderer->Points(pts);
			renderer->Material(mPointMaterial);

			mScene->AddObject(renderer);
			mObjects.push_back(renderer.get());
		}

		for (uint32_t i = 0; i < node->mNumChildren; i++)
			nodes.push(make_pair(node->mChildren[i], nodeobj.get()));
	}
	
	return root;
}
void PointCloud::SwitchScene(Object* s) {
	for (Object* o : mSceneRoots)
		o->mEnabled = (o == s);
	AABB aabb = s->BoundsHeirarchy();
	mPointMaterial->SetParameter("Extents", aabb.mExtents);
	mCameraControl = mScene->PluginManager()->GetPlugin<CameraControl>();
	mCameraControl->CameraDistance(aabb.mExtents.y / tanf(mScene->Cameras()[0]->FieldOfView() * .5f));
	mCameraControl->CameraPivot()->LocalPosition(aabb.mCenter);
}

bool PointCloud::Init(Scene* scene) {
	mScene = scene;

	Shader* pointShader = mScene->AssetManager()->LoadShader("Shaders/points.shader");
	mPointMaterial = make_shared<Material>("PointCloud", pointShader);
	mPointMaterial->SetParameter("Noise", mScene->AssetManager()->LoadTexture("Assets/rgbanoise.png", false));
	mPointMaterial->SetParameter("Time", 0.f);
	mPointMaterial->SetParameter("PointSize", mPointSize);

	vector<string> datasets {
		"Assets/bunny.obj",
		"Assets/dragon.obj",
		"Assets/bear.obj"
	};

	for (const string& c : datasets) {
		printf("Loading %s ... ", c.c_str());
		Object* o = LoadScene(c);
		if (!o) { printf("Failed!\n"); continue; }
		printf("Done.\n");
		mSceneRoots.push_back(o);
		
	}
	
	SwitchScene(mSceneRoots[0]);

	return true;
}

void PointCloud::Update(const FrameTime& frameTime) {
	int idx = -1;

	auto input = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

	if (input->KeyDownFirst(GLFW_KEY_1)) idx = 0;
	if (input->KeyDownFirst(GLFW_KEY_2)) idx = 1;
	if (input->KeyDownFirst(GLFW_KEY_3)) idx = 2;

	if (idx >= 0 && idx < mSceneRoots.size()) {
		SwitchScene(mSceneRoots[idx]);
		mAnimStart = frameTime.mTotalTime;
	}

	mPointMaterial->SetParameter("Time", .4f * (frameTime.mTotalTime - mAnimStart));
}