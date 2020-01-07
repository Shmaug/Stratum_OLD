#include <Scene/Scene.hpp>
#include <Scene/Renderer.hpp>
#include <Scene/MeshRenderer.hpp>
#include <Core/Instance.hpp>
#include <Util/Profiler.hpp>

#include <assimp/scene.h>
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>

using namespace std;

#define INSTANCE_BATCH_SIZE 4096
#define MAX_GPU_LIGHTS 64

#define SHADOW_ATLAS_RESOLUTION 8192
#define SHADOW_RESOLUTION 4096

Scene::Scene(::Instance* instance, ::AssetManager* assetManager, ::InputManager* inputManager, ::PluginManager* pluginManager)
	: mInstance(instance), mAssetManager(assetManager), mInputManager(inputManager), mPluginManager(pluginManager), mDrawGizmos(false) {
	mGizmos = new ::Gizmos(this);
	mShadowTexelSize = float2(1.f / SHADOW_ATLAS_RESOLUTION, 1.f / SHADOW_ATLAS_RESOLUTION) * .75f;
	mEnvironment = new ::Environment(this);
}
Scene::~Scene(){
	while (mObjects.size())
		RemoveObject(mObjects[0].get());

	safe_delete(mGizmos);
	safe_delete(mEnvironment);

	for (auto& kp : mDeviceData) {
		for (uint32_t i = 0; i < kp.first->MaxFramesInFlight(); i++) {
			safe_delete(kp.second.mLightBuffers[i]);
			safe_delete(kp.second.mShadowBuffers[i]);
		}
		safe_delete_array(kp.second.mLightBuffers);
		safe_delete_array(kp.second.mShadowBuffers);
		safe_delete(kp.second.mShadowAtlasFramebuffer);
		for (Camera* c : kp.second.mShadowCameras) safe_delete(c);
	}

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
		fprintf_color(Red, stderr, "Failed to open %s: %s\n", filename.c_str(), aiGetErrorString());
		throw;
	}

	Object* root = nullptr;

	vector<shared_ptr<Mesh>> meshes;
	vector<shared_ptr<Material>> materials;
	unordered_map<aiNode*, Object*> objectMap;

	vector<StdVertex> vertices;
	vector<uint32_t> indices;

	uint32_t totalVertices = 0;
	uint32_t totalIndices = 0;

	for (uint32_t m = 0; m < scene->mNumMeshes; m++) {
		const aiMesh* mesh = scene->mMeshes[m];
		totalVertices += mesh->mNumVertices;
		for (uint32_t i = 0; i < mesh->mNumFaces; i++)
			totalIndices += min(mesh->mFaces[i].mNumIndices, 3u);
	}

	vector<shared_ptr<Buffer>> vertexBuffers(mInstance->DeviceCount());
	vector<shared_ptr<Buffer>> indexBuffers (mInstance->DeviceCount());

	for (uint32_t i = 0; i < mInstance->DeviceCount(); i++) {
		vertexBuffers[i] = make_shared<Buffer>(scene->mRootNode->mName.C_Str() + string(" Vertices"), mInstance->GetDevice(i), sizeof(StdVertex) * totalVertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		indexBuffers[i]  = make_shared<Buffer>(scene->mRootNode->mName.C_Str() + string(" Indices") , mInstance->GetDevice(i), sizeof(uint32_t) * totalIndices  , VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	}

	for (uint32_t m = 0; m < scene->mNumMaterials; m++)
		materials.push_back(materialSetupFunc(this, scene->mMaterials[m]));

	for (uint32_t m = 0; m < scene->mNumMeshes; m++) {
		const aiMesh* mesh = scene->mMeshes[m];
		VkPrimitiveTopology topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		float3 mn, mx;

		uint32_t baseVertex = (uint32_t)vertices.size();
		uint32_t baseIndex  = (uint32_t)indices.size();

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

			if (i == 0)
				mn = mx = vertex.position;
			else {
				mn = min(vertex.position, mn);
				mx = max(vertex.position, mx);
			}
		}

		for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
			const aiFace& f = mesh->mFaces[i];
			indices.push_back((uint32_t)f.mIndices[0]);
			if (f.mNumIndices > 1) {
				indices.push_back((uint32_t)f.mIndices[1]);
				if (f.mNumIndices > 2)
					indices.push_back((uint32_t)f.mIndices[2]);
				else
					topo = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
			} else
				topo = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		}

		uint32_t vertexCount = (uint32_t)vertices.size() - baseVertex;
		uint32_t indexCount  = (uint32_t)indices.size() - baseIndex;

		meshes.push_back(make_shared<Mesh>(mesh->mName.C_Str(), mInstance,
			vertexBuffers.data(), indexBuffers.data(), AABB((mx + mn) * .5f, (mx - mn) * .5f), baseVertex, vertexCount, baseIndex, indexCount,
			&StdVertex::VertexInput, VK_INDEX_TYPE_UINT32, topo));
	}

	for (uint32_t i = 0; i < mInstance->DeviceCount(); i++) {
		vertexBuffers[i]->Upload(vertices.data(), vertices.size() * sizeof(StdVertex));
		indexBuffers[i]->Upload(indices.data(), indices.size() * sizeof(uint32_t));
	}

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
			uint32_t mat = scene->mMeshes[n->mMeshes[i]]->mMaterialIndex;

			shared_ptr<MeshRenderer> mr = make_shared<MeshRenderer>(n->mName.C_Str() + mesh->mName);
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
			light->Type(Sun);
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
			light->Type(Spot);
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
			light->Type(Point);
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

void Scene::Update() {
	PROFILER_BEGIN("Pre Update");
	for (const auto& p : mPluginManager->Plugins())
		if (p->mEnabled)
			p->PreUpdate();
	PROFILER_END;

	PROFILER_BEGIN("Update");
	for (const auto& p : mPluginManager->Plugins())
		if (p->mEnabled)
			p->Update();
	PROFILER_END;

	PROFILER_BEGIN("Post Update");
	for (const auto& p : mPluginManager->Plugins())
		if (p->mEnabled)
			p->PostUpdate();
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

void Scene::AddShadowCamera(DeviceData* dd, uint32_t si, ShadowData* sd, bool ortho, float size, const float3& pos, const quaternion& rot, float near, float far) {
	if (dd->mShadowCameras.size() <= si)
		dd->mShadowCameras.push_back(new Camera("ShadowCamera", dd->mShadowAtlasFramebuffer));
	Camera* sc = dd->mShadowCameras[si];

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
	sd->InvProj22 = 1.f / (sc->Projection()[2][2] * (sc->Far() - sc->Near()));
};

void Scene::PreFrame(CommandBuffer* commandBuffer) {
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

	Device* device = commandBuffer->Device();
	
	if (mDeviceData.count(device) == 0) {
		DeviceData& data = mDeviceData[device];
		data.mShadowAtlasFramebuffer = new Framebuffer("ShadowAtlas", device, SHADOW_ATLAS_RESOLUTION, SHADOW_ATLAS_RESOLUTION, {}, 
			VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, {}, VK_ATTACHMENT_LOAD_OP_LOAD);
		uint32_t c = device->MaxFramesInFlight();
		data.mLightBuffers = new Buffer*[c];
		data.mShadowBuffers = new Buffer*[c];
		for (uint32_t i = 0; i < c; i++) {
			data.mLightBuffers[i] = new Buffer("Light Buffer", device, MAX_GPU_LIGHTS * sizeof(GPULight), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			data.mShadowBuffers[i] = new Buffer("Shadow Buffer", device, MAX_GPU_LIGHTS * sizeof(ShadowData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			data.mLightBuffers[i]->Map();
			data.mShadowBuffers[i]->Map();
		}
	}
	DeviceData& data = mDeviceData.at(device);

	PROFILER_BEGIN("Lighting");
	uint32_t si = 0;

	mActiveLights.clear();
	if (mLights.size()) {
		PROFILER_BEGIN("Gather Lights");
		uint32_t li = 0;
		uint32_t frameContextIndex = device->FrameContextIndex();
		GPULight* lights = (GPULight*)data.mLightBuffers[frameContextIndex]->MappedData();
		ShadowData* shadows = (ShadowData*)data.mShadowBuffers[frameContextIndex]->MappedData();

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
				case Sun: {
					float4 cascadeSplits = 0;
					float cf = min(l->ShadowDistance(), mainCamera->Far());
					cascadeSplits[0] = .10f * cf;
					cascadeSplits[1] = .30f * cf;
					cascadeSplits[2] = .60f * cf;
					cascadeSplits[3] = cf;

					lights[li].CascadeSplits = cascadeSplits / mainCamera->Far();
					lights[li].ShadowIndex = (int32_t)si;
					
					float z0 = mainCamera->Near();
					for (uint32_t ci = 0; ci < 4; ci++) {
						float z1 = cascadeSplits[ci];

						float3 center = 0;
						float3 mx = 0;
						float3 mn = 1e20f;
						quaternion r = inverse(l->WorldRotation());
						
						for (uint32_t j = 0; j < 4; j++) {
							corners[j]   = rays[j].mOrigin + rays[j].mDirection * z0;
							corners[2*j] = rays[j].mOrigin + rays[j].mDirection * z1;
							center += corners[j] + corners[2 * j];
						}
						for (uint32_t j = 0; j < 8; j++) {
							corners[j] = r * (corners[j] - center);
							mx = max(mx, corners[j]);
							mn = min(mn, corners[j]);
						}

						float3 ext = mx - mn;
						float sz = max(ext.x, ext.y);
						AddShadowCamera(&data, si, &shadows[si], true, sz, center / 8, l->WorldRotation(), -5*sz, sz);
						si++;
						z0 = z1;
					}

					break;
				}
				case Point:
					break;
				case Spot:
					lights[li].ShadowIndex = (int32_t)si;
					AddShadowCamera(&data, si, &shadows[si], false, l->OuterSpotAngle() * 2, l->WorldPosition(), l->WorldRotation(), l->Radius() - .001f, l->Range());
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
			data.mShadowCameras[i]->mEnabled = true;
			Render(commandBuffer, data.mShadowCameras[i], data.mShadowAtlasFramebuffer, PASS_DEPTH, i == 0);
		}
		for (uint32_t i = si; i < data.mShadowCameras.size(); i++)
			data.mShadowCameras[i]->mEnabled = false;
		mDrawGizmos = g;

		data.mShadowAtlasFramebuffer->ResolveDepth(commandBuffer);

		END_CMD_REGION(commandBuffer);
		PROFILER_END;
	}

	PROFILER_END;

	mGizmos->PreFrame(device);
	mEnvironment->Update();
}

void Scene::Render(CommandBuffer* commandBuffer, Camera* camera, Framebuffer* framebuffer, PassType pass, bool clear) {
	DeviceData& data = mDeviceData.at(commandBuffer->Device());
	
	PROFILER_BEGIN("Pre Render");
	camera->PreRender();
	if (camera->FramebufferWidth() == 0 || camera->FramebufferHeight() == 0) {
		PROFILER_END;
		return;
	}
	if (pass == PASS_MAIN) {
		if (camera->DepthFramebuffer()) {
			PROFILER_BEGIN("Depth Prepass");
			BEGIN_CMD_REGION(commandBuffer, "Depth Prepass");
			Render(commandBuffer, camera, camera->DepthFramebuffer(), PASS_DEPTH);
			END_CMD_REGION(commandBuffer);
			PROFILER_END;
		}
		mEnvironment->PreRender(commandBuffer, camera);
	}
	for (const auto& p : mPluginManager->Plugins())
		if (p->mEnabled)
			p->PreRender(commandBuffer, camera, pass);
	PROFILER_END;
	
	PROFILER_BEGIN("Gather/Sort Renderers");
	mRenderList.clear();
	for (Renderer* r : mRenderers)
		if (r->Visible() && camera->IntersectFrustum(r->Bounds()) && (r->PassMask() & pass))
			mRenderList.push_back(r);
	// combine MeshRenderers that have the same material and mesh
	sort(mRenderList.begin(), mRenderList.end(), [](Renderer* a, Renderer* b) {
		if (a->RenderQueue() == b->RenderQueue())
			if (MeshRenderer* ma = dynamic_cast<MeshRenderer*>(a))
				if (MeshRenderer* mb = dynamic_cast<MeshRenderer*>(b))
					if (ma->Material() == mb->Material())
						return ma->Mesh() < mb->Mesh();
					else
						return ma->Material() < mb->Material();
		return a->RenderQueue() < b->RenderQueue();
	});
	PROFILER_END;
	for (Renderer* r : mRenderList)
		r->PreRender(commandBuffer, camera, pass);

	PROFILER_BEGIN("Draw");
	BEGIN_CMD_REGION(commandBuffer, "Draw Scene");
	framebuffer->BeginRenderPass(commandBuffer);
	if (clear) framebuffer->Clear(commandBuffer);
	camera->Set(commandBuffer);

	uint32_t frameContextIndex = commandBuffer->Device()->FrameContextIndex();
	DescriptorSet* batchDS = nullptr;
	Buffer* batchBuffer = nullptr;
	InstanceBuffer* curBatch = nullptr;
	MeshRenderer* batchStart = nullptr;
	uint32_t batchSize = 0;

	for (Renderer* r : mRenderList) {
		bool batched = false;
		MeshRenderer* cur = dynamic_cast<MeshRenderer*>(r);
		if (cur) {
			GraphicsShader* curShader = cur->Material()->GetShader(commandBuffer->Device(), pass);
			if (curShader->mDescriptorBindings.count("Instances")) {
				if (!batchStart || batchSize + 1 >= INSTANCE_BATCH_SIZE || (batchStart->Material() != cur->Material()) || batchStart->Mesh() != cur->Mesh()) {
					// render last batch
					if (batchStart) {
						PROFILER_BEGIN_RESUME("Draw Batched");
						BEGIN_CMD_REGION(commandBuffer, "Draw " + batchStart->mName);
						batchStart->DrawInstanced(commandBuffer, camera, batchSize, *batchDS, pass);
						END_CMD_REGION(commandBuffer);
						PROFILER_END;
					}

					// start a new batch
					PROFILER_BEGIN_RESUME("Start Batch");
					batchSize = 0;
					batchStart = cur;
					
					batchBuffer = commandBuffer->Device()->GetTempBuffer("Instance Batch", sizeof(InstanceBuffer) * INSTANCE_BATCH_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
					curBatch = (InstanceBuffer*)batchBuffer->MappedData();

					batchDS = commandBuffer->Device()->GetTempDescriptorSet("Instance Batch", curShader->mDescriptorSetLayouts[PER_OBJECT]);

					batchDS->CreateStorageBufferDescriptor(batchBuffer, 0, batchBuffer->Size(), INSTANCE_BUFFER_BINDING);
					if (pass == PASS_MAIN) {
						if (curShader->mDescriptorBindings.count("Lights"))
							batchDS->CreateStorageBufferDescriptor(data.mLightBuffers[frameContextIndex], 0, data.mLightBuffers[frameContextIndex]->Size(), LIGHT_BUFFER_BINDING);
						if (curShader->mDescriptorBindings.count("Shadows"))
							batchDS->CreateStorageBufferDescriptor(data.mShadowBuffers[frameContextIndex], 0, data.mShadowBuffers[frameContextIndex]->Size(), SHADOW_BUFFER_BINDING);
						if (curShader->mDescriptorBindings.count("ShadowAtlas"))
							batchDS->CreateSampledTextureDescriptor(data.mShadowAtlasFramebuffer->DepthBuffer(), SHADOW_ATLAS_BINDING);
					}

					batchDS->FlushWrites();
					PROFILER_END;

				}
				// append to batch
				PROFILER_BEGIN_RESUME("Append Batch");
				curBatch[batchSize].ObjectToWorld = cur->ObjectToWorld();
				curBatch[batchSize].WorldToObject = cur->WorldToObject();
				batchSize++;
				batched = true;
				PROFILER_END;
			}
		}

		if (!batched) {
			// render last batch
			if (batchStart) {
				PROFILER_BEGIN_RESUME("Draw Batched");
				BEGIN_CMD_REGION(commandBuffer, "Draw " + batchStart->mName);
				batchStart->DrawInstanced(commandBuffer, camera, batchSize, *batchDS, pass);
				END_CMD_REGION(commandBuffer);
				batchStart = nullptr;
				PROFILER_END;
			}
			PROFILER_BEGIN_RESUME("Draw Unbatched");
			BEGIN_CMD_REGION(commandBuffer, "Draw " + r->mName);
			r->Draw(commandBuffer, camera, pass);
			END_CMD_REGION(commandBuffer);
			PROFILER_END;
		}
	}
	// render last batch
	if (batchStart) {
		PROFILER_BEGIN_RESUME("Draw Batched");
		BEGIN_CMD_REGION(commandBuffer, "Draw " + batchStart->mName);
		batchStart->DrawInstanced(commandBuffer, camera, batchSize, *batchDS, pass);
		END_CMD_REGION(commandBuffer);
		PROFILER_END;
	}


	if (mDrawGizmos && pass == PASS_MAIN) {
		PROFILER_BEGIN("Draw Gizmos");
		/*
		for (Camera* c : data.mShadowCameras)
			if (camera != c) {
				float3 f0 = c->ClipToWorld(float3(-1, -1, 0));
				float3 f1 = c->ClipToWorld(float3(-1, 1, 0));
				float3 f2 = c->ClipToWorld(float3(1, -1, 0));
				float3 f3 = c->ClipToWorld(float3(1, 1, 0));
				float3 f4 = c->ClipToWorld(float3(-1, -1, 1));
				float3 f5 = c->ClipToWorld(float3(-1, 1, 1));
				float3 f6 = c->ClipToWorld(float3(1, -1, 1));
				float3 f7 = c->ClipToWorld(float3(1, 1, 1));
				mGizmos->DrawLine(f0, f1, 1);
				mGizmos->DrawLine(f0, f2, 1);
				mGizmos->DrawLine(f3, f1, 1);
				mGizmos->DrawLine(f3, f2, 1);
				mGizmos->DrawLine(f4, f5, 1);
				mGizmos->DrawLine(f4, f6, 1);
				mGizmos->DrawLine(f7, f5, 1);
				mGizmos->DrawLine(f7, f6, 1);
				mGizmos->DrawLine(f0, f4, 1);
				mGizmos->DrawLine(f1, f5, 1);
				mGizmos->DrawLine(f2, f6, 1);
				mGizmos->DrawLine(f3, f7, 1);
			}
		*/
		for (const auto& r : mObjects)
			if (r->EnabledHierarchy())
				r->DrawGizmos(commandBuffer, camera);

		for (const auto& p : mPluginManager->Plugins())
			if (p->mEnabled)
				p->DrawGizmos(commandBuffer, camera);
		BEGIN_CMD_REGION(commandBuffer, "Draw Gizmos");
		mGizmos->Draw(commandBuffer, pass, camera);
		END_CMD_REGION(commandBuffer);
		PROFILER_END;
	}
	
	vkCmdEndRenderPass(*commandBuffer);

	END_CMD_REGION(commandBuffer);
	PROFILER_END;

	// Post Render
	PROFILER_BEGIN("Post Render");
	for (const auto& p : mPluginManager->Plugins())
		if (p->mEnabled) p->PostRender(commandBuffer, camera, pass);
	PROFILER_END;
}

Collider* Scene::Raycast(const Ray& ray, float& hitT, uint32_t mask) {
	Collider* closest = nullptr;
	hitT = -1.f;

	for (const shared_ptr<Object>& n : mObjects) {
		if (n->EnabledHierarchy()) {
			if (Collider* c = dynamic_cast<Collider*>(n.get())) {
				if ((c->CollisionMask() & mask) != 0) {
					float t;
					if (c->Intersect(ray, &t) && t > 0 && (t < hitT || closest == nullptr)) {
						closest = c;
						hitT = t;
					}
				}
			}
		}
	}

	return closest;
}