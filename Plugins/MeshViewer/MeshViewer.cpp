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

	Object* LoadScene(const fs::path& filename, Shader* shader, Texture* environment, float scale = .05f, const float3& col = float3(1, 1, 1), float metal = 0, float rough = 1, float enevStrength = 1);
	Object* LoadObj(const fs::path& filename, Shader* shader, Texture* environment, float scale = .05f, const float3& col = float3(1,1,1), float metal=0, float rough=1, float enevStrength=1);
}; 

ENGINE_PLUGIN(MeshViewer)

#pragma pack(push)
#pragma pack(1)
struct objvertex {
	float3 position;
	float3 normal;
};
#pragma pack(pop)

const ::VertexInput ObjInput {
	{
		0, // binding
		sizeof(objvertex), // stride
		VK_VERTEX_INPUT_RATE_VERTEX // inputRate
	},
	{
		{ // Position
			0, // location
			0, // binding
			VK_FORMAT_R32G32B32_SFLOAT, // format
			0 // offset
		},
		{ // Normal
			1, // location
			0, // binding
			VK_FORMAT_R32G32B32_SFLOAT, // format
			sizeof(float3) // offset
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

Object* MeshViewer::LoadScene(const fs::path& filename, Shader* shader, Texture* environment, float scale, const float3& col, float metal, float rough, float enevStrength) {
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

			if ((aimesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE) == 0) continue;

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
				if (aimat->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, roughness) != aiReturn::aiReturn_SUCCESS) roughness = 1;
				if (aimat->Get(AI_MATKEY_TWOSIDED, twoSided) == aiReturn::aiReturn_SUCCESS && twoSided) cullMode = VK_CULL_MODE_NONE;

				aiString diffuse, normal, metalroughness;
				aimat->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_TEXTURE, &diffuse);
				aimat->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &metalroughness);
				aimat->GetTexture(aiTextureType_NORMALS, 0, &normal);

				material = make_shared<Material>(matname.C_Str(), shader);
				material->CullMode(cullMode);
				material->SetParameter("BrdfTexture", brdfTexture);
				material->SetParameter("Color", float4(color.r * col.r, color.g * col.g, color.b * col.b, 1));
				material->SetParameter("Metallic", metallic * metal);
				material->SetParameter("Roughness", roughness * rough);
				material->SetParameter("EnvironmentTexture", environment);
				material->SetParameter("EnvironmentStrength", enevStrength);
				if (twoSided) material->EnableKeyword("TWO_SIDED");

				if (diffuse != aiString("") && diffuse.C_Str()[0] != '*') {
					if (filename.has_parent_path())
						diffuse = filename.parent_path().string() + "/" + diffuse.C_Str();
					material->SetParameter("MainTexture", mScene->AssetManager()->LoadTexture(diffuse.C_Str()));
					material->EnableKeyword("COLOR_MAP");
				}

				if (normal != aiString("") && normal.C_Str()[0] != '*'){
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
Object* MeshViewer::LoadObj(const fs::path& filename, Shader* shader, Texture* environment, float scale, const float3& col, float metal, float rough, float enevStrength) {
	shared_ptr<Material> mat = make_shared<Material>("Ground", shader);
	mat->SetParameter("EnvironmentTexture", environment);
	mat->SetParameter("EnvironmentStrength", enevStrength);
	mat->SetParameter("BrdfTexture", mScene->AssetManager()->LoadTexture("Assets/BrdfLut.png", false));
	mat->SetParameter("Color", col);
	mat->SetParameter("Metallic", metal);
	mat->SetParameter("Roughness", rough);
	shared_ptr<MeshRenderer> obj = make_shared<MeshRenderer>(filename.string());
	obj->Material(mat);

	vector<objvertex> vertices;
	vector<uint32_t> indices;
	uint32_t a = 0;

	objvertex cur;

	ifstream file(filename);

	string line;
	while (getline(file, line)){
		if (line[0] == 'v') {
			if (line[1] == 'n') {
				uint32_t i = 3;
				size_t idx;
				cur.normal.x = stof(string(line.data() + i), &idx);
				i += idx + 1;
				cur.normal.y = stof(string(line.data() + i), &idx);
				i += idx + 1;
				cur.normal.z = stof(string(line.data() + i), &idx);
				i += idx + 1;
				a++;
			} else {
				uint32_t i = 2;
				size_t idx;
				cur.position.x = stof(string(line.data() + i), &idx);
				i += idx + 1;
				cur.position.y = stof(string(line.data() + i), &idx);
				i += idx + 1;
				cur.position.z = stof(string(line.data() + i), &idx);
				a++;
			}
			if (a == 2) {
				vertices.push_back(cur);
				memset(&cur, 0, sizeof(objvertex));
				a = 0;
			}
		} else {
			if (line[0] == 'f') {
				uint32_t i = 2;
				size_t idx;
				uint32_t i0 = stoi(string(line.data() + i), &idx); i += idx + 2;
				stoi(string(line.data() + i), &idx); i += idx + 1;
				uint32_t i1 = stoi(string(line.data() + i), &idx); i += idx + 2;
				stoi(string(line.data() + i), &idx); i += idx + 1;
				uint32_t i2 = stoi(string(line.data() + i), &idx);

				indices.push_back(i0);
				indices.push_back(i1);
				indices.push_back(i2);
			}
		}

	}

	obj->Mesh(make_shared<Mesh>(filename.string(), mScene->DeviceManager(), vertices.data(), indices.data(), vertices.size(), sizeof(objvertex), indices.size(), &ObjInput, VK_INDEX_TYPE_UINT32));

	mObjects.push_back(obj.get());
	mScene->AddObject(obj);
	return obj.get();
}

bool MeshViewer::Init(Scene* scene) {
	mScene = scene;

	Texture* brdfTexture = mScene->AssetManager()->LoadTexture("Assets/BrdfLut.png", false);
	Texture* whiteTexture = mScene->AssetManager()->LoadTexture("Assets/white.png");
	Texture* bumpTexture = mScene->AssetManager()->LoadTexture("Assets/bump.png", false);
	Texture* envTexture = mScene->AssetManager()->LoadTexture("Assets/daytime.hdr", false);

	Shader* pbrshader  = mScene->AssetManager()->LoadShader("Shaders/pbr.shader");
	Font* font = mScene->AssetManager()->LoadFont("Assets/segoeui.ttf", 24);
	shared_ptr<Material> fontMat = make_shared<Material>("Segoe UI", mScene->AssetManager()->LoadShader("Shaders/font.shader"));
	fontMat->SetParameter("MainTexture", font->Texture());

	float enevStrength = .5f;

	shared_ptr<Material> skyboxMat = make_shared<Material>("Skybox", mScene->AssetManager()->LoadShader("Shaders/skybox.shader"));
	skyboxMat->SetParameter("EnvironmentTexture", envTexture);
	shared_ptr<MeshRenderer> skybox = make_shared<MeshRenderer>("SkyCube");
	skybox->Mesh(shared_ptr<Mesh>(Mesh::CreateCube("Cube", mScene->DeviceManager())));
	skybox->Material(skyboxMat);
	mObjects.push_back(skybox.get());
	mScene->AddObject(skybox);

	shared_ptr<Material> groundMat = make_shared<Material>("Ground", pbrshader);
	groundMat->SetParameter("EnvironmentTexture", envTexture);
	groundMat->SetParameter("EnvironmentStrength", enevStrength);
	groundMat->SetParameter("MainTexture", whiteTexture);
	groundMat->SetParameter("NormalTexture", bumpTexture);
	groundMat->SetParameter("BrdfTexture", brdfTexture);
	groundMat->SetParameter("Color", float4(.9f, .9f, .9f, 1));
	groundMat->SetParameter("Metallic", 0.f);
	groundMat->SetParameter("Roughness", .8f);
	shared_ptr<MeshRenderer> ground = make_shared<MeshRenderer>("Ground");
	ground->Mesh(shared_ptr<Mesh>(Mesh::CreatePlane("Ground", mScene->DeviceManager(), 10.f)));
	ground->Material(groundMat);
	ground->LocalRotation(quaternion(float3(-PI * .5f, 0, 0)));
	mObjects.push_back(ground.get());
	mScene->AddObject(ground);

	shared_ptr<UICanvas> canvas = make_shared<UICanvas>("MeshViewerPanel", float2(.1f, 1.f));
	shared_ptr<VerticalLayout> layout = make_shared<VerticalLayout>("Layout");
	canvas->AddElement(layout);

	layout->Extent(1.f, 1.f, 0.f, 0.f);

	struct model {
		fs::path file;
		float3 color;
		float metallic;
		float roughness;
	};

	vector<model> models {
		 { "Assets/bunny.obj", float3(.6f, .2f, .25f), 0.f, .95f },
		 { "Assets/bear.obj", float3(.6f,.6f,.5f), 1.f, .6f },
		 { "Assets/dragon.obj", float3(.4f, .6f, .5f), 1.f, .3f }
	};

	float x = -(float)models.size() * .5f;

	for (const model& m : models) {
		printf("Loading %s ... ", m.file.string().c_str());
		Object* o = LoadObj(m.file, pbrshader, envTexture, 1, m.color, m.metallic, m.roughness);
		if (!o) { printf("Failed!\n"); continue; }
		printf("Done.\n");
		mSceneRoots.push_back(o);

		AABB aabb = o->BoundsHeirarchy();
		float mx = fmaxf(fmaxf(aabb.mExtents.x, aabb.mExtents.y), aabb.mExtents.z);
		o->LocalScale(.5f / mx);

		float3 offset = o->WorldPosition() - aabb.mCenter * .5f / mx;
		offset.x += x;
		offset.y += aabb.mExtents.y * .5f / mx;
		o->LocalPosition(offset);
		x += 1.f;

		shared_ptr<TextButton> btn = make_shared<TextButton>("Button");
		canvas->AddElement(btn);
		layout->AddChild(btn.get());
		btn->Font(font);
		btn->Material(fontMat);
		btn->Extent(1, 1.f / models.size(), 0, 0);
		btn->Text(m.file.string());
		btn->TextScale(btn->AbsoluteExtent().y * .3f);
	}
	layout->UpdateLayout();
	canvas->LocalPosition(-1.f, .5f, 0.f);

	shared_ptr<Light> light0 = make_shared<Light>("Sun");
	light0->Type(Spot);
	light0->Intensity(10.f);
	light0->InnerSpotAngle(radians(10.f));
	light0->OuterSpotAngle(radians(15.f));
	light0->Range(10.f);
	light0->Color(float3(1.f, .7f, .5f));
	light0->LocalRotation(quaternion(radians(float3(10.f, 45.f, 0))));
	light0->LocalPosition(light0->LocalRotation()* float3(0, 0, -2));
	mObjects.push_back(light0.get());
	mScene->AddObject(light0);

	shared_ptr<Light> light1 = make_shared<Light>("Point");
	light1->Type(Point);
	light1->Intensity(.8f);
	light1->Range(5.f);
	light1->LocalPosition(-1.5f, 1, -.5f);
	light1->Color(float3(.2f, 1.f, 1.f));
	mObjects.push_back(light1.get());
	mScene->AddObject(light1);

	//mScene->AddObject(canvas);
	//mObjects.push_back(canvas.get());

	return true;
}