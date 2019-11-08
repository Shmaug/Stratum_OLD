#include <Scene/Scene.hpp>
#include <Scene/MeshRenderer.hpp>
#include <Core/DeviceManager.hpp>
#include <Util/Profiler.hpp>

using namespace std;

#define INSTANCE_BATCH_SIZE 1024

Scene::Scene(::DeviceManager* deviceManager, ::AssetManager* assetManager, ::InputManager* inputManager, ::PluginManager* pluginManager)
	: mDeviceManager(deviceManager), mAssetManager(assetManager), mInputManager(inputManager), mPluginManager(pluginManager), mDrawGizmos(false) {
	mGizmos = new ::Gizmos(this);
	mShadowMaterial = make_shared<Material>("Shadow", mAssetManager->LoadShader("Shaders/shadow.shader"));
}
Scene::~Scene(){
	for (auto& kp : mDeviceData) {
		for (uint32_t i = 0; i < kp.first->MaxFramesInFlight(); i++) {
			safe_delete(kp.second.mLightBuffers[i]);
			safe_delete(kp.second.mShadowBuffers[i]);
			for (Buffer* b : kp.second.mInstanceBuffers[i])
				safe_delete(b);
			for (DescriptorSet* d : kp.second.mInstanceDescriptorSets[i])
				safe_delete(d);
		}
		safe_delete_array(kp.second.mLightBuffers);
		safe_delete_array(kp.second.mShadowBuffers);
		safe_delete_array(kp.second.mInstanceIndex);
		safe_delete_array(kp.second.mInstanceBuffers);
		safe_delete_array(kp.second.mInstanceDescriptorSets);
		safe_delete(kp.second.mShadowAtlasFramebuffer);
		for (Camera* c : kp.second.mShadowCameras) safe_delete(c);
	}
	safe_delete(mGizmos);

	while (mObjects.size())
		RemoveObject(mObjects[0].get());

	mCameras.clear();
	mRenderers.clear();
	mLights.clear();
	mObjects.clear();
}

void Scene::Update(const FrameTime& frameTime) {
	PROFILER_BEGIN("Pre Update");
	for (const auto& p : mPluginManager->Plugins())
		if (p->mEnabled)
			p->PreUpdate(frameTime);
	PROFILER_END;

	PROFILER_BEGIN("Update");
	for (const auto& p : mPluginManager->Plugins())
		if (p->mEnabled)
			p->Update(frameTime);
	PROFILER_END;

	PROFILER_BEGIN("Post Update");
	for (const auto& p : mPluginManager->Plugins())
		if (p->mEnabled)
			p->PostUpdate(frameTime);
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

void Scene::PreFrame(CommandBuffer* commandBuffer, uint32_t backBufferIndex) {
	Camera* mainCamera = nullptr;
	for (Camera* c : mCameras)
		if (c->EnabledHierarchy()) {
			mainCamera = c;
			break;
		}
	if (!mainCamera) return;

	Device* device = commandBuffer->Device();
	
	if (mDeviceData.count(device) == 0) {
		DeviceData& data = mDeviceData[device];
		uint32_t c = device->MaxFramesInFlight();
		data.mLightBuffers = new Buffer*[c];
		data.mShadowBuffers = new Buffer*[c];
		data.mInstanceBuffers = new vector<Buffer*>[c];
		data.mInstanceDescriptorSets = new vector<DescriptorSet*>[c];
		data.mInstanceIndex = new uint32_t[c];
		memset(data.mLightBuffers, 0, sizeof(Buffer*) * c);
		memset(data.mShadowBuffers, 0, sizeof(Buffer*) * c);
		memset(data.mInstanceBuffers, 0, sizeof(vector<Buffer*>) * c);
		memset(data.mInstanceDescriptorSets, 0, sizeof(vector<DescriptorSet*>) * c);
		data.mShadowAtlasFramebuffer = new Framebuffer("ShadowAtlas", device, 8192, 8192, { VK_FORMAT_R8G8B8A8_UNORM }, VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT);
	}
	DeviceData& data = mDeviceData.at(device);
	data.mInstanceIndex[backBufferIndex] = 0;

	PROFILER_BEGIN("Gather Lights");
	Buffer*& lb = data.mLightBuffers[backBufferIndex];
	if (lb && lb->Size() < mLights.size() * sizeof(GPULight)) safe_delete(lb);
	if (!lb) {
		lb = new Buffer("Light Buffer", device, max(1u, (uint32_t)mLights.size()) * (uint32_t)sizeof(GPULight), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		lb->Map();
	}

	Buffer*& sb = data.mShadowBuffers[backBufferIndex];
	if (sb && sb->Size() < mLights.size() * sizeof(ShadowData)) safe_delete(sb);
	if (!sb) {
		sb = new Buffer("Shadow Buffer", device, max(1u, (uint32_t)mLights.size()) * (uint32_t)sizeof(ShadowData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		sb->Map();
	}

	mActiveLights.clear();
	if (mLights.size()) {
		uint32_t si = 0;
		uint32_t li = 0;
		GPULight* lights = (GPULight*)lb->MappedData();
		ShadowData* shadows = (ShadowData*)sb->MappedData();
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
			lights[li].Direction = l->WorldRotation() * float3(0, 0, -1);
			lights[li].Type = l->Type();

			if (l->CastShadows()) {
				Camera* sc;
				if (data.mShadowCameras.size() <= si) {
					sc = new Camera("ShadowCamera", data.mShadowAtlasFramebuffer);
					sc->Framebuffer()->ColorBuffer(backBufferIndex, 0)->TransitionImageLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
					data.mShadowCameras.push_back(sc);
				} else
					sc = data.mShadowCameras[si];

				switch (l->Type()) {
				case Sun:
					sc->Orthographic(true);
					sc->OrthographicSize(20);
					sc->Near(1);
					sc->Far(100);
					sc->LocalRotation(l->WorldRotation());
					sc->LocalPosition(mainCamera->WorldPosition() + l->WorldRotation() * float3(0, 0, -50));
					break;
				case Point:
					break;
				case Spot:
					sc->Orthographic(false);
					sc->FieldOfView(l->OuterSpotAngle()*2);
					sc->Near(.001f);
					sc->Far(l->Range());
					sc->LocalRotation(l->WorldRotation());
					sc->LocalPosition(l->WorldPosition());
					break;
				}

				sc->ViewportX((si % 4) * 2048);
				sc->ViewportY((si / 4) * 2048);
				sc->ViewportWidth(2048);
				sc->ViewportHeight(2048);

				shadows[si].WorldToShadow = sc->ViewProjection();
				shadows[si].ShadowST = float4(sc->ViewportWidth(), sc->ViewportHeight(), sc->ViewportX(), sc->ViewportY()) / float4(sc->FramebufferWidth(), sc->FramebufferHeight(), sc->FramebufferWidth(), sc->FramebufferHeight());
				shadows[si].Proj = float4(sc->Orthographic() ? 1 : 0, sc->Near(), sc->Far());
				lights[li].ShadowIndex = (int32_t)si;
				si++;
			} else
				lights[li].ShadowIndex = -1;

			li++;
		}

		if (si > 0) {
			PROFILER_BEGIN("Render Shadows");
			data.mShadowAtlasFramebuffer->ColorBuffer(backBufferIndex, 0)->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, commandBuffer);
			data.mShadowAtlasFramebuffer->BeginRenderPass(commandBuffer, backBufferIndex);

			bool g = mDrawGizmos;
			mDrawGizmos = false;
			for (uint32_t i = 0; i < si; i++)
				Render(data.mShadowCameras[i], commandBuffer, backBufferIndex, mShadowMaterial.get(), false);
			mDrawGizmos = g;

			vkCmdEndRenderPass(*commandBuffer);
			data.mShadowAtlasFramebuffer->ColorBuffer(backBufferIndex, 0)->TransitionImageLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
			PROFILER_END;
		}
	}
	PROFILER_END;

	mGizmos->PreFrame(device, backBufferIndex);
}

void Scene::Render(Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, Material* materialOverride, bool startRenderPass) {
	DeviceData& data = mDeviceData.at(commandBuffer->Device());
	
	PROFILER_BEGIN("Gather/Sort Renderers");
	mRenderList.clear();
	for (Renderer* r : mRenderers)
		if (r->Visible() && camera->IntersectFrustum(r->Bounds()) && (materialOverride != mShadowMaterial.get() || r->CastShadows()))
			mRenderList.push_back(r);
	sort(mRenderList.begin(), mRenderList.end(), [](Renderer* a, Renderer* b) {
		if (a->RenderQueue() == b->RenderQueue())
			if (MeshRenderer* ma = dynamic_cast<MeshRenderer*>(a))
				if (MeshRenderer* mb = dynamic_cast<MeshRenderer*>(b))
					if (ma->Mesh() == mb->Mesh())
						return ma->Material().get() < mb->Material().get();
					else
						return ma->Mesh() < mb->Mesh();
		return a->RenderQueue() < b->RenderQueue();
	});
	PROFILER_END;

	camera->PreRender();
	
	PROFILER_BEGIN("Pre Render");
	for (const auto& p : mPluginManager->Plugins())
		if (p->mEnabled)
			p->PreRender(commandBuffer, backBufferIndex, camera);
	for (Renderer* r : mRenderList)
		r->PreRender(commandBuffer, backBufferIndex, camera, materialOverride);
	PROFILER_END;

	// combine MeshRenderers that have the same material and mesh
	PROFILER_BEGIN("Draw");
	BEGIN_CMD_REGION(commandBuffer, "Draw Scene");
	if (startRenderPass)
		camera->Framebuffer()->BeginRenderPass(commandBuffer, backBufferIndex);
	camera->Set(commandBuffer, backBufferIndex);
	
	DescriptorSet* batchDS = nullptr;
	Buffer* batchBuffer = nullptr;

	MeshRenderer* batchStart = nullptr;
	uint32_t batchSize = 0;

	for (Renderer* r : mRenderList) {
		bool batched = false;
		MeshRenderer* cur = dynamic_cast<MeshRenderer*>(r);
		if (cur) {
			GraphicsShader* curShader = materialOverride ? materialOverride->GetShader(commandBuffer->Device()) : cur->Material()->GetShader(commandBuffer->Device());
			if (curShader->mDescriptorBindings.count("Instances")) {
				if (!batchStart || batchSize + 1 >= INSTANCE_BATCH_SIZE ||
					(batchStart->Material() != cur->Material() && !materialOverride) || batchStart->Mesh() != cur->Mesh()) {
					// render last batch
					if (batchStart) {
						BEGIN_CMD_REGION(commandBuffer, "Draw " + batchStart->mName);
						batchStart->DrawInstanced(commandBuffer, backBufferIndex, camera, batchSize, *batchDS, materialOverride);
						END_CMD_REGION(commandBuffer);
					}

					// start a new batch
					batchSize = 0;
					batchStart = cur;

					if (data.mInstanceIndex[backBufferIndex] >= data.mInstanceDescriptorSets[backBufferIndex].size()) {
						// create a new buffer and descriptorset
						batchBuffer = new Buffer("Instance Batch", commandBuffer->Device(), sizeof(ObjectBuffer) * INSTANCE_BATCH_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
						batchBuffer->Map();
						data.mInstanceBuffers[backBufferIndex].push_back(batchBuffer);

						batchDS = new DescriptorSet("Instance Batch", commandBuffer->Device()->DescriptorPool(), curShader->mDescriptorSetLayouts[PER_OBJECT]);
						batchDS->CreateStorageBufferDescriptor(batchBuffer, OBJECT_BUFFER_BINDING);
						data.mInstanceDescriptorSets[backBufferIndex].push_back(batchDS);
					} else {
						// fetch existing buffer and descriptorset
						batchBuffer = data.mInstanceBuffers[backBufferIndex][data.mInstanceIndex[backBufferIndex]];
						batchDS = data.mInstanceDescriptorSets[backBufferIndex][data.mInstanceIndex[backBufferIndex]];
						if (batchDS->Layout() != curShader->mDescriptorSetLayouts[PER_OBJECT]) {
							// update descriptorset to have correct layout
							safe_delete(batchDS);
							batchDS = new DescriptorSet("Instance Batch", commandBuffer->Device()->DescriptorPool(), curShader->mDescriptorSetLayouts[PER_OBJECT]);
							batchDS->CreateStorageBufferDescriptor(batchBuffer, OBJECT_BUFFER_BINDING);
							data.mInstanceDescriptorSets[backBufferIndex][data.mInstanceIndex[backBufferIndex]] = batchDS;
						}
					}
					data.mInstanceIndex[backBufferIndex]++;

					if (curShader->mDescriptorBindings.count("Lights"))
						batchDS->CreateStorageBufferDescriptor(data.mLightBuffers[backBufferIndex], LIGHT_BUFFER_BINDING);
					if (curShader->mDescriptorBindings.count("Shadows"))
						batchDS->CreateStorageBufferDescriptor(data.mShadowBuffers[backBufferIndex], SHADOW_BUFFER_BINDING);
					if (curShader->mDescriptorBindings.count("ShadowAtlas"))
						batchDS->CreateSampledTextureDescriptor(data.mShadowAtlasFramebuffer->ColorBuffer(backBufferIndex, 0), SHADOW_ATLAS_BINDING);
				}
				// append to batch
				((ObjectBuffer*)batchBuffer->MappedData())[batchSize].ObjectToWorld = cur->ObjectToWorld();
				((ObjectBuffer*)batchBuffer->MappedData())[batchSize].WorldToObject = cur->WorldToObject();
				batchSize++;
				batched = true;
			}
		}

		if (!batched) {
			// render last batch
			if (batchStart) {
				BEGIN_CMD_REGION(commandBuffer, "Draw " + batchStart->mName);
				batchStart->DrawInstanced(commandBuffer, backBufferIndex, camera, batchSize, *batchDS, materialOverride);
				END_CMD_REGION(commandBuffer);
				batchStart = nullptr;
			}
			BEGIN_CMD_REGION(commandBuffer, "Draw " + r->mName);
			r->Draw(commandBuffer, backBufferIndex, camera, materialOverride);
			END_CMD_REGION(commandBuffer);
		}
	}
	// render last batch
	if (batchStart) {
		BEGIN_CMD_REGION(commandBuffer, "Draw " + batchStart->mName);
		batchStart->DrawInstanced(commandBuffer, backBufferIndex, camera, batchSize, *batchDS, materialOverride);
		END_CMD_REGION(commandBuffer);
	}
	PROFILER_END;

	if (mDrawGizmos) {
		PROFILER_BEGIN("Draw Gizmos");
		BEGIN_CMD_REGION(commandBuffer, "Draw Gizmos");
		for (const auto& r : mObjects)
			if (r->EnabledHierarchy()) {
				BEGIN_CMD_REGION(commandBuffer, "Gizmos " + r->mName);
				r->DrawGizmos(commandBuffer, backBufferIndex, camera);
				END_CMD_REGION(commandBuffer);
			}

		for (const auto& p : mPluginManager->Plugins())
			if (p->mEnabled)
				p->DrawGizmos(commandBuffer, backBufferIndex, camera);
		mGizmos->Draw(commandBuffer, backBufferIndex, camera);
		END_CMD_REGION(commandBuffer);
		PROFILER_END;
	}

	if (startRenderPass)
		vkCmdEndRenderPass(*commandBuffer);
	
	END_CMD_REGION(commandBuffer);
	PROFILER_END;

	// Post Render
	PROFILER_BEGIN("Post Render");
	for (const auto& p : mPluginManager->Plugins())
		if (p->mEnabled)
			p->PostRender( commandBuffer, backBufferIndex, camera);
	PROFILER_END;

	camera->ResolveWindow(commandBuffer, backBufferIndex);
}

Collider* Scene::Raycast(const Ray& ray, float& hitT, uint32_t mask) {
	Collider* closest = nullptr;
	hitT = -1.f;

	queue<Object*> nodes;
	for (const auto& o : mObjects) nodes.push(o.get());
	while (!nodes.empty()){
		Object* n = nodes.front(); nodes.pop();
		if (n->mEnabled && ray.Intersect(n->BoundsHierarchy()).x > 0) {
			if (Collider* c = dynamic_cast<Collider*>(n)) {
				if ((c->CollisionMask() & mask) != 0) {
					float t = ray.Intersect(c->ColliderBounds()).x;
					if (t > 0 && (t < hitT || closest == nullptr)) {
						closest = c;
						hitT = t;
					}
				}
			}
			for (Object* c : n->mChildren)
				nodes.push(c);
		}
	}

	return closest;
}