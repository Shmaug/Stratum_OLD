#include <Scene/Scene.hpp>
#include <Core/DeviceManager.hpp>
#include <Util/Profiler.hpp>

using namespace std;

Scene::Scene(::DeviceManager* deviceManager, ::AssetManager* assetManager, ::InputManager* inputManager, ::PluginManager* pluginManager)
	: mDeviceManager(deviceManager), mAssetManager(assetManager), mInputManager(inputManager), mPluginManager(pluginManager) {}
Scene::~Scene(){
	for (auto& kp : mLightBuffers) {
		for (uint32_t i = 0; i < kp.first->MaxFramesInFlight(); i++)
			safe_delete(kp.second[i]);
		safe_delete_array(kp.second);
	}
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
				object->mChildren[0]->Parent(object->mParent);
			object->Parent(nullptr);
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
		lb = new Buffer("Light Buffer", commandBuffer->Device(), max(1u, mLights.size()) * sizeof(GPULight), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		lb->Map();
	}
	if (mLights.size()){
		GPULight* lights = (GPULight*)lb->MappedData();
		for (uint32_t i = 0; i < mLights.size(); i++) {
			float cosInner = cosf(mLights[i]->InnerSpotAngle());
			float cosOuter = cosf(mLights[i]->OuterSpotAngle());

			lights[i].WorldPosition = mLights[i]->WorldPosition();
			lights[i].InvSqrRange = 1.f / (mLights[i]->Range() * mLights[i]->Range());
			lights[i].Color = mLights[i]->Color() * mLights[i]->Intensity();
			lights[i].SpotAngleScale = 1.f / fmaxf(.001f, cosInner - cosOuter);
			lights[i].SpotAngleOffset = -cosOuter * lights[i].SpotAngleScale;
			lights[i].Direction = mLights[i]->WorldRotation() * float3(0, 0, -1);
			lights[i].Type = mLights[i]->Type();
		}
	}
	PROFILER_END;

	PROFILER_BEGIN("Sort");
	sort(mRenderers.begin(), mRenderers.end(), [](Renderer* a, Renderer* b) {
		return a->RenderQueue() < b->RenderQueue();
	});
	PROFILER_END;

	camera->PreRender();
	
	PROFILER_BEGIN("Pre Render");
	for (const auto& p : mPluginManager->Plugins())
		if (p->mEnabled)
			p->PreRender(frameTime, camera, commandBuffer, backBufferIndex);
	PROFILER_END;

	PROFILER_BEGIN("Draw");
	BEGIN_CMD_REGION(commandBuffer, "Draw Scene");
	camera->BeginRenderPass(commandBuffer, backBufferIndex);
	for (const auto& r : mRenderers)
		if (r->Visible()) {
			BEGIN_CMD_REGION(commandBuffer, "Draw " + r->mName);
			r->Draw(frameTime, camera, commandBuffer, backBufferIndex, nullptr);
			END_CMD_REGION(commandBuffer);
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