#include <Scene/Scene.hpp>
#include <Scene/Renderer.hpp>
#include <Scene/MeshRenderer.hpp>
#include <Scene/SkinnedMeshRenderer.hpp>
#include <Scene/GUI.hpp>
#include <Core/Instance.hpp>
#include <Util/Profiler.hpp>

#include <assimp/scene.h>
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>

using namespace std;

#define INSTANCE_BATCH_SIZE 1024
#define MAX_GPU_LIGHTS 64

#define SHADOW_ATLAS_RESOLUTION 8192
#define SHADOW_RESOLUTION 4096

const ::VertexInput Float3VertexInput{
	{
		{
			0, // binding
			sizeof(float3), // stride
			VK_VERTEX_INPUT_RATE_VERTEX // inputRate
		}
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

struct AIWeight {
	string bones[4];
	float4 weights;

	AIWeight() {
		bones[0] = bones[1] = bones[2] = bones[3] = "";
		weights[0] = weights[1] = weights[2] = weights[3] = 0.f;
	}
	inline void Set(const std::string& cluster, float weight) {
		if (weight < .001f) return;

		uint32_t index = 0;
		float m = weights[0];
		for (uint32_t i = 0; i < 4; i++) {
			if (cluster == bones[i]) {
				index = i;
				break;
			} else if (weights[i] < m) {
				index = i;
				m = weights[i];
			}
		}

		bones[index] = cluster;
		weights[index] = weight;
	}
	inline void Normalize() {
		weights /= dot(float4(1), weights);
	}
};

inline uint32_t GetDepth(aiNode* node) {
	uint32_t d = 0;
	while (node->mParent) {
		node = node->mParent;
		d++;
	}
	return d;
}
inline float4x4 ConvertMatrix(const aiMatrix4x4& m) {
	return float4x4(
		m.a1, m.b1, m.c1, m.d1,
		m.a2, m.b2, m.c2, m.d2,
		m.a3, m.b3, m.c3, m.d3,
		m.a4, m.b4, m.c4, m.d4
	);
}
inline Bone* AddBone(AnimationRig& rig, aiNode* node, const aiScene* scene, aiNode* root, unordered_map<aiNode*, Bone*>& boneMap, float scale) {
	if (node == root) return nullptr;
	if (boneMap.count(node))
		return boneMap.at(node);

	float4x4 mat = ConvertMatrix(node->mTransformation);
	Bone* parent = nullptr;

	if (node->mParent) {
		// merge empty bones
		aiNode* p = node->mParent;
		while (p && p->mName == aiString("")) {
			mat = ConvertMatrix(p->mTransformation) * mat;
			p = p->mParent;
		}
		// parent transform is the first non-empty parent bone
		if (p) parent = AddBone(rig, p, scene, root, boneMap, scale);
	}

	quaternion q;
	float3 p;
	float3 s;
	mat.Decompose(&p, &q, &s);

	Bone* bone = new Bone(node->mName.C_Str(), (uint32_t)rig.size());
	boneMap.emplace(node, bone);
	rig.push_back(bone);
	bone->LocalPosition(p * scale);
	bone->LocalRotation(q);
	bone->LocalScale(s);

	if (parent) parent->AddChild(bone);
	return bone;
}

bool RendererCompare(Object* oa, Object* ob) {
	Renderer* a = dynamic_cast<Renderer*>(oa);
	Renderer* b = dynamic_cast<Renderer*>(ob);
	uint32_t qa = a->Visible() ? a->RenderQueue() : 0xFFFFFFFF;
	uint32_t qb = b->Visible() ? b->RenderQueue() : 0xFFFFFFFF;
	if (qa == qb && qa != 0xFFFFFFFF) {
		MeshRenderer* ma = dynamic_cast<MeshRenderer*>(a);
		MeshRenderer* mb = dynamic_cast<MeshRenderer*>(b);
		if (ma && mb)
			if (ma->Material() == mb->Material())
				return ma->Mesh() < mb->Mesh();
			else
				return ma->Material() < mb->Material();
	}
	return qa < qb;
};

Scene::Scene(::Instance* instance, ::AssetManager* assetManager, ::InputManager* inputManager, ::PluginManager* pluginManager)
	: mInstance(instance), mAssetManager(assetManager), mInputManager(inputManager), mPluginManager(pluginManager), mLastBvhBuild(0), mDrawGizmos(false), mBvhDirty(true),
	mFixedTimeStep(.0025f), mPhysicsTimeLimitPerFrame(.2f) , mFixedAccumulator(0), mDeltaTime(0), mTotalTime(0), mFps(0), mFrameTimeAccum(0), mFrameCount(0){

	mBvh = new ObjectBvh2();
	mShadowTexelSize = float2(1.f / SHADOW_ATLAS_RESOLUTION, 1.f / SHADOW_ATLAS_RESOLUTION) * .75f;
	mEnvironment = new ::Environment(this);

	mShadowAtlasFramebuffer = new Framebuffer("ShadowAtlas", mInstance->Device(), SHADOW_ATLAS_RESOLUTION, SHADOW_ATLAS_RESOLUTION, {}, VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, {}, VK_ATTACHMENT_LOAD_OP_LOAD);
	mShadowAtlases = new Texture*[mInstance->Device()->MaxFramesInFlight()];
	
	auto commandBuffer = mInstance->Device()->GetCommandBuffer();

	uint32_t c = mInstance->Device()->MaxFramesInFlight();
	mLightBuffers = new Buffer*[c];
	mShadowBuffers = new Buffer*[c];
	for (uint32_t i = 0; i < c; i++) {
		mLightBuffers[i] = new Buffer("Light Buffer", mInstance->Device(), MAX_GPU_LIGHTS * sizeof(GPULight), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
		mShadowBuffers[i] = new Buffer("Shadow Buffer", mInstance->Device(), MAX_GPU_LIGHTS * sizeof(ShadowData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
		mShadowAtlases[i] = new Texture("ShadowAtlas", mInstance->Device(), SHADOW_ATLAS_RESOLUTION, SHADOW_ATLAS_RESOLUTION, 1, VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
		
		mShadowAtlases[i]->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer.get());
	}
	mInstance->Device()->Execute(commandBuffer, false)->Wait();

	float r = .5f;
	float3 verts[8]{
		float3(-r, -r, -r),
		float3(r, -r, -r),
		float3(-r, -r,  r),
		float3(r, -r,  r),
		float3(-r,  r, -r),
		float3(r,  r, -r),
		float3(-r,  r,  r),
		float3(r,  r,  r),
	};
	uint16_t indices[36]{
		2,7,6,2,3,7,
		0,1,2,2,1,3,
		1,5,7,7,3,1,
		4,5,1,4,1,0,
		6,4,2,4,0,2,
		4,7,5,4,6,7
	};
	mSkyboxCube = new Mesh("SkyCube", mInstance->Device(), verts, indices, 8, sizeof(float3), 36, &Float3VertexInput, VK_INDEX_TYPE_UINT16);

	mStartTime = mClock.now();
	mLastFrame = mClock.now();
}
Scene::~Scene(){
	safe_delete(mSkyboxCube);
	safe_delete(mBvh);

	while (mObjects.size())
		RemoveObject(mObjects[0].get());

	safe_delete(mEnvironment);

	for (uint32_t i = 0; i < mInstance->Device()->MaxFramesInFlight(); i++) {
		safe_delete(mShadowAtlases[i]);
		safe_delete(mLightBuffers[i]);
		safe_delete(mShadowBuffers[i]);
	}
	safe_delete_array(mShadowAtlases);
	safe_delete_array(mLightBuffers);
	safe_delete_array(mShadowBuffers);
	safe_delete(mShadowAtlasFramebuffer);
	for (Camera* c : mShadowCameras) safe_delete(c);

	mCameras.clear();
	mRenderers.clear();
	mLights.clear();
	mObjects.clear();
}

Object* Scene::LoadModelScene(const string& filename,
	function<shared_ptr<Material>(Scene*, aiMaterial*)> materialSetupFunc,
	function<void(Scene*, Object*, aiMaterial*)> objectSetupFunc,
	float scale, float directionalLightIntensity, float spotLightIntensity, float pointLightIntensity) {
	const aiScene* scene = aiImportFile(filename.c_str(), aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_FlipUVs | aiProcess_MakeLeftHanded | aiProcess_SortByPType);
	if (!scene) {
		fprintf_color(COLOR_RED, stderr, "Failed to open %s: %s\n", filename.c_str(), aiGetErrorString());
		throw;
	}

	Object* root = nullptr;

	vector<shared_ptr<Mesh>> meshes;
	vector<shared_ptr<Material>> materials;
	unordered_map<aiNode*, Object*> objectMap;

	vector<AIWeight> weights;
	unordered_map<string, aiBone*> uniqueBones;

	vector<StdVertex> vertices;
	vector<uint32_t> indices;

	uint32_t totalVertices = 0;
	uint32_t totalIndices = 0;

	bool hasBones = false;

	for (uint32_t m = 0; m < scene->mNumMeshes; m++) {
		const aiMesh* mesh = scene->mMeshes[m];
		totalVertices += mesh->mNumVertices;
		for (uint32_t i = 0; i < mesh->mNumFaces; i++)
			totalIndices += min(mesh->mFaces[i].mNumIndices, 3u);

		if (mesh->HasBones()) hasBones = true;
	}

	shared_ptr<Buffer> vertexBuffer = make_shared<Buffer>(filename + " Vertices", mInstance->Device(), sizeof(StdVertex) * totalVertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	shared_ptr<Buffer> indexBuffer  = make_shared<Buffer>(filename + " Indices" , mInstance->Device(), sizeof(uint32_t) * totalIndices  , VK_BUFFER_USAGE_INDEX_BUFFER_BIT  | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	shared_ptr<Buffer> weightBuffer = nullptr;
	if (hasBones) weightBuffer = make_shared<Buffer>(filename + " Weights", mInstance->Device(), sizeof(VertexWeight) * totalVertices, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	for (uint32_t m = 0; m < scene->mNumMaterials; m++)
		materials.push_back(materialSetupFunc(this, scene->mMaterials[m]));

	for (uint32_t m = 0; m < scene->mNumMeshes; m++) {
		const aiMesh* mesh = scene->mMeshes[m];

		if (mesh->mPrimitiveTypes != aiPrimitiveType_TRIANGLE) {
			meshes.push_back(nullptr);
			continue;
		}

		VkPrimitiveTopology topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		uint32_t baseVertex = (uint32_t)vertices.size();
		uint32_t baseIndex  = (uint32_t)indices.size();

		// vertex data
		for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
			StdVertex vertex = {};
			memset(&vertex, 0, sizeof(StdVertex));

			vertex.position = { (float)mesh->mVertices[i].x, (float)mesh->mVertices[i].y, (float)mesh->mVertices[i].z };
			if (mesh->HasNormals()) vertex.normal = { (float)mesh->mNormals[i].x, (float)mesh->mNormals[i].y, (float)mesh->mNormals[i].z };
			if (mesh->HasTangentsAndBitangents()) {
				vertex.tangent = { (float)mesh->mTangents[i].x, (float)mesh->mTangents[i].y, (float)mesh->mTangents[i].z, 1.f };
				float3 bt = float3((float)mesh->mBitangents[i].x, (float)mesh->mBitangents[i].y, (float)mesh->mBitangents[i].z);
				vertex.tangent.w = dot(cross(vertex.tangent.xyz, vertex.normal), bt) > 0.f ? 1.f : -1.f;
			}
			if (mesh->HasTextureCoords(0)) vertex.uv = { (float)mesh->mTextureCoords[0][i].x, (float)mesh->mTextureCoords[0][i].y };
			vertex.position *= scale;

			vertices.push_back(vertex);
			weights.push_back(AIWeight());
		}

		// index data
		float3 mn = vertices[baseVertex].position, mx = vertices[baseVertex].position;
		for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
			const aiFace& f = mesh->mFaces[i];
			indices.push_back(f.mIndices[0]);
			mn = min(vertices[baseVertex + f.mIndices[0]].position, mn);
			mx = max(vertices[baseVertex + f.mIndices[0]].position, mx);
			if (f.mNumIndices > 1) {
				indices.push_back(f.mIndices[1]);
				mn = min(vertices[baseVertex + f.mIndices[1]].position, mn);
				mx = max(vertices[baseVertex + f.mIndices[1]].position, mx);
				if (f.mNumIndices > 2) {
					indices.push_back(f.mIndices[2]);
					mn = min(vertices[baseVertex + f.mIndices[2]].position, mn);
					mx = max(vertices[baseVertex + f.mIndices[2]].position, mx);
				} else
					topo = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
			} else
				topo = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		}

		uint32_t vertexCount = (uint32_t)vertices.size() - baseVertex;
		uint32_t indexCount  = (uint32_t)indices.size() - baseIndex;

		TriangleBvh2* bvh = nullptr;
		if (topo == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) {
			bvh = new TriangleBvh2();
			bvh->Build(vertices.data() + baseVertex, 0, vertexCount, sizeof(StdVertex), indices.data() + baseIndex, indexCount, VK_INDEX_TYPE_UINT32);
		}

		if (mesh->HasBones()) {
			for (uint16_t c = 0; c < mesh->mNumBones; c++) {
				aiBone* bone = mesh->mBones[c];
				for (uint32_t i = 0; i < bone->mNumWeights; i++) {
					uint32_t index = baseVertex + bone->mWeights[i].mVertexId;
					weights[index].Set(bone->mName.C_Str(), (float)bone->mWeights[i].mWeight);
				}
				if (uniqueBones.count(bone->mName.C_Str()) == 0)
					uniqueBones.emplace(bone->mName.C_Str(), bone);
			}

			meshes.push_back(make_shared<Mesh>(mesh->mName.C_Str(), mInstance->Device(),
				AABB(mn, mx), bvh, vertexBuffer, indexBuffer, weightBuffer, baseVertex, vertexCount, baseIndex, indexCount,
				&StdVertex::VertexInput, VK_INDEX_TYPE_UINT32, topo));
		} else {
			meshes.push_back(make_shared<Mesh>(mesh->mName.C_Str(), mInstance->Device(),
				AABB(mn, mx), bvh, vertexBuffer, indexBuffer, baseVertex, vertexCount, baseIndex, indexCount,
				&StdVertex::VertexInput, VK_INDEX_TYPE_UINT32, topo));
		}
	}

	AnimationRig rig;

	if (uniqueBones.size()) {
		unordered_map<aiNode*, Bone*> boneMap;

		// find root node
		aiNode* root = scene->mRootNode;
		uint32_t rootDepth = 0xFFFFFFFF;
		for (auto& b : uniqueBones) {
			aiNode* node = scene->mRootNode->FindNode(b.second->mName);
			while (node && node->mName == aiString(""))
				node = node->mParent;
			uint32_t d = GetDepth(node);
			if (d < rootDepth) {
				rootDepth = d;

				while (node->mParent && node->mParent->mName == aiString(""))
					node = node->mParent;
				root = node->mParent;
			}
		}

		// compute bone matrices and bonesByName
		unordered_map<string, uint32_t> bonesByName;
		for (auto& b : uniqueBones) {
			aiNode* node = scene->mRootNode->FindNode(b.second->mName);
			Bone* bone = AddBone(rig, node, scene, root, boneMap, scale);
			if (!bone) continue;
			BoneTransform bt;
			ConvertMatrix(b.second->mOffsetMatrix).Decompose(&bt.mPosition, &bt.mRotation, &bt.mScale);
			bt.mPosition *= scale;
			bone->mInverseBind = float4x4::TRS(bt.mPosition, bt.mRotation, bt.mScale);
			bonesByName.emplace(b.second->mName.C_Str(), bone->mBoneIndex);
		}
		/*
		float4x4 rootTransform(1.f);
		while (root) {
			rootTransform = rootTransform * ConvertMatrix(root->mTransformation);
			root = root->mParent;
		}
		BoneTransform roott;
		rootTransform.Decompose(&roott.mPosition, &roott.mRotation, &roott.mScale);
		roott.mPosition *= scale;

		for (auto& b : rig) {
			if (!b->Parent()) {
				BoneTransform bt{
					b->LocalPosition(),
					b->LocalRotation(),
					b->LocalScale()
				};
				bt = roott * bt;
				b->LocalPosition(bt.mPosition);
				b->LocalRotation(bt.mRotation);
				b->LocalScale(bt.mScale);
			}
		}
		*/
		vector<VertexWeight> vertexWeights(vertices.size());
		for (uint32_t i = 0; i < vertices.size(); i++) {
			weights[i].Normalize();
			for (uint32_t j = 0; j < 4; j++) {
				if (bonesByName.count(weights[i].bones[j])) {
					vertexWeights[i].Indices[j] = bonesByName.at(weights[i].bones[j]);
					vertexWeights[i].Weights[j] = weights[i].weights[j];
				}
			}
		}

		weightBuffer->Upload(vertexWeights.data(), vertexWeights.size() * sizeof(VertexWeight));
	}

	vertexBuffer->Upload(vertices.data(), vertices.size() * sizeof(StdVertex));
	indexBuffer->Upload(indices.data(), indices.size() * sizeof(uint32_t));

	queue<pair<Object*, aiNode*>> nodes;
	nodes.push(make_pair((Object*)nullptr, scene->mRootNode));
	while (nodes.size()) {
		auto np = nodes.front();
		nodes.pop();
		aiNode* n = np.second;

		aiVector3D position;
		aiVector3D nscale;
		aiQuaternion rotation;
		n->mTransformation.Decompose(nscale, rotation, position);

		shared_ptr<Object> obj = make_shared<Object>(n->mName.C_Str());
		AddObject(obj);
		obj->LocalPosition(position.x * scale, position.y * scale, position.z * scale);
		obj->LocalRotation(quaternion(rotation.x, rotation.y, rotation.z, rotation.w));
		obj->LocalScale(nscale.x, nscale.y, nscale.z);

		if (np.first) np.first->AddChild(obj.get());
		else root = obj.get();

		objectSetupFunc(this, obj.get(), nullptr);

		objectMap.emplace(n, obj.get());

		for (uint32_t i = 0; i < n->mNumMeshes; i++) {
			shared_ptr<Mesh> mesh = meshes[n->mMeshes[i]];
			if (!mesh) continue;
			uint32_t mat = scene->mMeshes[n->mMeshes[i]]->mMaterialIndex;

			shared_ptr<MeshRenderer> mr;
			if (mesh->WeightBuffer()) {
				auto smr = make_shared<SkinnedMeshRenderer>(n->mName.C_Str() + mesh->mName);
				smr->Rig(rig);
				mr = smr;
			} else
				mr = make_shared<MeshRenderer>(n->mName.C_Str() + mesh->mName);
			
			AddObject(mr);
			mr->Material(mat < materials.size() ? materials[mat] : nullptr);
			mr->Mesh(mesh);
			obj->AddChild(mr.get());
			objectSetupFunc(this, mr.get(), scene->mMaterials[mat]);
		}

		for (uint32_t i = 0; i < n->mNumChildren; i++)
			nodes.push({ obj.get(), n->mChildren[i] });
	}

	const float minAttenuation = .001f; // min light attenuation for computing range from infinite attenuation

	for (uint32_t i = 0; i < scene->mNumLights; i++) {
		switch (scene->mLights[i]->mType) {
		case aiLightSource_DIRECTIONAL:
		{
			if (directionalLightIntensity <= 0) break;

			aiNode* lightNode = scene->mRootNode->FindNode(scene->mLights[i]->mName);
			aiVector3D position;
			aiVector3D nscale;
			aiQuaternion rotation;
			lightNode->mTransformation.Decompose(nscale, rotation, position);

			float3 col = float3(scene->mLights[i]->mColorDiffuse.r, scene->mLights[i]->mColorDiffuse.g, scene->mLights[i]->mColorDiffuse.b);
			float li = length(col);

			shared_ptr<Light> light = make_shared<Light>(scene->mLights[i]->mName.C_Str());
			light->LocalRotation(quaternion(rotation.x, rotation.y, rotation.z, rotation.w));
			light->Type(LIGHT_TYPE_SUN);
			light->Intensity(li * directionalLightIntensity);
			light->Color(col / li);
			light->CastShadows(true);
			AddObject(light);

			objectMap.at(lightNode->mParent)->AddChild(light.get());
			break;
		}
		case aiLightSource_SPOT:
		{
			if (spotLightIntensity <= 0) break;

			aiNode* lightNode = scene->mRootNode->FindNode(scene->mLights[i]->mName);
			aiVector3D position;
			aiVector3D nscale;
			aiQuaternion rotation;
			lightNode->mTransformation.Decompose(nscale, rotation, position);

			position += lightNode->mTransformation * scene->mLights[i]->mPosition;

			float a = scene->mLights[i]->mAttenuationQuadratic;
			float b = scene->mLights[i]->mAttenuationLinear;
			float c = scene->mLights[i]->mAttenuationConstant - (1 / minAttenuation);
			float3 col = float3(scene->mLights[i]->mColorDiffuse.r, scene->mLights[i]->mColorDiffuse.g, scene->mLights[i]->mColorDiffuse.b);
			float li = length(col);

			shared_ptr<Light> light = make_shared<Light>(scene->mLights[i]->mName.C_Str());
			light->LocalPosition(position.x * scale, position.y * scale, position.z * scale);
			light->LocalRotation(quaternion(rotation.x, rotation.y, rotation.z, rotation.w));
			light->Type(LIGHT_TYPE_SPOT);
			light->InnerSpotAngle(scene->mLights[i]->mAngleInnerCone);
			light->OuterSpotAngle(scene->mLights[i]->mAngleOuterCone);
			light->Intensity(li * spotLightIntensity);
			light->Range((-b + sqrtf(b * b - 4 * a * c)) / (2 * a));
			light->Color(col / li);
			AddObject(light);

			objectMap.at(lightNode->mParent)->AddChild(light.get());
			break;
		}
		case aiLightSource_POINT:
		{
			if (pointLightIntensity <= 0) break;

			aiNode* lightNode = scene->mRootNode->FindNode(scene->mLights[i]->mName);
			aiVector3D position;
			aiVector3D nscale;
			aiQuaternion rotation;
			lightNode->mTransformation.Decompose(nscale, rotation, position);

			position += lightNode->mTransformation * scene->mLights[i]->mPosition;

			float a = scene->mLights[i]->mAttenuationQuadratic;
			float b = scene->mLights[i]->mAttenuationLinear;
			float c = scene->mLights[i]->mAttenuationConstant - (1 / minAttenuation);
			float3 col = float3(scene->mLights[i]->mColorDiffuse.r, scene->mLights[i]->mColorDiffuse.g, scene->mLights[i]->mColorDiffuse.b);
			float li = length(col);

			shared_ptr<Light> light = make_shared<Light>(scene->mLights[i]->mName.C_Str());
			light->LocalPosition(position.x * scale, position.y* scale, position.z* scale);
			light->Type(LIGHT_TYPE_POINT);
			light->Intensity(li * pointLightIntensity);
			light->Range((-b + sqrtf(b*b - 4*a*c)) / (2 * a));
			light->Color(col / li);
			AddObject(light);

			objectMap.at(lightNode->mParent)->AddChild(light.get());
			break;
		}
		}
	}

	printf("Loaded %s\n", filename.c_str());
	return root;
}

void Scene::Update(CommandBuffer* commandBuffer) {
	auto t1 = mClock.now();
	mDeltaTime = (t1 - mLastFrame).count() * 1e-9f;
	mTotalTime = (t1 - mStartTime).count() * 1e-9f;
	mLastFrame = t1;

	// count fps
	mFrameTimeAccum += mDeltaTime;
	mFrameCount++;
	if (mFrameTimeAccum > 1.f) {
		mFps = mFrameCount / mFrameTimeAccum;
		mFrameTimeAccum -= 1.f;
		mFrameCount = 0;
	}

	PROFILER_BEGIN("FixedUpdate");
	float physicsTime = 0;
	mFixedAccumulator += mDeltaTime;
	t1 = mClock.now();
	while (mFixedAccumulator > mFixedTimeStep && physicsTime < mPhysicsTimeLimitPerFrame) {
		for (auto o : mObjects)
			if (o->EnabledHierarchy())
				o->FixedUpdate(commandBuffer);
		for (const auto& p : mPluginManager->Plugins())
			if (p->mEnabled)
				p->FixedUpdate(commandBuffer);

		mFixedAccumulator -= mFixedTimeStep;
		physicsTime = (mClock.now() - t1).count() * 1e-9f;
	}
	PROFILER_END;

	PROFILER_BEGIN("Update");
	for (const auto& p : mPluginManager->Plugins())
		if (p->mEnabled)
			p->PreUpdate(commandBuffer);

	for (const auto& p : mPluginManager->Plugins())
		if (p->mEnabled)
			p->Update(commandBuffer);

	for (const auto& p : mPluginManager->Plugins())
		if (p->mEnabled)
			p->PostUpdate(commandBuffer);
	PROFILER_END;

}

void Scene::PrePresent() {
	PROFILER_BEGIN("PrePresent");
	for (const auto& p : mPluginManager->Plugins())
		if (p->mEnabled)
			p->PrePresent();
	PROFILER_END;

}

void Scene::AddObject(shared_ptr<Object> object) {
	mObjects.push_back(object);
	object->mScene = this;

	if (auto l = dynamic_cast<Light*>(object.get()))
		mLights.push_back(l);
	if (auto c = dynamic_cast<Camera*>(object.get()))
		mCameras.push_back(c);
	if (auto r = dynamic_cast<Renderer*>(object.get()))
		mRenderers.push_back(r);

	mBvhDirty = true;
}
void Scene::RemoveObject(Object* object) {
	if (!object) return;

	if (auto l = dynamic_cast<Light*>(object))
		for (auto it = mLights.begin(); it != mLights.end();) {
			if (*it == l) {
				it = mLights.erase(it);
				break;
			} else
				it++;
		}

	if (auto c = dynamic_cast<Camera*>(object))
		for (auto it = mCameras.begin(); it != mCameras.end();) {
			if (*it == c) {
				it = mCameras.erase(it);
				break;
			} else
				it++;
		}

	if (auto r = dynamic_cast<Renderer*>(object))
		for (auto it = mRenderers.begin(); it != mRenderers.end();) {
			if (*it == r) {
				it = mRenderers.erase(it);
				break;
			} else
				it++;
		}

	for (auto it = mObjects.begin(); it != mObjects.end();)
		if (it->get() == object) {
			mBvhDirty = true;
			while (object->mChildren.size())
				object->RemoveChild(object->mChildren[0]);
			if (object->mParent) object->mParent->RemoveChild(object);
			object->mParent = nullptr;
			object->mScene = nullptr;
			it = mObjects.erase(it);
			break;
		} else
			it++;
}

void Scene::AddShadowCamera(uint32_t si, ShadowData* sd, bool ortho, float size, const float3& pos, const quaternion& rot, float near, float far) {
	if (mShadowCameras.size() <= si)
		mShadowCameras.push_back(new Camera("ShadowCamera", mShadowAtlasFramebuffer));
	Camera* sc = mShadowCameras[si];

	sc->Orthographic(ortho);
	if (ortho) sc->OrthographicSize(size);
	else sc->FieldOfView(size);
	sc->Near(near);
	sc->Far(far);
	sc->LocalPosition(pos);
	sc->LocalRotation(rot);

	sc->ViewportX((float)((si % (SHADOW_ATLAS_RESOLUTION / SHADOW_RESOLUTION)) * SHADOW_RESOLUTION));
	sc->ViewportY((float)((si / (SHADOW_ATLAS_RESOLUTION / SHADOW_RESOLUTION)) * SHADOW_RESOLUTION));
	sc->ViewportWidth(SHADOW_RESOLUTION);
	sc->ViewportHeight(SHADOW_RESOLUTION);

	sd->WorldToShadow = sc->ViewProjection();
	sd->CameraPosition = pos;
	sd->ShadowST = float4(sc->ViewportWidth() - 2, sc->ViewportHeight() - 2, sc->ViewportX() + 1, sc->ViewportY() + 1) / SHADOW_ATLAS_RESOLUTION;
	sd->InvProj22 = 1.f / (sc->Projection()[2][2] * (far - near));
};

void Scene::PreFrame(CommandBuffer* commandBuffer) {
	vkCmdSetLineWidth(*commandBuffer, 1.0f);
	
	PROFILER_BEGIN("Renderer PreFrame");
	for (Renderer* r : mRenderers)
		if (r->EnabledHierarchy())
			r->PreFrame(commandBuffer);
	PROFILER_END;

	Camera* mainCamera = nullptr;
	sort(mCameras.begin(), mCameras.end(), [](const auto& a, const auto& b) {
		return a->RenderPriority() > b->RenderPriority();
	});
	for (Camera* c : mCameras)
		if (c->EnabledHierarchy()) {
			mainCamera = c;
			break;
		}
	if (!mainCamera) return;

	if (!mBvh) {
		PROFILER_BEGIN("Sort Renderers");
		sort(mRenderers.begin(), mRenderers.end(), RendererCompare);
		PROFILER_END;
	}

	Device* device = commandBuffer->Device();
	PROFILER_BEGIN("Lighting");
	uint32_t si = 0;
	mShadowCount = 0;
	mActiveLights.clear();
	if (mainCamera && mLights.size()) {
		AABB sceneBounds;
		if (mBvh)
			sceneBounds = BVH()->RendererBounds();
		else
			for (Renderer* r : mRenderers)
				if (r->Visible()) sceneBounds.Encapsulate(r->Bounds());
		
		float3 sceneCenter = sceneBounds.Center();
		float3 sceneExtent = sceneBounds.Extents();
		float sceneExtentMax = max(max(sceneExtent.x, sceneExtent.y), sceneExtent.z) * 1.73205080757f; // sqrt(3)*x
		
		PROFILER_BEGIN("Gather Lights");
		uint32_t li = 0;
		uint32_t frameContextIndex = device->FrameContextIndex();
		GPULight* lights = (GPULight*)mLightBuffers[frameContextIndex]->MappedData();
		ShadowData* shadows = (ShadowData*)mShadowBuffers[frameContextIndex]->MappedData();

		uint32_t maxShadows = (SHADOW_ATLAS_RESOLUTION / SHADOW_RESOLUTION) * (SHADOW_ATLAS_RESOLUTION / SHADOW_RESOLUTION);

		float ct = tanf(mainCamera->FieldOfView() * .5f) * max(1.f, mainCamera->Aspect());
		float3 cp = mainCamera->WorldPosition();
		float3 fwd = mainCamera->WorldRotation().forward();

		Ray rays[4] {
			mainCamera->ScreenToWorldRay(float2(0, 0)),
			mainCamera->ScreenToWorldRay(float2(1, 0)),
			mainCamera->ScreenToWorldRay(float2(0, 1)),
			mainCamera->ScreenToWorldRay(float2(1, 1))
		};
		float3 corners[8];

		for (Light* l : mLights) {
			if (!l->EnabledHierarchy()) continue;
			mActiveLights.push_back(l);

			float cosInner = cosf(l->InnerSpotAngle());
			float cosOuter = cosf(l->OuterSpotAngle());

			lights[li].WorldPosition = l->WorldPosition();
			lights[li].InvSqrRange = 1.f / (l->Range() * l->Range());
			lights[li].Color = l->Color() * l->Intensity();
			lights[li].SpotAngleScale = 1.f / fmaxf(.001f, cosInner - cosOuter);
			lights[li].SpotAngleOffset = -cosOuter * lights[li].SpotAngleScale;
			lights[li].Direction = -l->WorldRotation().forward();
			lights[li].Type = l->Type();
			lights[li].ShadowIndex = -1;
			lights[li].CascadeSplits = -1.f;

			if (l->CastShadows() && si+1 < maxShadows) {
				switch (l->Type()) {
				case LIGHT_TYPE_SUN: {
					float4 cascadeSplits = 0;
					float cf = min(l->ShadowDistance(), mainCamera->Far());

					switch (l->CascadeCount()) {
					case 4:
						cascadeSplits[0] = cf * .07f;
						cascadeSplits[1] = cf * .18f;
						cascadeSplits[2] = cf * .40f;
						cascadeSplits[3] = cf;
					case 3:
						cascadeSplits[0] = cf * .15f;
						cascadeSplits[1] = cf * .4f;
						cascadeSplits[2] = cf;
						cascadeSplits[3] = cf;
					case 2:
						cascadeSplits[0] = cf * .4f;
						cascadeSplits[1] = cf;
						cascadeSplits[2] = cf;
						cascadeSplits[3] = cf;
					case 1:
						cascadeSplits = cf;
					}

					lights[li].CascadeSplits = cascadeSplits / mainCamera->Far();
					lights[li].ShadowIndex = (int32_t)si;
					
					float z0 = mainCamera->Near();
					for (uint32_t ci = 0; ci < l->CascadeCount(); ci++) {
						float z1 = cascadeSplits[ci];

						// compute corners and center of the frusum this cascade covers
						float3 pos = 0;
						for (uint32_t j = 0; j < 4; j++) {
							corners[j]   = rays[j].mOrigin + rays[j].mDirection * z0;
							corners[j+4] = rays[j].mOrigin + rays[j].mDirection * z1;
							pos += corners[j] + corners[j+4];
						}
						pos /= 8.f;

						// min and max relative to light rotation
						float3 mx = 0;
						float3 mn = 1e20f;
						quaternion r = inverse(l->WorldRotation());
						for (uint32_t j = 0; j < 8; j++) {
							float3 rc = r * (corners[j] - pos);
							mx = max(mx, rc);
							mn = min(mn, rc);
						}

						if (max(mx.x - mn.x, mx.y - mn.y) > sceneExtentMax) {
							// use scene bounds instead of frustum bounds
							pos = sceneCenter;
							corners[0] = float3(-sceneExtent.x,  sceneExtent.y, -sceneExtent.z) + sceneCenter;
							corners[1] = float3( sceneExtent.x,  sceneExtent.y, -sceneExtent.z) + sceneCenter;
							corners[2] = float3(-sceneExtent.x, -sceneExtent.y, -sceneExtent.z) + sceneCenter;
							corners[3] = float3( sceneExtent.x, -sceneExtent.y, -sceneExtent.z) + sceneCenter;
							corners[4] = float3(-sceneExtent.x,  sceneExtent.y,  sceneExtent.z) + sceneCenter;
							corners[5] = float3( sceneExtent.x,  sceneExtent.y,  sceneExtent.z) + sceneCenter;
							corners[6] = float3(-sceneExtent.x, -sceneExtent.y,  sceneExtent.z) + sceneCenter;
							corners[7] = float3( sceneExtent.x, -sceneExtent.y,  sceneExtent.z) + sceneCenter;
						}

						// project direction onto scene bounds for near and far
						float3 fwd   = l->WorldRotation() * float3(0, 0, 1);
						float3 right = l->WorldRotation() * float3(1, 0, 0);
						float near = 0;
						float far = 0;
						float sz = 0;
						for (uint32_t j = 0; j < 8; j++){
							float d = dot(corners[j] - pos, fwd);
							near = min(near, d);
							far = max(far, d);
							sz = max(sz, abs(dot(corners[j] - pos, right)));
						}

						AddShadowCamera(si, &shadows[si], true, 2*sz, pos, l->WorldRotation(), near, far);
						si++;
						z0 = z1;
					}

					break;
				}
				case LIGHT_TYPE_POINT:
					break;
				case LIGHT_TYPE_SPOT:
					lights[li].CascadeSplits = 1.f;
					lights[li].ShadowIndex = (int32_t)si;
					AddShadowCamera(si, &shadows[si], false, l->OuterSpotAngle() * 2, l->WorldPosition(), l->WorldRotation(), l->Radius() - .001f, l->Range());
					si++;
					break;
				}
			}

			li++;
			if (li >= MAX_GPU_LIGHTS) break;
		}
		PROFILER_END;
	}
	if (si) {
		PROFILER_BEGIN("Render Shadows");
		BEGIN_CMD_REGION(commandBuffer, "Render Shadows");

		bool g = mDrawGizmos;
		mDrawGizmos = false;
		for (uint32_t i = 0; i < si; i++) {
			mShadowCameras[i]->mEnabled = true;
			Render(commandBuffer, mShadowCameras[i], mShadowAtlasFramebuffer, PASS_DEPTH, i == 0);
			mShadowCount++;
		}
		for (uint32_t i = si; i < mShadowCameras.size(); i++)
			mShadowCameras[i]->mEnabled = false;
		mDrawGizmos = g;

		uint32_t fc = commandBuffer->Device()->FrameContextIndex();
		mShadowAtlases[fc]->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandBuffer);
		mShadowAtlasFramebuffer->ResolveDepth(commandBuffer, mShadowAtlases[fc]->Image());
		mShadowAtlases[fc]->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);

		END_CMD_REGION(commandBuffer);
		PROFILER_END;
	}
	mEnvironment->Update();
	PROFILER_END;

	Gizmos::PreFrame(this);
	GUI::PreFrame(this);
}

void Scene::Render(CommandBuffer* commandBuffer, Camera* camera, Framebuffer* framebuffer, PassType pass, bool clear) {
	PROFILER_BEGIN("Gather Renderers");
	mRenderList.clear();
	BVH()->FrustumCheck(camera->Frustum(), mRenderList, pass);
	PROFILER_END;
	PROFILER_BEGIN("Sort Renderers");
	sort(mRenderList.begin(), mRenderList.end(), RendererCompare);
	PROFILER_END;

	Render(commandBuffer, camera, framebuffer, pass, clear, mRenderList);
}

void Scene::Render(CommandBuffer* commandBuffer, Camera* camera, Framebuffer* framebuffer, PassType pass, bool clear, vector<Object*>& renderList) {
	camera->PreRender();
	if (camera->FramebufferWidth() == 0 || camera->FramebufferHeight() == 0)
		return;

	PROFILER_BEGIN("Environment PreRender");
	BEGIN_CMD_REGION(commandBuffer, "Environment PreRender");
	mEnvironment->PreRender(commandBuffer, camera);
	END_CMD_REGION(commandBuffer);
	PROFILER_END;

	PROFILER_BEGIN("Plugin PreRender");
	BEGIN_CMD_REGION(commandBuffer, "Plugin PreRender");
	for (const auto& p : mPluginManager->Plugins())
		if (p->mEnabled)
			p->PreRender(commandBuffer, camera, pass);
	END_CMD_REGION(commandBuffer);
	PROFILER_END;

	PROFILER_BEGIN("Renderer PreRender");
	BEGIN_CMD_REGION(commandBuffer, "Renderer PreRender");
	// renderer prerender
	for (Object* o : renderList)
		dynamic_cast<Renderer*>(o)->PreRender(commandBuffer, camera, pass);
	END_CMD_REGION(commandBuffer);
	PROFILER_END;

	PROFILER_BEGIN("Render");
	BEGIN_CMD_REGION(commandBuffer, "Render");

	PROFILER_BEGIN("Begin RenderPass");
	// begin renderpass
	if (!framebuffer) framebuffer = camera->Framebuffer();
	framebuffer->BeginRenderPass(commandBuffer);
	if (clear) framebuffer->Clear(commandBuffer);
	camera->Set(commandBuffer);
	PROFILER_END;

	PROFILER_BEGIN("Plugin PreRenderScene");
	for (const auto& p : mPluginManager->Plugins())
		if (p->mEnabled) p->PreRenderScene(commandBuffer, camera, pass);
	PROFILER_END;

	// skybox
	if (mEnvironment->mSkyboxMaterial && pass == PASS_MAIN) {
		PROFILER_BEGIN("Draw skybox");
		ShaderVariant* shader = mEnvironment->mSkyboxMaterial->GetShader(PASS_MAIN);
		VkPipelineLayout layout = commandBuffer->BindMaterial(mEnvironment->mSkyboxMaterial.get(), pass, mSkyboxCube->VertexInput(), camera, mSkyboxCube->Topology());
		commandBuffer->BindVertexBuffer(mSkyboxCube->VertexBuffer().get(), 0, 0);
		commandBuffer->BindIndexBuffer(mSkyboxCube->IndexBuffer().get(), 0, mSkyboxCube->IndexType());
		camera->SetStereo(commandBuffer, shader, EYE_LEFT);
		vkCmdDrawIndexed(*commandBuffer, mSkyboxCube->IndexCount(), 1, mSkyboxCube->BaseIndex(), mSkyboxCube->BaseVertex(), 0);
		commandBuffer->mTriangleCount += mSkyboxCube->IndexCount() / 3;
		if (camera->StereoMode() != STEREO_NONE) {
			camera->SetStereo(commandBuffer, shader, EYE_RIGHT);
			vkCmdDrawIndexed(*commandBuffer, mSkyboxCube->IndexCount(), 1, mSkyboxCube->BaseIndex(), mSkyboxCube->BaseVertex(), 0);
			commandBuffer->mTriangleCount += mSkyboxCube->IndexCount() / 3;
		}
		PROFILER_END;
	}

	#pragma region Render renderers
	uint32_t frameContextIndex = commandBuffer->Device()->FrameContextIndex();
	DescriptorSet* batchDS = nullptr;
	Buffer* batchBuffer = nullptr;
	InstanceBuffer* curBatch = nullptr;
	MeshRenderer* batchStart = nullptr;
	uint32_t batchSize = 0;

	auto DrawLastBatch = [&]() {
		if (batchStart) {
			PROFILER_BEGIN("Draw Batch");
			batchStart->DrawInstanced(commandBuffer, camera, batchSize, *batchDS, pass);
			batchStart = nullptr;
			PROFILER_END;
		}
	};
	for (Object* o : renderList) {
		Renderer* r = dynamic_cast<Renderer*>(o);
		bool batched = false;
		if (MeshRenderer* cur = dynamic_cast<MeshRenderer*>(r)) {
			GraphicsShader* curShader = cur->Material()->GetShader(pass);
			if (curShader->mDescriptorBindings.count("Instances")) {
				if (!batchStart || batchSize + 1 >= INSTANCE_BATCH_SIZE || (batchStart->Material() != cur->Material()) || batchStart->Mesh() != cur->Mesh()) {
					// render last batch
					DrawLastBatch();

					// start a new batch
					PROFILER_BEGIN("Start batch");
					batchSize = 0;
					batchStart = cur;

					batchBuffer = commandBuffer->Device()->GetTempBuffer("Instance Batch", sizeof(InstanceBuffer) * INSTANCE_BATCH_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
					curBatch = (InstanceBuffer*)batchBuffer->MappedData();

					batchDS = commandBuffer->Device()->GetTempDescriptorSet("Instance Batch", curShader->mDescriptorSetLayouts[PER_OBJECT]);
					batchDS->CreateStorageBufferDescriptor(batchBuffer, 0, batchBuffer->Size(), INSTANCE_BUFFER_BINDING);
					if (pass == PASS_MAIN) {
						if (curShader->mDescriptorBindings.count("Lights"))
							batchDS->CreateStorageBufferDescriptor(mLightBuffers[frameContextIndex], 0, mLightBuffers[frameContextIndex]->Size(), LIGHT_BUFFER_BINDING);
						if (curShader->mDescriptorBindings.count("Shadows"))
							batchDS->CreateStorageBufferDescriptor(mShadowBuffers[frameContextIndex], 0, mShadowBuffers[frameContextIndex]->Size(), SHADOW_BUFFER_BINDING);
						if (curShader->mDescriptorBindings.count("ShadowAtlas"))
							batchDS->CreateSampledTextureDescriptor(mShadowAtlases[frameContextIndex], SHADOW_ATLAS_BINDING);
					}
					batchDS->FlushWrites();

					PROFILER_END;
				}

				// append to batch
				PROFILER_BEGIN("Append to batch");
				curBatch[batchSize].ObjectToWorld = cur->ObjectToWorld();
				curBatch[batchSize].WorldToObject = cur->WorldToObject();
				batchSize++;
				batched = true;
				PROFILER_END;
			}
		}

		if (!batched) {
			// render last batch
			DrawLastBatch();
			PROFILER_BEGIN("Draw Unbatched");
			r->Draw(commandBuffer, camera, pass);
			PROFILER_END;
		}
	}
	// render last batch
	DrawLastBatch();
	#pragma endregion

	if (mDrawGizmos && pass == PASS_MAIN) {
		PROFILER_BEGIN("Draw Gizmos");
		BEGIN_CMD_REGION(commandBuffer, "Draw Gizmos");
		/*
		for (Camera* c : mShadowCameras)
			if (camera != c) {
				float3 f0 = c->ClipToWorld(float3(-1, -1, 0));
				float3 f1 = c->ClipToWorld(float3(-1, 1, 0));
				float3 f2 = c->ClipToWorld(float3(1, -1, 0));
				float3 f3 = c->ClipToWorld(float3(1, 1, 0));
				float3 f4 = c->ClipToWorld(float3(-1, -1, 1));
				float3 f5 = c->ClipToWorld(float3(-1, 1, 1));
				float3 f6 = c->ClipToWorld(float3(1, -1, 1));
				float3 f7 = c->ClipToWorld(float3(1, 1, 1));
				Gizmos::DrawLine(f0, f1, 1);
				Gizmos::DrawLine(f0, f2, 1);
				Gizmos::DrawLine(f3, f1, 1);
				Gizmos::DrawLine(f3, f2, 1);
				Gizmos::DrawLine(f4, f5, 1);
				Gizmos::DrawLine(f4, f6, 1);
				Gizmos::DrawLine(f7, f5, 1);
				Gizmos::DrawLine(f7, f6, 1);
				Gizmos::DrawLine(f0, f4, 1);
				Gizmos::DrawLine(f1, f5, 1);
				Gizmos::DrawLine(f2, f6, 1);
				Gizmos::DrawLine(f3, f7, 1);
			}
		*/

		if (mBvh) mBvh->DrawGizmos(commandBuffer, camera, this);

		for (const auto& r : mObjects)
			if (r->EnabledHierarchy())
				r->DrawGizmos(commandBuffer, camera);

		for (const auto& p : mPluginManager->Plugins())
			if (p->mEnabled)
				p->DrawGizmos(commandBuffer, camera);
		Gizmos::Draw(commandBuffer, pass, camera);
		END_CMD_REGION(commandBuffer);
		PROFILER_END;
	}

	if (pass == PASS_MAIN) {
		PROFILER_BEGIN("Draw GUI");
		GUI::Draw(commandBuffer, pass, camera);
		PROFILER_END;
	}

	camera->Set(commandBuffer);
	PROFILER_BEGIN("Plugin PostRenderScene");
	for (const auto& p : mPluginManager->Plugins())
		if (p->mEnabled) p->PostRenderScene(commandBuffer, camera, pass);
	PROFILER_END;


	PROFILER_BEGIN("End RenderPass");
	vkCmdEndRenderPass(*commandBuffer);
	PROFILER_END;

	END_CMD_REGION(commandBuffer);
	PROFILER_END;
}

vector<Object*> Scene::Objects() const {
	vector<Object*> objs(mObjects.size());
	for (uint32_t i = 0; i < mObjects.size(); i++)
		objs[i] = mObjects[i].get();
	return objs;
}

ObjectBvh2* Scene::BVH() {
	if (mBvh && mBvhDirty) {
		PROFILER_BEGIN("Build BVH");
		vector<Object*> objs = Objects();
		mBvh->Build(objs.data(), objs.size());
		mBvhDirty = false;
		mLastBvhBuild = mInstance->FrameCount();
		PROFILER_END;
	}
	return mBvh;
}