#include <Scene/Scene.hpp>
#include <Scene/MeshRenderer.hpp>
#include <Core/DeviceManager.hpp>
#include <Util/Profiler.hpp>

using namespace std;

#define MAX_INSTANCE_BATCH 64

Scene::Scene(::DeviceManager* deviceManager, ::AssetManager* assetManager, ::InputManager* inputManager, ::PluginManager* pluginManager)
	: mDeviceManager(deviceManager), mAssetManager(assetManager), mInputManager(inputManager), mPluginManager(pluginManager), mDrawGizmos(false) {
	mGizmos = new ::Gizmos(this);
}
Scene::~Scene(){
	for (auto& kp : mLightBuffers) {
		for (uint32_t i = 0; i < kp.first->MaxFramesInFlight(); i++)
			safe_delete(kp.second[i]);
		safe_delete_array(kp.second);
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

void Scene::Render(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, Material* materialOverride) {
	PROFILER_BEGIN("Gather Lights");
	if (mLightBuffers.count(commandBuffer->Device()) == 0) {
		Buffer** b = new Buffer*[commandBuffer->Device()->MaxFramesInFlight()];
		memset(b, 0, sizeof(Buffer*) * commandBuffer->Device()->MaxFramesInFlight());
		mLightBuffers.emplace(commandBuffer->Device(), b);
	}

	Buffer*& lb = mLightBuffers.at(commandBuffer->Device())[backBufferIndex];
	if (lb && lb->Size() < mLights.size() * sizeof(GPULight))
		safe_delete(lb);
	if (!lb) {
		lb = new Buffer("Light Buffer", commandBuffer->Device(), vmax(1u, mLights.size()) * sizeof(GPULight), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		lb->Map();
	}
	mActiveLights.clear();
	if (mLights.size()){
		uint32_t li = 0;
		GPULight* lights = (GPULight*)lb->MappedData();
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

			li++;
		}
	}
	PROFILER_END;

	PROFILER_BEGIN("Gather/Sort");
	mRenderList.clear();
	for (Renderer* r : mRenderers)
		if (r->Visible() && camera->IntersectFrustum(r->Bounds()))
			mRenderList.push_back(r);
	sort(mRenderList.begin(), mRenderList.end(), [](Renderer* a, Renderer* b) {
		return a->RenderQueue() < b->RenderQueue();
	});
	PROFILER_END;

	camera->PreRender();
	
	PROFILER_BEGIN("Pre Render");
	for (const auto& p : mPluginManager->Plugins())
		if (p->mEnabled)
			p->PreRender(frameTime, camera, commandBuffer, backBufferIndex);
	for (Renderer* r : mRenderList)
		r->PreRender(frameTime, camera, commandBuffer, backBufferIndex, materialOverride);
	PROFILER_END;

	// combine MeshRenderers that have the same material and mesh
	PROFILER_BEGIN("Draw");
	BEGIN_CMD_REGION(commandBuffer, "Draw Scene");
	camera->BeginRenderPass(commandBuffer, backBufferIndex);
	
	DescriptorSet* batchDS = nullptr;
	Buffer* batchBuffer = nullptr;

	MeshRenderer* batchStart = nullptr;
	uint32_t batchSize = 0;

	for (Renderer* r : mRenderers) {
		MeshRenderer* cur = dynamic_cast<MeshRenderer*>(r);
		if (cur && cur->Batchable()) {
			if (batchStart && batchSize + 1 < MAX_INSTANCE_BATCH &&
				(materialOverride || batchStart->Material() == cur->Material()) && batchStart->Mesh() == cur->Mesh()) {
				// append to batch
				((ObjectBuffer*)batchBuffer-> MappedData())[batchSize].ObjectToWorld = cur->ObjectToWorld();
				((ObjectBuffer*)batchBuffer-> MappedData())[batchSize].WorldToObject = cur->WorldToObject();
				batchSize++;
			} else {
				// not batchable
				if (batchStart) {
					BEGIN_CMD_REGION(commandBuffer, "Draw " + batchStart->mName);
					batchStart->DrawInstanced(frameTime, camera, commandBuffer, backBufferIndex, batchSize, *batchDS, materialOverride);
					END_CMD_REGION(commandBuffer);
				}
				
				batchSize = 0;
				batchStart = cur;

				batchBuffer = new Buffer("Batch", commandBuffer->Device(), sizeof(ObjectBuffer) * MAX_INSTANCE_BATCH, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
				batchBuffer->Map();
				batchDS = new DescriptorSet("BatchDS", commandBuffer->Device()->DescriptorPool(), cur->Material()->GetShader(commandBuffer->Device())->mDescriptorSetLayouts[PER_OBJECT]);
				batchDS->CreateStorageBufferDescriptor(batchBuffer, OBJECT_BUFFER_BINDING);
				batchDS->CreateStorageBufferDescriptor(lb, LIGHT_BUFFER_BINDING);

			}
			// append to batch
			((ObjectBuffer*)batchBuffer->MappedData())[batchSize].ObjectToWorld = cur->ObjectToWorld();
			((ObjectBuffer*)batchBuffer->MappedData())[batchSize].WorldToObject = cur->WorldToObject();
			batchSize++;
		} else {
			// render last batch
			if (batchStart) {
				BEGIN_CMD_REGION(commandBuffer, "Draw " + batchStart->mName);
				batchStart->DrawInstanced(frameTime, camera, commandBuffer, backBufferIndex, batchSize, *batchDS, materialOverride);
				END_CMD_REGION(commandBuffer);
				batchStart = nullptr;
			}
			
			BEGIN_CMD_REGION(commandBuffer, "Draw " + r->mName);
			r->Draw(frameTime, camera, commandBuffer, backBufferIndex, materialOverride);
			END_CMD_REGION(commandBuffer);
		}
	}
	// render last batch
	if (batchStart) {
		BEGIN_CMD_REGION(commandBuffer, "Draw " + batchStart->mName);
		batchStart->DrawInstanced(frameTime, camera, commandBuffer, backBufferIndex, batchSize, *batchDS, materialOverride);
		END_CMD_REGION(commandBuffer);
	}
	PROFILER_END;

	if (mDrawGizmos) {
		PROFILER_BEGIN("Draw Gizmos");
		BEGIN_CMD_REGION(commandBuffer, "Draw Gizmos");
		for (const auto& r : mObjects)
			if (r->EnabledHierarchy()) {
				BEGIN_CMD_REGION(commandBuffer, "Gizmos " + r->mName);
				r->DrawGizmos(frameTime, camera, commandBuffer, backBufferIndex);
				END_CMD_REGION(commandBuffer);
			}

		for (const auto& p : mPluginManager->Plugins())
			if (p->mEnabled)
				p->DrawGizmos(frameTime, camera, commandBuffer, backBufferIndex);

		END_CMD_REGION(commandBuffer);
		PROFILER_END;
	}

	camera->EndRenderPass(commandBuffer, backBufferIndex);
	END_CMD_REGION(commandBuffer);
	PROFILER_END;

	// Post Render
	PROFILER_BEGIN("Post Render");
	for (const auto& p : mPluginManager->Plugins())
		if (p->mEnabled)
			p->PostRender(frameTime, camera, commandBuffer, backBufferIndex);
	PROFILER_END;

	camera->PostRender(commandBuffer, backBufferIndex);
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