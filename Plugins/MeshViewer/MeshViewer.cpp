#include <Core/EnginePlugin.hpp>
#include <thread>
#include <unordered_map>

#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Scene/MeshRenderer.hpp>
#include <Scene/TextRenderer.hpp>
#include <Interface/UICanvas.hpp>
#include <Interface/UIImage.hpp>
#include <Interface/UILabel.hpp>
#include <Interface/UILayout.hpp>
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
	PLUGIN_EXPORT void Update(const FrameTime& frameTime) override;
	PLUGIN_EXPORT void DrawGizmos(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex);

private:
	float mLastClick;

	float mEnvironmentStrength;
	Texture* mEnvironmentTexture;

	vector<shared_ptr<Object>> mLoadedObjects;

	// for editing lights
	Light* mSelectedLight;
	vector<Light*> mLights;

	vector<Object*> mObjects;
	unordered_map<string, Object*> mLoaded;
	vector<shared_ptr<Material>> mMaterials;
	Scene* mScene;

	UICanvas* mPanel;
	UIElement* mFileLoadPanel;
	UIElement* mLightSettingsPanel;
	vector<UIImage*> mLoadFileButtons;
	UILabel* mTitleText;
	UILabel* mLoadText;
	UIImage* mLoadBar;

	Shader* mPBRShader;

	bool mDraggingPanel;
	bool mLoading;
	float mLoadProgress;
	std::thread mLoadThread;

	void LoadAsync(fs::path filename, float scale = .05f);
	Object* LoadScene(fs::path filename, float scale = .05f);
	Object* LoadObj(fs::path filename, float scale = .05f, const float4& col = float4(1), float metal = 0, float rough = 1);
};

ENGINE_PLUGIN(MeshViewer)

MeshViewer::MeshViewer()
	: mScene(nullptr), mPBRShader(nullptr), mLoading(false), mLastClick(0), mEnvironmentStrength(1.f), mEnvironmentTexture(nullptr), mSelectedLight(nullptr), mPanel(nullptr), mDraggingPanel(false) {
	mEnabled = true;
}
MeshViewer::~MeshViewer() {
	if (mLoadThread.joinable()) mLoadThread.join();
	for (Object* obj : mObjects)
		mScene->RemoveObject(obj);
}

struct Vertex {
	float3 position;
	float3 normal;
	float4 tangent;
	float2 uv;

	static const ::VertexInput VertexInput;
};
const ::VertexInput Vertex::VertexInput {
	{
		0, // binding
		sizeof(Vertex), // stride
		VK_VERTEX_INPUT_RATE_VERTEX // inputRate
	},
	{
		{
			0, // location
			0, // binding
			VK_FORMAT_R32G32B32_SFLOAT, // format
			offsetof(Vertex, position) // offset
		},
		{
			1, // location
			0, // binding
			VK_FORMAT_R32G32B32_SFLOAT, // format
			offsetof(Vertex, normal) // offset
		},
		{
			2, // location
			0, // binding
			VK_FORMAT_R32G32B32A32_SFLOAT, // format
			offsetof(Vertex, tangent) // offset
		},
		{
			3, // location
			0, // binding
			VK_FORMAT_R32G32_SFLOAT, // format
			offsetof(Vertex, uv) // offset
		}
	}
};
Object* MeshViewer::LoadScene(fs::path path, float scale) {
	mLoadProgress = 0;
	
	Texture* brdfTexture  = mScene->AssetManager()->LoadTexture("Assets/BrdfLut.png", false);
	Texture* whiteTexture = mScene->AssetManager()->LoadTexture("Assets/white.png");
	Texture* bumpTexture  = mScene->AssetManager()->LoadTexture("Assets/bump.png", false);

	unordered_map<uint32_t, shared_ptr<Mesh>> meshes;
	unordered_map<uint32_t, shared_ptr<Material>> materials;

	const aiScene* aiscene = aiImportFile(path.string().c_str(), aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_FlipUVs  | aiProcess_MakeLeftHanded);
	if (!aiscene) {
		cerr << "Failed to open " << path.string() <<  ": " << aiGetErrorString() << endl;
		return nullptr;
	}

	uint32_t meshCount = 0;
	queue<aiNode*> aiNodes;
	aiNodes.push(aiscene->mRootNode);
	while (!aiNodes.empty()){
		aiNode* node = aiNodes.front();
		aiNodes.pop();
		meshCount += node->mNumMeshes;
		for (uint32_t i = 0; i < node->mNumChildren; i++)
			aiNodes.push(node->mChildren[i]);
	}
	
	queue<pair<aiNode*, Object*>> nodes;
	nodes.push(make_pair(aiscene->mRootNode, nullptr));
	Object* root = nullptr;

	uint32_t meshNum = 0;
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
		nodepair.second->AddChild(nodeobj.get());
		nodeobj->LocalPosition(t.x, t.y, t.z);
		nodeobj->LocalRotation(quaternion(r.x, r.y, r.z, r.w));
		nodeobj->LocalScale(s.x, s.y, s.z);
		mLoadedObjects.push_back(nodeobj);

		if (nodepair.first == aiscene->mRootNode)
			root = nodeobj.get();

		for (uint32_t i = 0; i < node->mNumMeshes; i++) {
			aiMesh* aimesh = aiscene->mMeshes[node->mMeshes[i]];
			aiMaterial* aimat = aiscene->mMaterials[aimesh->mMaterialIndex];

			mLoadProgress = (float)meshNum / (float)meshCount;

			if ((aimesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE) == 0) continue;

			shared_ptr<Material>& material = materials[aimesh->mMaterialIndex];
			if (!material) {
				VkCullModeFlags cullMode = VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
				BlendMode blendMode = BLEND_MODE_MAX_ENUM;
				bool twoSided = false;
				aiColor3D emissionFac;
				aiColor4D color;
				aiString matname, diffuse, normal, metalroughness, emission, occlusion, specgloss;
				aiString alphaMode;
				float metallic, roughness;
				aimat->Get(AI_MATKEY_NAME, matname);
				if (aimat->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_FACTOR, color) != aiReturn::aiReturn_SUCCESS)
					color.r = color.g = color.b = color.a = 1;
				if (aimat->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, metallic) != aiReturn::aiReturn_SUCCESS) metallic = 0.f;
				if (aimat->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, roughness) != aiReturn::aiReturn_SUCCESS) roughness = .5f;
				if (aimat->Get(AI_MATKEY_TWOSIDED, twoSided) == aiReturn::aiReturn_SUCCESS && twoSided) cullMode = VK_CULL_MODE_NONE;
				if (aimat->Get(AI_MATKEY_COLOR_EMISSIVE, emissionFac) != aiReturn::aiReturn_SUCCESS) emissionFac.r = emissionFac.g = emissionFac.b = 0;
				if (aimat->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == aiReturn::aiReturn_SUCCESS && strcmp(alphaMode.C_Str(), "BLEND") == 0) blendMode = Alpha;
				aimat->Get(AI_MATKEY_GLTF_PBRSPECULARGLOSSINESS, specgloss);

				aimat->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_TEXTURE, &diffuse);
				aimat->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &metalroughness);
				aimat->GetTexture(aiTextureType_LIGHTMAP, 0, &occlusion);
				aimat->GetTexture(aiTextureType_NORMALS, 0, &normal);
				aimat->GetTexture(aiTextureType_EMISSIVE, 0, &emission);

				material = make_shared<Material>(matname.C_Str(), mPBRShader);
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
				if (specgloss != aiString("") && specgloss.C_Str()[0] != '*') {
					if (path.has_parent_path())
						specgloss = path.parent_path().string() + "/" + specgloss.C_Str();
					material->EnableKeyword("SPECGLOSS_MAP");
					material->SetParameter("SpecGlossTexture", mScene->AssetManager()->LoadTexture(specgloss.C_Str()));
				}
				if (normal != aiString("") && normal.C_Str()[0] != '*'){
					if (path.has_parent_path())
						normal = path.parent_path().string() + "/" + normal.C_Str();
					material->EnableKeyword("NORMAL_MAP");
					material->SetParameter("NormalTexture", mScene->AssetManager()->LoadTexture(normal.C_Str(), false));
				}
				if (occlusion != aiString("") && occlusion.C_Str()[0] != '*') {
					if (path.has_parent_path())
						occlusion = path.parent_path().string() + "/" + occlusion.C_Str();
					material->EnableKeyword("OCCLUSION_MAP");
					material->SetParameter("OcclusionTexture", mScene->AssetManager()->LoadTexture(occlusion.C_Str(), false));
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
			if (!mesh) {
				const aiMesh* aimesh = aiscene->mMeshes[node->mMeshes[i]];
				vector<Vertex> vertices(aimesh->mNumVertices);
				memset(vertices.data(), 0, sizeof(Vertex) * aimesh->mNumVertices);

				float3 mn, mx;

				// append vertices, keep track of bounding box
				for (uint32_t j = 0; j < aimesh->mNumVertices; j++) {
					Vertex& vertex = vertices[j];

					vertex.position = float3((float)aimesh->mVertices[j].x, (float)aimesh->mVertices[j].y, (float)aimesh->mVertices[j].z);
					if (aimesh->HasNormals()) vertex.normal = float3((float)aimesh->mNormals[j].x, (float)aimesh->mNormals[j].y, (float)aimesh->mNormals[j].z);
					if (aimesh->HasTangentsAndBitangents()) {
						vertex.tangent = float4((float)aimesh->mTangents[j].x, (float)aimesh->mTangents[j].y, (float)aimesh->mTangents[j].z, 1.f);
						float3 bt = float3((float)aimesh->mBitangents[j].x, (float)aimesh->mBitangents[j].y, (float)aimesh->mBitangents[j].z);
						vertex.tangent.w = dot(cross(vertex.tangent.xyz, vertex.normal), bt) > 0.f ? 1.f : -1.f;
					}
					if (aimesh->HasTextureCoords(0)) vertex.uv = float2((float)aimesh->mTextureCoords[0][j].x, (float)aimesh->mTextureCoords[0][j].y);
					vertex.position *= scale;

					float vp = .5f * (float)j / (float)aimesh->mNumVertices;
					mLoadProgress = ((float)meshNum + vp) / (float)meshCount;
				}

				bool use32bit = aimesh->mNumVertices > 0xFFFF;
				vector<uint16_t> indices16;
				vector<uint32_t> indices32;

				if (use32bit)
					for (uint32_t j = 0; j < aimesh->mNumFaces; j++) {
						const aiFace& f = aimesh->mFaces[j];
						if (f.mNumIndices == 0) continue;
						indices32.push_back(f.mIndices[0]);
						if (f.mNumIndices == 2) indices32.push_back(f.mIndices[1]);
						for (uint32_t j = 2; j < f.mNumIndices; j++) {
							indices32.push_back(f.mIndices[j - 1]);
							indices32.push_back(f.mIndices[j]);
						}
						float fp = .5f + .5f * (float)j / (float)aimesh->mNumFaces;
						mLoadProgress = ((float)meshNum + fp) / (float)meshCount;
					} else {
						for (uint32_t j = 0; j < aimesh->mNumFaces; j++) {
							const aiFace& f = aimesh->mFaces[j];
							if (f.mNumIndices == 0) continue;
							indices16.push_back(f.mIndices[0]);
							if (f.mNumIndices == 2) indices16.push_back(f.mIndices[1]);
							for (uint32_t j = 2; j < f.mNumIndices; j++) {
								indices16.push_back(f.mIndices[j - 1]);
								indices16.push_back(f.mIndices[j]);
							}
							float fp = .5f + .5f * (float)j / (float)aimesh->mNumFaces;
							mLoadProgress = ((float)meshNum + fp) / (float)meshCount;
						}
					}

				void* indices;
				VkIndexType indexType;
				uint32_t indexCount;
				if (use32bit) {
					indexCount = (uint32_t)indices32.size();
					indexType = VK_INDEX_TYPE_UINT32;
					indices = indices32.data();
				} else {
					indexCount = (uint32_t)indices16.size();
					indexType = VK_INDEX_TYPE_UINT16;
					indices = indices16.data();
				}

				// mesh = make_shared<Mesh>(aimesh->mName.C_Str(), mScene->DeviceManager(), aimesh, scale);
				mesh = make_shared<Mesh>(aimesh->mName.C_Str(), mScene->DeviceManager(),
					vertices.data(), indices, (uint32_t)vertices.size(), (uint32_t)sizeof(Vertex), indexCount, &Vertex::VertexInput, indexType);
			}

			// TODO: load weights...

			shared_ptr<MeshRenderer> meshRenderer = make_shared<MeshRenderer>(node->mName.C_Str() + string(".") + aimesh->mName.C_Str());
			nodeobj.get()->AddChild(meshRenderer.get());
			meshRenderer->Mesh(mesh);
			meshRenderer->Material(material);
			mLoadedObjects.push_back(meshRenderer);

			meshNum++;
		}

		for (uint32_t i = 0; i < node->mNumChildren; i++)
			nodes.push(make_pair(node->mChildren[i], nodeobj.get()));
	}

	AABB aabb = root->BoundsHierarchy();
	root->LocalScale(.5f / fmaxf(fmaxf(aabb.mExtents.x, aabb.mExtents.y), aabb.mExtents.z));
	aabb = root->BoundsHierarchy();
	float3 offset = root->WorldPosition() - aabb.mCenter;
	offset.y += aabb.mExtents.y;
	root->LocalPosition(offset);

	mLoaded.emplace(path.string(), root);

	mLoading = false;
	return root;
}
Object* MeshViewer::LoadObj(fs::path filename, float scale, const float4& col, float metal, float rough) {
	if (mLoaded.count(filename.string())) return mLoaded.at(filename.string());

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
	
	shared_ptr<Material> mat = make_shared<Material>(filename.string(), mPBRShader);
	mat->SetParameter("EnvironmentTexture", mEnvironmentTexture);
	mat->SetParameter("EnvironmentStrength", mEnvironmentStrength);
	mat->SetParameter("BrdfTexture", mScene->AssetManager()->LoadTexture("Assets/BrdfLut.png", false));
	mat->SetParameter("Color", col);
	mat->SetParameter("Metallic", metal);
	mat->SetParameter("Roughness", rough);
	shared_ptr<MeshRenderer> obj = make_shared<MeshRenderer>(filename.string());
	obj->Material(mat);

	mMaterials.push_back(mat);

	vector<objvertex> vertices;
	vector<uint32_t> indices;
	uint32_t a = 0;

	objvertex cur;

	ifstream file(filename.string());

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

	obj->Mesh(make_shared<Mesh>(filename.string(), mScene->DeviceManager(), vertices.data(), indices.data(), (uint32_t)vertices.size(), (uint32_t)sizeof(objvertex), (uint32_t)indices.size(), &ObjInput, VK_INDEX_TYPE_UINT32));

	mObjects.push_back(obj.get());
	mScene->AddObject(obj);

	AABB aabb = obj->BoundsHierarchy();
	obj->LocalScale(.5f / fmaxf(fmaxf(aabb.mExtents.x, aabb.mExtents.y), aabb.mExtents.z));
	aabb = obj->BoundsHierarchy();
	float3 offset = obj->WorldPosition() - aabb.mCenter;
	offset.y += aabb.mExtents.y;
	obj->LocalPosition(offset);
	mLoaded.emplace(filename.string(), obj.get());

	return obj.get();
}

void MeshViewer::LoadAsync(fs::path path, float scale) {
	// disable all the other models
	for (auto& l : mLoaded)
		l.second->mEnabled = false;

	if (mLoaded.count(path.string()))
		// we've already loaded this model, just enable it
		mLoaded.at(path.string())->mEnabled = true;
	else {
		// load the model asynchronously
		mLoading = true;
		mLoadProgress = 0.f;
		mLoadThread = thread(&MeshViewer::LoadScene, this, path, scale);
	}
}

void GetFiles(const string& path, vector<fs::path>& files) {
	for (auto f : fs::directory_iterator(path)) {
		if (fs::is_directory(f.path()))
			GetFiles(f.path().string(), files);
		else if (fs::is_regular_file(f.path()))
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

	mPBRShader  = mScene->AssetManager()->LoadShader("Shaders/pbr.shader");

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

	shared_ptr<Material> groundMat = make_shared<Material>("Ground", mPBRShader);
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
	mLights.push_back(light0.get());
	mScene->AddObject(light0);

	shared_ptr<Light> light1 = make_shared<Light>("Point");
	light1->Type(Point);
	light1->Intensity(.8f);
	light1->Range(3.f);
	light1->LocalPosition(-1.25f, 1.5f, -.25f);
	light1->Color(float3(.5f, .5f, 1.f));
	mObjects.push_back(light1.get());
	mLights.push_back(light1.get());
	mScene->AddObject(light1);

	shared_ptr<Light> light2 = make_shared<Light>("Point");
	light2->Type(Point);
	light2->Intensity(.8f);
	light2->Range(3.f);
	light2->LocalPosition(1.25f, 1.5f, -.25f);
	light2->Color(float3(1.f, .5f, .5f));
	mObjects.push_back(light2.get());
	mLights.push_back(light2.get());
	mScene->AddObject(light2);

	shared_ptr<Light> light3 = make_shared<Light>("Sun");
	light3->Type(Sun);
	light3->Intensity(.1f);
	light3->Color(float3(1.f, .95f, .9f));
	light3->LocalRotation(quaternion(radians(float3(45, -45, 0))));
	light3->LocalPosition(float3(0, 3, 0));
	mLights.push_back(light3.get());
	mObjects.push_back(light3.get());
	mScene->AddObject(light3);
	#pragma endregion

	#pragma region Menu
	Font* font = mScene->AssetManager()->LoadFont("Assets/OpenSans-Regular.ttf", 96);
	Font* boldfont = mScene->AssetManager()->LoadFont("Assets/OpenSans-Bold.ttf", 96);

	#pragma region Header
	shared_ptr<UICanvas> panel = make_shared<UICanvas>("MeshViewerPanel", float2(.15f, .25f));
	panel->RenderQueue(5000);
	panel->LocalPosition(0.2f, 1.f, -0.1f);
	mPanel = panel.get();

	shared_ptr<UIImage> panelbg = panel->AddElement<UIImage>("Background", panel.get());
	panelbg->Depth(1.f);
	panelbg->Texture(white);
	panelbg->Color(float4(0, 0, 0, .5f));
	panelbg->Extent(1, 1, 0, 0);
	panelbg->Outline(true);

	shared_ptr<UILabel> title = panel->AddElement<UILabel>("Title", panel.get());
	title->VerticalAnchor(TextAnchor::Minimum);
	title->Font(boldfont);
	title->Position(0, 1, 0, -.02f);
	title->Extent(1, 0, 0, .02f);
	title->TextScale(.05f);
	title->Text("MeshViewer");
	title->mRecieveRaycast = true;
	mTitleText = title.get();
	
	shared_ptr<UIImage> separator = panel->AddElement<UIImage>("Separator", panel.get());
	separator->Texture(white);
	separator->Color(float4(0));
	separator->Outline(true);
	separator->Position(0, 1, 0, -.05f);
	separator->Extent(.95f, 0, 0, 0);
	#pragma endregion

	#pragma region File loading
	shared_ptr<UILayout> fileLayout = panel->AddElement<UILayout>("File Loader", panel.get());
	fileLayout->Extent(.9f, 1, 0, -.025f);
	fileLayout->Position(0, 0, 0, -.05f);
	fileLayout->Spacing(.0025f);
	mFileLoadPanel = fileLayout.get();

	shared_ptr<UIImage> loadBar = panel->AddElement<UIImage>("LoadBar", panel.get());
	loadBar->Depth(.1f);
	loadBar->Texture(white);
	loadBar->Color(float4(.05f, .25f, .05f, 1));
	loadBar->Position(0, -1, 0, .01f);
	loadBar->Extent(1, 0, 0, .01f);
	loadBar->Outline(false);
	mLoadBar = loadBar.get();

	shared_ptr<UILabel> loadText = panel->AddElement<UILabel>("LoadText", panel.get());
	loadText->Depth(.01f);
	loadText->Font(font);
	loadText->TextScale(.0175f);
	loadText->Position(0, -1, 0, .01f);
	loadText->Extent(1, 0, 0, .01f);
	mLoadText = loadText.get();

	vector<fs::path> files;
	GetFiles("Assets", files);
	for (auto f : files) {
		string ext = f.extension().string();
		if (ext != ".obj" && ext != ".fbx" && ext != ".gltf" && ext != ".glb") continue;

		shared_ptr<UIImage> bg = panel->AddElement<UIImage>(f.string(), panel.get());
		bg->Texture(white);
		bg->Color(float4(0));
		bg->Extent(1, 0, 0, .015f);
		bg->Outline(false);
		bg->mRecieveRaycast = true;
		fileLayout->AddChild(bg.get());
		mLoadFileButtons.push_back(bg.get());

		shared_ptr<UILabel> btn = panel->AddElement<UILabel>(f.string(), panel.get());
		btn->VerticalAnchor(TextAnchor::Middle);
		btn->HorizontalAnchor(TextAnchor::Minimum);
		btn->Font(font);
		btn->Extent(.9f, 1, 0, 0);
		btn->TextScale(bg->AbsoluteExtent().y * 2);
		btn->Text(f.filename().string());
		bg->AddChild(btn.get());
	}

	fileLayout->UpdateLayout();
	#pragma endregion

	#pragma region Light editor
	shared_ptr<UILayout> lightLayout = panel->AddElement<UILayout>("Light Editor", panel.get());
	lightLayout->Extent(.9f, 1, 0, -.025f);
	lightLayout->Position(0, 0, 0, -.05f);
	lightLayout->Spacing(.0015f);
	mLightSettingsPanel = lightLayout.get();

	shared_ptr<UILabel> colorLabel = panel->AddElement<UILabel>("Color Label", panel.get());
	colorLabel->VerticalAnchor(TextAnchor::Middle);
	colorLabel->HorizontalAnchor(TextAnchor::Minimum);
	colorLabel->Font(font);
	colorLabel->Extent(.98f, 0, 0, .01f);
	colorLabel->TextScale(.02f);
	colorLabel->Text("Color");
	lightLayout->AddChild(colorLabel.get());


	shared_ptr<UILabel> intensityLabel = panel->AddElement<UILabel>("Intensity Label", panel.get());
	intensityLabel->VerticalAnchor(TextAnchor::Middle);
	intensityLabel->HorizontalAnchor(TextAnchor::Minimum);
	intensityLabel->Font(font);
	intensityLabel->Extent(.98f, 0, 0, .01f);
	intensityLabel->TextScale(.02f);
	intensityLabel->Text("Intensity");
	lightLayout->AddChild(intensityLabel.get());
	

	shared_ptr<UILabel> typeLabel = panel->AddElement<UILabel>("Type Label", panel.get());
	typeLabel->VerticalAnchor(TextAnchor::Middle);
	typeLabel->HorizontalAnchor(TextAnchor::Minimum);
	typeLabel->Font(font);
	typeLabel->Extent(.98f, 0, 0, .01f);
	typeLabel->TextScale(.02f);
	typeLabel->Text("Type");
	lightLayout->AddChild(typeLabel.get());


	shared_ptr<UILabel> radiusLabel = panel->AddElement<UILabel>("Radius Label", panel.get());
	radiusLabel->VerticalAnchor(TextAnchor::Middle);
	radiusLabel->HorizontalAnchor(TextAnchor::Minimum);
	radiusLabel->Font(font);
	radiusLabel->Extent(.98f, 0, 0, .01f);
	radiusLabel->TextScale(.02f);
	radiusLabel->Text("Radius");
	lightLayout->AddChild(radiusLabel.get());


	shared_ptr<UILabel> rangeLabel = panel->AddElement<UILabel>("Range Label", panel.get());
	rangeLabel->VerticalAnchor(TextAnchor::Middle);
	rangeLabel->HorizontalAnchor(TextAnchor::Minimum);
	rangeLabel->Font(font);
	rangeLabel->Extent(.98f, 0, 0, .01f);
	rangeLabel->TextScale(.02f);
	rangeLabel->Text("Range");
	lightLayout->AddChild(rangeLabel.get());


	shared_ptr<UILabel> outerAngleLabel = panel->AddElement<UILabel>("Outer Angle Label", panel.get());
	outerAngleLabel->VerticalAnchor(TextAnchor::Middle);
	outerAngleLabel->HorizontalAnchor(TextAnchor::Minimum);
	outerAngleLabel->Font(font);
	outerAngleLabel->Extent(.98f, 0, 0, .01f);
	outerAngleLabel->TextScale(.02f);
	outerAngleLabel->Text("Outer Angle");
	lightLayout->AddChild(outerAngleLabel.get());


	shared_ptr<UILabel> innerAngleLabel = panel->AddElement<UILabel>("Inner Angle Label", panel.get());
	innerAngleLabel->VerticalAnchor(TextAnchor::Middle);
	innerAngleLabel->HorizontalAnchor(TextAnchor::Minimum);
	innerAngleLabel->Font(font);
	innerAngleLabel->Extent(.98f, 0, 0, .01f);
	innerAngleLabel->TextScale(.02f);
	innerAngleLabel->Text("Inner Angle");
	lightLayout->AddChild(innerAngleLabel.get());

	lightLayout->UpdateLayout();
	lightLayout->mVisible = false;
	#pragma endregion

	mScene->AddObject(panel);
	mObjects.push_back(panel.get());
	#pragma endregion
	
	return true;
}

void MeshViewer::Update(const FrameTime& frameTime) {
	MouseKeyboardInput* input = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

	if (mLoading) {
		char str[16];
		sprintf(str, "%d%%", (int)(mLoadProgress*100.f+.5f));
		mLoadText->Text(str);
		mLoadText->mVisible = true;

		UDim2 position = mLoadBar->Position();
		UDim2 extent = mLoadBar->Extent();
		mLoadBar->Extent(mLoadProgress, extent.mScale.y, extent.mOffset.x, extent.mOffset.y);
		mLoadBar->Position(mLoadProgress - 1.f, position.mScale.y, position.mOffset.x, position.mOffset.y);
		mLoadBar->mVisible = true;
	} else {
		if (mLoadThread.joinable()) mLoadThread.join();
		mLoadText->mVisible = false;
		mLoadBar->mVisible = false;
		if (mLoadedObjects.size()) {
			for (const auto& o : mLoadedObjects) {
				mScene->AddObject(o);
				mObjects.push_back(o.get());
			}
			mLoadedObjects.clear();
		}
	}

	if (mDraggingPanel) {
		if (!input->MouseButtonDown(GLFW_MOUSE_BUTTON_LEFT))
			mDraggingPanel = false;
		else
			mPanel->LocalPosition() += mScene->Cameras()[0]->WorldRotation() * float3(input->CursorDelta() * float2(1, -1) * .1f, 0);
	} else {
		const Ray& ray = input->GetPointer(0)->mWorldRay;
		float hitT;
		Collider* hit = mScene->Raycast(ray, hitT);
		if (hit && hit == mPanel) {
			UIElement* elem = mPanel->Raycast(ray);

			if (elem == mTitleText) {
				if (input->MouseButtonDownFirst(GLFW_MOUSE_BUTTON_LEFT))
					mDraggingPanel = true;
			}

			for (UIImage* i : mLoadFileButtons) {
				if (!mLoading && i == elem) {
					i->Color(float4(1,1,1,.25f));
					i->Outline(true);
					if (input->MouseButtonDownFirst(GLFW_MOUSE_BUTTON_LEFT))
						LoadAsync(elem->mName);
				} else {
					i->Color(float4(0));
					i->Outline(false);
				}
			}
		}

		// Toggle menu on/off
		if (input->MouseButtonDownFirst(GLFW_MOUSE_BUTTON_RIGHT)) {
			if (frameTime.mTotalTime - mLastClick < .2f) {
				mPanel->mEnabled = !mPanel->mEnabled;
				const InputPointer* ptr = input->GetPointer(0);
				mPanel->LocalPosition(ptr->mWorldRay.mOrigin + ptr->mWorldRay.mDirection * 1.5f);
				mPanel->LocalRotation(mScene->Cameras()[0]->WorldRotation());
			}
			mLastClick = frameTime.mTotalTime;
		}
	}

}

void MeshViewer::DrawGizmos(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex) {
	MouseKeyboardInput* input = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

	const Ray& ray = input->GetPointer(0)->mWorldRay;
	float hitT;
	Collider* hit = mScene->Raycast(ray, hitT);

	Gizmos* gizmos = mScene->Gizmos();

	bool change = input->MouseButtonDownFirst(GLFW_MOUSE_BUTTON_LEFT);

	// manipulate selection
	if (mSelectedLight) {
		mLightSettingsPanel->mVisible = true;
		mFileLoadPanel->mVisible = false;
		switch (mSelectedLight->Type()) {
			case LightType::Spot:
				gizmos->DrawWireSphere(commandBuffer, backBufferIndex, mSelectedLight->WorldPosition(), mSelectedLight->Radius(), float4(mSelectedLight->Color(), .5f));
				gizmos->DrawWireCircle(commandBuffer, backBufferIndex,
				mSelectedLight->WorldPosition() + mSelectedLight->WorldRotation() * float3(0,0,mSelectedLight->Range()),
					mSelectedLight->Range() * tanf(mSelectedLight->InnerSpotAngle() * .5f), mSelectedLight->WorldRotation(), float4(mSelectedLight->Color(), .5f));
				gizmos->DrawWireCircle(commandBuffer, backBufferIndex,
				mSelectedLight->WorldPosition() + mSelectedLight->WorldRotation() * float3(0,0,mSelectedLight->Range()),
					mSelectedLight->Range() * tanf(mSelectedLight->OuterSpotAngle() * .5f), mSelectedLight->WorldRotation(), float4(mSelectedLight->Color(), .5f));
				break;

			case LightType::Point:
				gizmos->DrawWireSphere(commandBuffer, backBufferIndex, mSelectedLight->WorldPosition(), mSelectedLight->Radius(), float4(mSelectedLight->Color(), .5f));
				gizmos->DrawWireSphere(commandBuffer, backBufferIndex, mSelectedLight->WorldPosition(), mSelectedLight->Range(), float4(mSelectedLight->Color(), .1f));
				break;
			}

		if (input->KeyDown(GLFW_KEY_LEFT_SHIFT)) {
			quaternion r = mSelectedLight->WorldRotation();
			if (mScene->Gizmos()->RotationHandle(commandBuffer, backBufferIndex, input->GetPointer(0), mSelectedLight->WorldPosition(), r)) {
				mSelectedLight->LocalRotation(r);
				change = false;
			}
		}else{
			float3 p = mSelectedLight->WorldPosition();
			if (mScene->Gizmos()->PositionHandle(commandBuffer, backBufferIndex, input->GetPointer(0), camera->WorldRotation(), p)) {
				mSelectedLight->LocalPosition(p);
				change = false;
			}
		}
	}else{
		mLightSettingsPanel->mVisible = false;
		mFileLoadPanel->mVisible = true;
	}

	// change selection
	Light* sl = mSelectedLight;
	if (change) mSelectedLight = nullptr;
	for (Light* light : mLights) {
		float lt = ray.Intersect(Sphere(light->WorldPosition(), .09f)).x;
		bool hover = lt > 0 && (hitT < 0 || lt < hitT);
		if (hover) hitT = lt;

		float3 col = light->mEnabled ? light->Color() : light->Color() * .2f;
		gizmos->DrawBillboard(commandBuffer, backBufferIndex, light->WorldPosition(), hover && light != sl ? .09f : .075f, float4(col, 1), 
			mScene->AssetManager()->LoadTexture("Assets/icons.png"), float4(.5f, .5f, 0, 0));

		if (hover){
			if (input->MouseButtonDownFirst(GLFW_MOUSE_BUTTON_RIGHT))
				light->mEnabled = !light->mEnabled;
			if (change)
				mSelectedLight = light;
		}
	}
}