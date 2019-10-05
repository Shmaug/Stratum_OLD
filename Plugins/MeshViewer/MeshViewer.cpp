#include <Core/EnginePlugin.hpp>
#include <thread>

#include <Scene/MeshRenderer.hpp>
#include <Scene/TextRenderer.hpp>
#include <Util/Profiler.hpp>
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
	Scene* mScene;
}; 

ENGINE_PLUGIN(MeshViewer)

MeshViewer::MeshViewer() : mScene(nullptr) {}
MeshViewer::~MeshViewer() {
	for (Object* obj : mObjects)
		mScene->RemoveObject(obj);
	for (Object* obj : mObjects)
		safe_delete(obj);
}

void LoadScene(const filesystem::path& filename, Scene* scene, Shader* shader, vector<Object*>& objects, float scale = .05f) {
	Texture* brdfTexture  = scene->DeviceManager()->AssetDatabase()->LoadTexture("Assets/BrdfLut.png", false);
	Texture* whiteTexture = scene->DeviceManager()->AssetDatabase()->LoadTexture("Assets/white.png");
	Texture* bumpTexture  = scene->DeviceManager()->AssetDatabase()->LoadTexture("Assets/bump.png", false);

	unordered_map<uint32_t, shared_ptr<Mesh>> meshes;
	unordered_map<uint32_t, shared_ptr<Material>> materials;

	const aiScene* aiscene = aiImportFile(filename.string().c_str(), aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_FlipUVs  | aiProcess_MakeLeftHanded);
	if (!aiscene) return;

	queue<pair<aiNode*, Object*>> nodes;
	nodes.push(make_pair(aiscene->mRootNode, nullptr));

	while(!nodes.empty()) {
		pair<aiNode*, Object*> nodepair = nodes.front();
		nodes.pop();

		aiNode* node = nodepair.first;

		aiVector3D s;
		aiQuaternion r;
		aiVector3D t;
		node->mTransformation.Decompose(s, r, t);

		t *= scale;
		
		Object* nodeobj = new Object(node->mName.C_Str());
		nodeobj->Parent(nodepair.second);
		nodeobj->LocalPosition(t.x, t.y, t.z);
		nodeobj->LocalRotation(quat(r.w, r.x, r.y, r.z));
		nodeobj->LocalScale(s.x, s.y, s.z);

		scene->AddObject(nodeobj);
		objects.push_back(nodeobj);

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
					material->SetParameter("DiffuseTexture", whiteTexture);
				else {
					if (filename.has_parent_path())
						diffuse = filename.parent_path().string() + "/" + diffuse.C_Str();
					material->SetParameter("DiffuseTexture", scene->DeviceManager()->AssetDatabase()->LoadTexture(diffuse.C_Str()));
				}

				if (normal == aiString("") || normal.C_Str()[0] == '*')
					material->SetParameter("NormalTexture", bumpTexture);
				else {
					if (filename.has_parent_path())
						normal = filename.parent_path().string() + "/" + normal.C_Str();
					material->SetParameter("NormalTexture", scene->DeviceManager()->AssetDatabase()->LoadTexture(normal.C_Str(), false));
				}
			}

			shared_ptr<Mesh>& mesh = meshes[node->mMeshes[i]];
			if (!mesh) mesh = make_shared<Mesh>(aimesh->mName.C_Str(), scene->DeviceManager(), aimesh, scale);

			auto meshRenderer = new MeshRenderer(node->mName.C_Str() + string(".") + aimesh->mName.C_Str());
			meshRenderer->Parent(nodeobj);
			meshRenderer->Mesh(mesh);
			meshRenderer->Material(material);

			scene->AddObject(meshRenderer);
			objects.push_back(meshRenderer);

			//printf("%s: <%.2f, %.2f, %.2f> [%.2f, %.2f, %.2f]\n", node->mName.C_Str(), meshRenderer->Bounds().mCenter.x, meshRenderer->Bounds().mCenter.y, meshRenderer->Bounds().mCenter.z, meshRenderer->Bounds().mExtents.x, meshRenderer->Bounds().mExtents.y, meshRenderer->Bounds().mExtents.z);
		}

		for (uint32_t i = 0; i < node->mNumChildren; i++)
			nodes.push(make_pair(node->mChildren[i], nodeobj));
	}
}

bool MeshViewer::Init(Scene* scene) {
	mScene = scene;

	Shader* pbrshader  = scene->DeviceManager()->AssetDatabase()->LoadShader("Shaders/pbr.shader");
	//LoadScene("Assets/SanMiguel/SanMiguel.gltf", scene, pbrshader, mObjects);

	return true;
}