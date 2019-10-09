#include <Core/EnginePlugin.hpp>
#include <thread>

#include <Scene/MeshRenderer.hpp>
#include <Scene/TextRenderer.hpp>
#include <Interface/UICanvas.hpp>
#include <Interface/TextButton.hpp>
#include <Interface/VerticalLayout.hpp>
#include <Util/Profiler.hpp>

#include <Plugins/CameraControl/CameraControl.hpp>

#include <assimp/scene.h>
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include <assimp/pbrmaterial.h>

#include <filesystem>

using namespace std;

class MeshViewer : public EnginePlugin {
public:
	PLUGIN_EXPORT MeshViewer();
	PLUGIN_EXPORT ~MeshViewer();

	PLUGIN_EXPORT bool Init(Scene* scene) override;

private:
	vector<Object*> mObjects;
	vector<Object*> mSceneRoots;
	Scene* mScene;

	Object* LoadScene(const filesystem::path& filename, Shader* shader, float scale = .05f);
	void SwitchScene(Object* s);
}; 

ENGINE_PLUGIN(MeshViewer)

MeshViewer::MeshViewer() : mScene(nullptr) {}
MeshViewer::~MeshViewer() {
	for (Object* obj : mObjects)
		mScene->RemoveObject(obj);
}

Object* MeshViewer::LoadScene(const filesystem::path& filename, Shader* shader, float scale) {
	Texture* brdfTexture  = mScene->AssetManager()->LoadTexture("Assets/BrdfLut.png", false);
	Texture* whiteTexture = mScene->AssetManager()->LoadTexture("Assets/white.png");
	Texture* bumpTexture  = mScene->AssetManager()->LoadTexture("Assets/bump.png", false);

	unordered_map<uint32_t, shared_ptr<Mesh>> meshes;
	unordered_map<uint32_t, shared_ptr<Material>> materials;

	const aiScene* aiscene = aiImportFile(filename.string().c_str(), aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_FlipUVs  | aiProcess_MakeLeftHanded);
	if (!aiscene) return nullptr;

	queue<pair<aiNode*, Object*>> nodes;
	nodes.push(make_pair(aiscene->mRootNode, nullptr));
	Object* root = nullptr;

	while(!nodes.empty()) {
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
			root = nodeobj.get();

		for (uint32_t i = 0; i < node->mNumMeshes; i++) {
			aiMesh* aimesh = aiscene->mMeshes[node->mMeshes[i]];
			aiMaterial* aimat = aiscene->mMaterials[aimesh->mMaterialIndex];

			shared_ptr<Material>& material = materials[aimesh->mMaterialIndex];
			if (!material) {
				VkCullModeFlags cullMode = VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
				int twoSided;
				aiColor3D color;
				aiString matname;
				float metallic, roughness;
				aimat->Get(AI_MATKEY_NAME, matname);
				if (aimat->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_FACTOR, color) != aiReturn::aiReturn_SUCCESS) color.r = color.g = color.b = 1;
				if (aimat->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, metallic) != aiReturn::aiReturn_SUCCESS) metallic = 0;
				if (aimat->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, roughness) != aiReturn::aiReturn_SUCCESS) roughness = .8f;
				if (aimat->Get(AI_MATKEY_TWOSIDED, twoSided) == aiReturn::aiReturn_SUCCESS && twoSided) cullMode = VK_CULL_MODE_NONE;

				aiString diffuse, normal, metalroughness;
				aimat->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_TEXTURE, &diffuse);
				aimat->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &metalroughness);
				aimat->GetTexture(aiTextureType_NORMALS, 0, &normal);

				material = make_shared<Material>(matname.C_Str(), shader);
				material->CullMode(cullMode);
				material->SetParameter("BrdfTexture", brdfTexture);
				material->SetParameter("Color", vec4(color.r, color.g, color.b, 1.f));
				material->SetParameter("Metallic", metallic);
				material->SetParameter("Roughness", roughness);

				if (diffuse == aiString("") || diffuse.C_Str()[0] == '*')
					material->SetParameter("MainTexture", whiteTexture);
				else {
					if (filename.has_parent_path())
						diffuse = filename.parent_path().string() + "/" + diffuse.C_Str();
					material->SetParameter("MainTexture", mScene->AssetManager()->LoadTexture(diffuse.C_Str()));
				}

				if (normal == aiString("") || normal.C_Str()[0] == '*')
					material->SetParameter("NormalTexture", bumpTexture);
				else {
					if (filename.has_parent_path())
						normal = filename.parent_path().string() + "/" + normal.C_Str();
					material->SetParameter("NormalTexture", mScene->AssetManager()->LoadTexture(normal.C_Str(), false));
					material->EnableKeyword("NORMAL_MAP");
				}
			}

			shared_ptr<Mesh>& mesh = meshes[node->mMeshes[i]];
			if (!mesh) mesh = make_shared<Mesh>(aimesh->mName.C_Str(), mScene->DeviceManager(), aimesh, scale);

			shared_ptr<MeshRenderer> meshRenderer = make_shared<MeshRenderer>(node->mName.C_Str() + string(".") + aimesh->mName.C_Str());
			meshRenderer->Parent(nodeobj.get());
			meshRenderer->Mesh(mesh);
			meshRenderer->Material(material);

			mScene->AddObject(meshRenderer);
			mObjects.push_back(meshRenderer.get());

			//printf("%s: <%.2f, %.2f, %.2f> [%.2f, %.2f, %.2f]\n", node->mName.C_Str(), meshRenderer->Bounds().mCenter.x, meshRenderer->Bounds().mCenter.y, meshRenderer->Bounds().mCenter.z, meshRenderer->Bounds().mExtents.x, meshRenderer->Bounds().mExtents.y, meshRenderer->Bounds().mExtents.z);
		}

		for (uint32_t i = 0; i < node->mNumChildren; i++)
			nodes.push(make_pair(node->mChildren[i], nodeobj.get()));
	}

	return root;
}
void MeshViewer::SwitchScene(Object* s) {
	for (Object* o : mSceneRoots)
		o->mEnabled = (o == s);
	AABB aabb = s->BoundsHeirarchy();
	auto cameraControl = mScene->PluginManager()->GetPlugin<CameraControl>();
	cameraControl->CameraDistance(aabb.mExtents.y / tanf(mScene->Cameras()[0]->FieldOfView() * .5f));
	cameraControl->CameraPivot()->LocalPosition(aabb.mCenter);
}

bool MeshViewer::Init(Scene* scene) {
	mScene = scene;

	Shader* pbrshader  = mScene->AssetManager()->LoadShader("Shaders/pbr.shader");
	Shader* fontshader = mScene->AssetManager()->LoadShader("Shaders/font.shader");
	Font* font = mScene->AssetManager()->LoadFont("Assets/segoeui.ttf", 24);
	shared_ptr<Material> fontMat = make_shared<Material>("Segoe UI", fontshader);
	fontMat->SetParameter("MainTexture", font->Texture());

	shared_ptr<UICanvas> canvas = make_shared<UICanvas>("MeshViewerPanel", vec2(.1f, 1.f));
	shared_ptr<VerticalLayout> layout = make_shared<VerticalLayout>("Layout");
	canvas->AddElement(layout);
	mScene->AddObject(canvas);
	mObjects.push_back(canvas.get());

	layout->Extent(1.f, 1.f, 0.f, 0.f);

	vector<filesystem::path> datasets {
		"Assets/bunny.obj",
		"Assets/dragon.obj",
		"Assets/bear.obj"
	};

	for (const auto& c : datasets) {
		printf("Loading %s ... ", c.string().c_str());
		Object* o = LoadScene(c, pbrshader);
		if (!o) { printf("Failed!\n"); continue; }
		printf("Done.\n");
		mSceneRoots.push_back(o);

		float mx = gmax(gmax(o->BoundsHeirarchy().mExtents.x, o->BoundsHeirarchy().mExtents.y), o->BoundsHeirarchy().mExtents.z);
		o->LocalScale(1.f / mx);

		shared_ptr<TextButton> btn = make_shared<TextButton>("Button");
		canvas->AddElement(btn);
		layout->AddChild(btn.get());
		btn->Font(font);
		btn->Material(fontMat);
		btn->Extent(1, 1.f / datasets.size(), 0, 0);
		btn->Text(c.filename().string());
		btn->TextScale(btn->AbsoluteExtent().y * .3f);
	}
	layout->UpdateLayout();
	canvas->LocalPosition(-1.f, .5f, 0.f);

	SwitchScene(mSceneRoots[0]);

	return true;
}