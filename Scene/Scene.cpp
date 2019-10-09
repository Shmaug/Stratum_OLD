#include <Scene/Scene.hpp>
#include <Core/DeviceManager.hpp>
#include <Util/Profiler.hpp>

using namespace std;

Scene::Scene(::DeviceManager* deviceManager, ::AssetManager* assetManager, ::InputManager* inputManager, ::PluginManager* pluginManager)
	: mDeviceManager(deviceManager), mAssetManager(assetManager), mInputManager(inputManager), mPluginManager(pluginManager) {}
Scene::~Scene(){
	mCameras.clear();
	mRenderers.clear();
	mObjects.clear();
}

void Scene::Update(const FrameTime& frameTime) {
	PROFILER_BEGIN("Pre Update");
	for (const auto& p : mPluginManager->Plugins())
		p->PreUpdate(frameTime);
	PROFILER_END;

	PROFILER_BEGIN("Update");
	for (const auto& p : mPluginManager->Plugins())
		p->Update(frameTime);
	PROFILER_END;

	PROFILER_BEGIN("Post Update");
	for (const auto& p : mPluginManager->Plugins())
		p->PostUpdate(frameTime);
	PROFILER_END;
}

void Scene::AddObject(shared_ptr<Object> object) {
	mObjects.push_back(object);
	object->mScene = this;
	
	if (auto c = dynamic_cast<Camera*>(object.get()))
		mCameras.push_back(c);

	if (auto r = dynamic_cast<Renderer*>(object.get()))
		mRenderers.push_back(r);
}
void Scene::RemoveObject(Object* object) {
	if (!object) return;
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
			for (auto& c : object->mChildren)
				c->Parent(object->mParent);
			object->Parent(nullptr);
			object->mScene = nullptr;
			it = mObjects.erase(it);
			break;
		} else
			it++;
}

void Scene::Render(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, Material* materialOverride) {
	camera->PreRender();

	PROFILER_BEGIN("Sort");
	sort(mRenderers.begin(), mRenderers.end(), [](const auto& a, const auto& b) {
		return a->RenderQueue() < b->RenderQueue();
	});
	PROFILER_END;

	PROFILER_BEGIN("Pre Render");
	for (const auto& p : mPluginManager->Plugins())
		p->PreRender(frameTime, camera, commandBuffer, backBufferIndex);
	PROFILER_END;

	PROFILER_BEGIN("Draw");
	BEGIN_CMD_REGION(commandBuffer, "Draw Scene");
	camera->BeginRenderPass(commandBuffer, backBufferIndex);
	for (const auto& r : mRenderers)
		if (r->Visible()) {
			PROFILER_BEGIN("Draw " + r->mName);
			BEGIN_CMD_REGION(commandBuffer, "Draw " + r->mName);
			r->Draw(frameTime, camera, commandBuffer, backBufferIndex, nullptr);
			END_CMD_REGION(commandBuffer);
			PROFILER_END;
		}
	camera->EndRenderPass(commandBuffer, backBufferIndex);
	END_CMD_REGION(commandBuffer);
	PROFILER_END;

	// Post Render
	PROFILER_BEGIN("Post Render");
	for (const auto& p : mPluginManager->Plugins())
		p->PostRender(frameTime, camera, commandBuffer, backBufferIndex);
	PROFILER_END;
}