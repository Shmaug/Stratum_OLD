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

#ifdef __GNUC__
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif

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

	Object* LoadScene(const fs::path& filename, Shader* shader, float scale = .05f);
}; 

ENGINE_PLUGIN(MeshViewer)

const ::VertexInput CubeVertexInput {
	{
		0, // binding
		sizeof(float3), // stride
		VK_VERTEX_INPUT_RATE_VERTEX // inputRate
	},
	{
		{
			0, // location
			0, // binding
			VK_FORMAT_R32G32B32_SFLOAT, // format
			0 // offset
		}
	}
};

MeshViewer::MeshViewer() : mScene(nullptr) {
	mEnabled = true;
}
MeshViewer::~MeshViewer() {
	for (Object* obj : mObjects)
		mScene->RemoveObject(obj);
}

Object* MeshViewer::LoadScene(const fs::path& filename, Shader* shader, float scale) {
	Texture* brdfTexture  = mScene->AssetManager()->LoadTexture("Assets/BrdfLut.png", false);
	Texture* whiteTexture = mScene->AssetManager()->LoadTexture("Assets/white.png");
	Texture* bumpTexture = mScene->AssetManager()->LoadTexture("Assets/bump.png", false);
	Texture* environment  = mScene->AssetManager()->LoadTexture("Assets/daytime.hdr", false);

	unordered_map<uint32_t, shared_ptr<Mesh>> meshes;
	unordered_map<uint32_t, shared_ptr<Material>> materials;

	const aiScene* aiscene = aiImportFile(filename.string().c_str(), aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_FlipUVs  | aiProcess_MakeLeftHanded);
	if (!aiscene) return nullptr;

	queue<pair<aiNode*, Object*>> nodes;
	nodes.push(make_pair(aiscene->mRootNode, nullptr));
	Object* root = nullptr;

	uint32_t renderQueueOffset = 0;

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
		nodeobj->LocalRotation(quaternion(r.x, r.y, r.z, r.w));
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
				bool twoSided;
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
				material->SetParameter("Color", float4(color.r, color.g, color.b, 1));
				material->SetParameter("Metallic", metallic);
				material->SetParameter("Roughness", roughness);
				material->SetParameter("EnvironmentTexture", environment);
				if (twoSided) material->EnableKeyword("TWO_SIDED");

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

				material->RenderQueue(material->RenderQueue() + renderQueueOffset);
				renderQueueOffset++;
			}

			shared_ptr<Mesh>& mesh = meshes[node->mMeshes[i]];
			if (!mesh) mesh = make_shared<Mesh>(aimesh->mName.C_Str(), mScene->DeviceManager(), aimesh, scale);

			shared_ptr<MeshRenderer> meshRenderer = make_shared<MeshRenderer>(node->mName.C_Str() + string(".") + aimesh->mName.C_Str());
			meshRenderer->Parent(nodeobj.get());
			meshRenderer->Mesh(mesh);
			meshRenderer->Material(material);

			mScene->AddObject(meshRenderer);
			mObjects.push_back(meshRenderer.get());
		}

		for (uint32_t i = 0; i < node->mNumChildren; i++)
			nodes.push(make_pair(node->mChildren[i], nodeobj.get()));
	}

	return root;
}

bool MeshViewer::Init(Scene* scene) {
	mScene = scene;

	float3 cubeVerts[8] {
		float3(-1, -1, -1),
		float3( 1, -1, -1),
		float3(-1, -1,  1),
		float3( 1, -1,  1),
		float3(-1,  1, -1),
		float3( 1,  1, -1),
		float3(-1,  1,  1),
		float3( 1,  1,  1),
	};
	uint16_t cubeIndices[36]{
		2,7,6,2,3,7,
		0,1,2,2,1,3,
		1,5,7,7,3,1,
		4,5,1,4,1,0,
		6,4,2,4,0,2,
		4,7,5,4,6,7
	};
	shared_ptr<Material> skyboxMat = make_shared<Material>("Skybox", mScene->AssetManager()->LoadShader("Shaders/skybox.shader"));
	skyboxMat->SetParameter("EnvironmentTexture", mScene->AssetManager()->LoadTexture("Assets/daytime.hdr", false));
	shared_ptr<MeshRenderer> skybox = make_shared<MeshRenderer>("SkyCube");
	skybox->Mesh(make_shared<Mesh>("Cube", scene->DeviceManager(), cubeVerts, cubeIndices, 8, sizeof(float3), 36, &CubeVertexInput, VK_INDEX_TYPE_UINT16));
	skybox->Material(skyboxMat);

	mObjects.push_back(skybox.get());
	mScene->AddObject(skybox);

	Shader* pbrshader  = mScene->AssetManager()->LoadShader("Shaders/pbr.shader");
	Font* font = mScene->AssetManager()->LoadFont("Assets/segoeui.ttf", 24);
	shared_ptr<Material> fontMat = make_shared<Material>("Segoe UI", mScene->AssetManager()->LoadShader("Shaders/font.shader"));
	fontMat->SetParameter("MainTexture", font->Texture());

	shared_ptr<UICanvas> canvas = make_shared<UICanvas>("MeshViewerPanel", float2(.1f, 1.f));
	shared_ptr<VerticalLayout> layout = make_shared<VerticalLayout>("Layout");
	canvas->AddElement(layout);
	//mScene->AddObject(canvas);
	//mObjects.push_back(canvas.get());

	layout->Extent(1.f, 1.f, 0.f, 0.f);

	vector<fs::path> datasets{
		"Assets/bunny.obj",
		"Assets/bear.obj",
		"Assets/dragon.obj",
	};

	float x = -(float)datasets.size() * .5f;

	for (const fs::path& c : datasets) {
		printf("Loading %s ... ", c.string().c_str());
		Object* o = LoadScene(c, pbrshader);
		if (!o) { printf("Failed!\n"); continue; }
		printf("Done.\n");
		mSceneRoots.push_back(o);

		float mx = fmaxf(fmaxf(o->BoundsHeirarchy().mExtents.x, o->BoundsHeirarchy().mExtents.y), o->BoundsHeirarchy().mExtents.z);
		o->LocalScale(1.f / mx);

		o->LocalPosition(x * 1.5f, 0, 0);
		x += 1.f;

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

	shared_ptr<Light> light0 = make_shared<Light>("Sun");
	light0->Type(Sun);
	light0->Intensity(1.5f);
	light0->Color(float3(1.f, 1.f, .95f));
	light0->LocalRotation(quaternion(radians(float3(75.f, 45.f, 0))));
	mObjects.push_back(light0.get());
	mScene->AddObject(light0);

	SwitchScene(mSceneRoots[0]);

	return true;
}