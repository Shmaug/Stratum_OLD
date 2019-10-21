#include <Core/EnginePlugin.hpp>
#include <thread>
#include <unordered_map>

#include <Scene/Scene.hpp>
#include <Scene/MeshRenderer.hpp>
#include <Scene/TextRenderer.hpp>
#include <Interface/UICanvas.hpp>
#include <Interface/UIImage.hpp>
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
	float mEnvironmentStrength;
	Texture* mEnvironmentTexture;

	vector<Object*> mObjects;
	unordered_map<string, Object*> mLoaded;
	vector<shared_ptr<Material>> mMaterials;
	Scene* mScene;

	vector<TextButton*> mSceneButtons;

	Object* LoadScene(const string& filename, Shader* shader, float scale = .05f);
	Object* LoadObj(const string& filename, Shader* shader, float scale = .05f, const float4& col = float4(1), float metal = 0, float rough = 1);
};

ENGINE_PLUGIN(MeshViewer)


MeshViewer::MeshViewer() : mScene(nullptr), mEnvironmentStrength(1.f), mEnvironmentTexture(nullptr) {
	mEnabled = true;
}
MeshViewer::~MeshViewer() {
	for (Object* obj : mObjects)
		mScene->RemoveObject(obj);
}

Object* MeshViewer::LoadScene(const string& filename, Shader* shader, float scale) {
	if (mLoaded.count(filename)) return mLoaded.at(filename);

	fs::path path(filename);
	
	Texture* brdfTexture  = mScene->AssetManager()->LoadTexture("Assets/BrdfLut.png", false);
	Texture* whiteTexture = mScene->AssetManager()->LoadTexture("Assets/white.png");
	Texture* bumpTexture  = mScene->AssetManager()->LoadTexture("Assets/bump.png", false);

	unordered_map<uint32_t, shared_ptr<Mesh>> meshes;
	unordered_map<uint32_t, shared_ptr<Material>> materials;

	const aiScene* aiscene = aiImportFile(filename.c_str(), aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_FlipUVs  | aiProcess_MakeLeftHanded);
	if (!aiscene) {
		cerr << "Failed to open " << filename <<  ": " << aiGetErrorString() << endl;
		return nullptr;
	}

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
				BlendMode blendMode = BLEND_MODE_MAX_ENUM;
				bool twoSided = false;
				aiColor3D emissionFac;
				aiColor4D color;
				aiString matname, diffuse, normal, metalroughness, emission;;
				aiString alphaMode;
				float metallic, roughness;
				aimat->Get(AI_MATKEY_NAME, matname);
				if (aimat->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_FACTOR, color) != aiReturn::aiReturn_SUCCESS)
					color.r = color.g = color.b = color.a = 1;
				if (aimat->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, metallic) != aiReturn::aiReturn_SUCCESS) metallic = 0.f;
				if (aimat->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, roughness) != aiReturn::aiReturn_SUCCESS) roughness = .3f;
				if (aimat->Get(AI_MATKEY_TWOSIDED, twoSided) == aiReturn::aiReturn_SUCCESS && twoSided) cullMode = VK_CULL_MODE_NONE;
				if (aimat->Get(AI_MATKEY_COLOR_EMISSIVE, emissionFac) != aiReturn::aiReturn_SUCCESS) emissionFac.r = emissionFac.g = emissionFac.b = 0;
				if (aimat->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == aiReturn::aiReturn_SUCCESS && strcmp(alphaMode.C_Str(), "BLEND") == 0) blendMode = Alpha;

				aimat->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_TEXTURE, &diffuse);
				aimat->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &metalroughness);
				aimat->GetTexture(aiTextureType_NORMALS, 0, &normal);
				aimat->GetTexture(aiTextureType_EMISSIVE, 0, &emission);

				material = make_shared<Material>(matname.C_Str(), shader);
				material->CullMode(cullMode);
				material->BlendMode(blendMode);
				material->SetParameter("BrdfTexture", brdfTexture);
				material->SetParameter("Color", float4(color.r, color.g, color.b, color.a));
				material->SetParameter("Metallic", metallic);
				material->SetParameter("Roughness", roughness);
				material->SetParameter("EnvironmentTexture", mEnvironmentTexture);
				material->SetParameter("EnvironmentStrength", mEnvironmentStrength);
				if (twoSided) material->EnableKeyword("TWO_SIDED");

				if (diffuse != aiString("") && diffuse.C_Str()[0] != '*') {
					if (path.has_parent_path())
						diffuse = path.parent_path().string() + "/" + diffuse.C_Str();
					material->EnableKeyword("COLOR_MAP");
					material->SetParameter("MainTexture", mScene->AssetManager()->LoadTexture(diffuse.C_Str()));
				}
				if (normal != aiString("") && normal.C_Str()[0] != '*'){
					if (path.has_parent_path())
						normal = path.parent_path().string() + "/" + normal.C_Str();
					material->EnableKeyword("NORMAL_MAP");
					material->SetParameter("NormalTexture", mScene->AssetManager()->LoadTexture(normal.C_Str(), false));
				}
				if (emission != aiString("") && emission.C_Str()[0] != '*') {
					if (path.has_parent_path())
						emission = path.parent_path().string() + "/" + emission.C_Str();
					material->EnableKeyword("EMISSION");
					material->SetParameter("Emission", float3(emissionFac.r, emissionFac.g, emissionFac.b));
					material->SetParameter("EmissionTexture", mScene->AssetManager()->LoadTexture(emission.C_Str()));
				} else if (emissionFac.r > 0 || emissionFac.g > 0 || emissionFac.b > 0) {
					material->EnableKeyword("EMISSION");
					material->SetParameter("Emission", float3(emissionFac.r, emissionFac.g, emissionFac.b));
					material->SetParameter("EmissionTexture", mScene->AssetManager()->LoadTexture("Assets/white.png"));
				}

				material->RenderQueue(material->RenderQueue() + renderQueueOffset);
				mMaterials.push_back(material);
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

	AABB aabb = root->BoundsHeirarchy();
	root->LocalScale(.5f / fmaxf(fmaxf(aabb.mExtents.x, aabb.mExtents.y), aabb.mExtents.z));
	aabb = root->BoundsHeirarchy();
	float3 offset = root->WorldPosition() - aabb.mCenter;
	offset.y += aabb.mExtents.y;
	root->LocalPosition(offset);

	mLoaded.emplace(filename, root);

	return root;
}
Object* MeshViewer::LoadObj(const string& filename, Shader* shader, float scale, const float4& col, float metal, float rough) {
	if (mLoaded.count(filename)) return mLoaded.at(filename);

	#pragma pack(push)
	#pragma pack(1)
	struct objvertex {
		float3 position;
		float3 normal;
	};
	#pragma pack(pop)
	static const ::VertexInput ObjInput {
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
	
	shared_ptr<Material> mat = make_shared<Material>(filename, shader);
	mat->SetParameter("EnvironmentTexture", mEnvironmentTexture);
	mat->SetParameter("EnvironmentStrength", mEnvironmentStrength);
	mat->SetParameter("BrdfTexture", mScene->AssetManager()->LoadTexture("Assets/BrdfLut.png", false));
	mat->SetParameter("Color", col);
	mat->SetParameter("Metallic", metal);
	mat->SetParameter("Roughness", rough);
	shared_ptr<MeshRenderer> obj = make_shared<MeshRenderer>(filename);
	obj->Material(mat);

	mMaterials.push_back(mat);

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
				i += (uint32_t)idx + 1;
				cur.normal.y = stof(string(line.data() + i), &idx);
				i += (uint32_t)idx + 1;
				cur.normal.z = -stof(string(line.data() + i), &idx);
				i += (uint32_t)idx + 1;
				a++;
			} else {
				uint32_t i = 2;
				size_t idx;
				cur.position.x = stof(string(line.data() + i), &idx) * scale;
				i += (uint32_t)idx + 1;
				cur.position.y = stof(string(line.data() + i), &idx) * scale;
				i += (uint32_t)idx + 1;
				cur.position.z = -stof(string(line.data() + i), &idx) * scale;
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
				uint32_t i0 = stoi(string(line.data() + i), &idx); i += (uint32_t)idx + 2;
				stoi(string(line.data() + i), &idx); i += (uint32_t)idx + 1;
				uint32_t i1 = stoi(string(line.data() + i), &idx); i += (uint32_t)idx + 2;
				stoi(string(line.data() + i), &idx); i += (uint32_t)idx + 1;
				uint32_t i2 = stoi(string(line.data() + i), &idx);

				indices.push_back(i0 - 1);
				indices.push_back(i1 - 1);
				indices.push_back(i2 - 1);
			}
		}

	}

	obj->Mesh(make_shared<Mesh>(filename, mScene->DeviceManager(), vertices.data(), indices.data(), (uint32_t)vertices.size(), (uint32_t)sizeof(objvertex), (uint32_t)indices.size(), &ObjInput, VK_INDEX_TYPE_UINT32));

	mObjects.push_back(obj.get());
	mScene->AddObject(obj);

	AABB aabb = obj->BoundsHeirarchy();
	obj->LocalScale(.5f / fmaxf(fmaxf(aabb.mExtents.x, aabb.mExtents.y), aabb.mExtents.z));
	aabb = obj->BoundsHeirarchy();
	float3 offset = obj->WorldPosition() - aabb.mCenter;
	offset.y += aabb.mExtents.y;
	obj->LocalPosition(offset);
	mLoaded.emplace(filename, obj.get());

	return obj.get();
}

void GetFiles(const string& path, vector<fs::path>& files) {
	for (auto f : fs::directory_iterator(path)) {
		if (f.is_directory())
			GetFiles(f.path().string(), files);
		else if (f.is_regular_file())
			files.push_back(f.path());
	}
}

bool MeshViewer::Init(Scene* scene) {
	mScene = scene;

	mEnvironmentTexture = mScene->AssetManager()->LoadTexture("Assets/sky.hdr", false);
	mEnvironmentStrength = .5f;

	// preload these since they will likely be used later
	Texture* white = mScene->AssetManager()->LoadTexture("Assets/white.png");
	mScene->AssetManager()->LoadTexture("Assets/bump.png", false);

	Shader* pbrshader  = mScene->AssetManager()->LoadShader("Shaders/pbr.shader");

	#pragma region Skybox and Ground
	shared_ptr<Material> skyboxMat = make_shared<Material>("Skybox", mScene->AssetManager()->LoadShader("Shaders/skybox.shader"));
	skyboxMat->SetParameter("EnvironmentTexture", mEnvironmentTexture);
	mMaterials.push_back(skyboxMat);
	shared_ptr<MeshRenderer> skybox = make_shared<MeshRenderer>("SkyCube");
	skybox->LocalScale(1e23f);
	skybox->Mesh(shared_ptr<Mesh>(Mesh::CreateCube("Cube", mScene->DeviceManager())));
	skybox->Material(skyboxMat);
	mObjects.push_back(skybox.get());
	mScene->AddObject(skybox);

	shared_ptr<Material> groundMat = make_shared<Material>("Ground", pbrshader);
	groundMat->SetParameter("EnvironmentTexture", mEnvironmentTexture);
	groundMat->SetParameter("EnvironmentStrength", mEnvironmentStrength);
	groundMat->SetParameter("BrdfTexture", mScene->AssetManager()->LoadTexture("Assets/BrdfLut.png", false));
	groundMat->SetParameter("Color", float4(.9f, .9f, .9f, 1));
	groundMat->SetParameter("Metallic", 0.f);
	groundMat->SetParameter("Roughness", .8f);
	mMaterials.push_back(groundMat);
	shared_ptr<MeshRenderer> ground = make_shared<MeshRenderer>("Ground");
	ground->Mesh(shared_ptr<Mesh>(Mesh::CreatePlane("Ground", mScene->DeviceManager(), 100.f)));
	ground->Material(groundMat);
	ground->LocalRotation(quaternion(float3(-PI * .5f, 0, 0)));
	mObjects.push_back(ground.get());
	mScene->AddObject(ground);
	#pragma endregion

	#pragma region Lights
	shared_ptr<Light> light0 = make_shared<Light>("Spot");
	light0->Type(Spot);
	light0->Intensity(10.f);
	light0->InnerSpotAngle(radians(10.f));
	light0->OuterSpotAngle(radians(25.f));
	light0->Range(12);
	light0->Color(float3(1.f, .7f, .5f));
	light0->LocalRotation(quaternion(radians(float3(45, 45, 0))));
	light0->LocalPosition(light0->LocalRotation() * float3(0, 0, -3) + float3(0, 1, 0));
	mObjects.push_back(light0.get());
	mScene->AddObject(light0);

	shared_ptr<Light> light1 = make_shared<Light>("Point");
	light1->Type(Point);
	light1->Intensity(.8f);
	light1->Range(3.f);
	light1->LocalPosition(-1.25f, 1.5f, -.25f);
	light1->Color(float3(.5f, .5f, 1.f));
	mObjects.push_back(light1.get());
	mScene->AddObject(light1);

	shared_ptr<Light> light2 = make_shared<Light>("Point");
	light2->Type(Point);
	light2->Intensity(.8f);
	light2->Range(3.f);
	light2->LocalPosition(1.25f, 1.5f, -.25f);
	light2->Color(float3(1.f, .5f, .5f));
	mObjects.push_back(light2.get());
	mScene->AddObject(light2);
	#pragma endregion

	#pragma region Menu
	Shader* fontShader = mScene->AssetManager()->LoadShader("Shaders/font.shader");
	Font* font = mScene->AssetManager()->LoadFont("Assets/OpenSans-Regular.ttf", 96);
	Font* boldfont = mScene->AssetManager()->LoadFont("Assets/OpenSans-Bold.ttf", 96);
	shared_ptr<Material> uiMat = make_shared<Material>("UI", mScene->AssetManager()->LoadShader("Shaders/ui.shader"));
	shared_ptr<Material> fontMat = make_shared<Material>("OpenSans", fontShader);
	shared_ptr<Material> boldMat = make_shared<Material>("OpenSans Bold", fontShader);
	fontMat->SetParameter("MainTexture", font->Texture());
	boldMat->SetParameter("MainTexture", boldfont->Texture());

	shared_ptr<UICanvas> panel = make_shared<UICanvas>("MeshViewerPanel", float2(.2f, .25f));
	panel->RenderQueue(5000);
	panel->LocalPosition(0, 1.5f, 0.5f);

	shared_ptr<UIImage> bg = make_shared<UIImage>("Background");
	panel->AddElement(bg);
	bg->Texture(white);
	bg->Material(uiMat);
	bg->Color(float4(0, 0, 0, .5f));
	bg->Extent(1, 1, 0, 0);

	shared_ptr<VerticalLayout> layout = make_shared<VerticalLayout>("Layout");
	panel->AddElement(layout);
	layout->Extent(1, 1, 0, 0);

	shared_ptr<TextButton> title = make_shared<TextButton>("Title");
	panel->AddElement(title);
	title->HorizontalAnchor(TextAnchor::Minimum);
	title->Font(boldfont);
	title->Material(boldMat);
	title->Extent(1, 0, 0, .1f);
	title->TextScale(.1f);
	title->Text("MeshViewer");
	layout->AddChild(title.get());

	vector<fs::path> files;
	GetFiles("Assets", files);
	for (auto f : files) {
		string ext = f.extension().string();
		if (ext != ".obj" && ext != ".fbx" && ext != ".gltf" && ext != ".glb") continue;

		shared_ptr<TextButton> btn = make_shared<TextButton>(f.string());
		panel->AddElement(btn);
		btn->HorizontalAnchor(TextAnchor::Minimum);
		btn->Font(font);
		btn->Material(fontMat);
		btn->Extent(1, 0, 0, .075f);
		btn->TextScale(.075f);
		btn->Text(f.filename().string());
		layout->AddChild(btn.get());
		mSceneButtons.push_back(btn.get());
	}

	layout->UpdateLayout();

	mScene->AddObject(panel);
	mObjects.push_back(panel.get());
	#pragma endregion

	LoadScene(mSceneButtons[0]->mName, pbrshader);

	return true;
}