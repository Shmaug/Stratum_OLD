#include <Core/EnginePlugin.hpp>
#include <iterator>
#include <filesystem>

#include <Interface/UICanvas.hpp>
#include <Interface/VerticalLayout.hpp>
#include <Interface/TextButton.hpp>
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

	void SwitchScene(Object* s);
	Object* LoadScene(const filesystem::path& filename, float scale = .05f);

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

Object* PointCloud::LoadScene(const filesystem::path& filename, float scale) {
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
		nodeobj->Parent(nodepair.second);
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
			renderer->Parent(nodeobj.get());
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

	/*
	shared_ptr<UICanvas> canvas = make_shared<UICanvas>("PointCloudUI", vec2(1, 1));
	shared_ptr<VerticalLayout> layout = make_shared<VerticalLayout>("Layout");
	canvas->AddElement(layout);
	layout->Extent(1.f, 1.f, 0.f, 0.f);

	mScene->AddObject(canvas);
	mObjects.push_back(canvas.get());

	Shader* fontshader = scene->DeviceManager()->AssetDatabase()->LoadShader("Shaders/font.shader");
	Font* font = scene->DeviceManager()->AssetDatabase()->LoadFont("Assets/segoeui.ttf", 24);
	shared_ptr<Material> fontMat = make_shared<Material>("Segoe UI", fontshader);
	fontMat->SetParameter("MainTexture", font->Texture());

	vector<string> datasets {
		"Assets/bunny.obj",
		//"Assets/dragon.obj",
		//"Assets/bear.obj"
	};

	for (const string& c : datasets) {
		printf("Loading %s ... ", c.c_str());
		Object* o = LoadScene(c);
		if (!o) { printf("Failed!\n"); continue; }
		printf("Done.\n");
		mSceneRoots.push_back(o);
		
		shared_ptr<TextButton> btn = make_shared<TextButton>("Button");
		canvas->AddElement(btn);
		btn->Font(font);
		btn->Material(fontMat);
		btn->Extent(1, 0, 0, .1f);
		btn->Text(c);
		layout->AddChild(btn.get());

		printf("<%.2f,%.2f>\t\<2f,%.2f,%.2f>\n", btn->AbsoluteExtent().x, btn->AbsoluteExtent().y, btn->AbsolutePosition().x, btn->AbsolutePosition().y, btn->AbsolutePosition().z);
	}
	
	SwitchScene(mSceneRoots[0]);
	*/

	return true;
}

void PointCloud::Update(const FrameTime& frameTime) {
	int idx = -1;

	if (idx >= 0 && idx < mSceneRoots.size()) {
		SwitchScene(mSceneRoots[idx]);
		mAnimStart = frameTime.mTotalTime;
	}

	mPointMaterial->SetParameter("Time", .4f * (frameTime.mTotalTime - mAnimStart));
}