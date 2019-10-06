#include <Scene/Scene.hpp>
#include <Core/DeviceManager.hpp>
#include <Util/Profiler.hpp>

using namespace std;

Scene::Scene(::DeviceManager* deviceManager) : mDeviceManager(deviceManager) {}

void Scene::Update(const FrameTime& frameTime) {
	PROFILER_BEGIN("Pre Update");
	for (const auto& p : mPlugins)
		p->PreUpdate(frameTime);
	PROFILER_END;

	PROFILER_BEGIN("Update");
	for (const auto& p : mPlugins)
		p->Update(frameTime);
	PROFILER_END;

	PROFILER_BEGIN("Post Update");
	for (const auto& p : mPlugins)
		p->PostUpdate(frameTime);
	PROFILER_END;
}

void Scene::AddObject(Object* object) {
	mObjects.push_back(object);
	object->mScene = this;
	
	if (auto c = dynamic_cast<Camera*>(object))
		mCameras.push_back(c);

	if (auto r = dynamic_cast<Renderer*>(object))
		mRenderers.push_back(r);
}
void Scene::RemoveObject(Object* object) {
	for (auto it = mObjects.begin(); it != mObjects.end();)
		if (*it == object) {
			object->mScene = nullptr;
			object->Parent(nullptr);
			it = mObjects.erase(it);
			break;
		} else
			it++;
	
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
}

void Scene::Render(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, Material* materialOverride) {
	PROFILER_BEGIN("Sort");
	sort(mRenderers.begin(), mRenderers.end(), [](const auto& a, const auto& b) {
		return a->RenderQueue() < b->RenderQueue();
	});
	PROFILER_END;

	for (const auto& p : mPlugins)
		p->PreRender(frameTime, camera, commandBuffer, backBufferIndex);

	PROFILER_BEGIN("Draw");
	camera->BeginRenderPass(commandBuffer, backBufferIndex);
	for (const auto& r : mRenderers)
		if (r->Visible()) {
			PROFILER_BEGIN("Draw " + r->mName);
			r->Draw(frameTime, camera, commandBuffer, backBufferIndex, nullptr);
			PROFILER_END;
		}
	camera->EndRenderPass(commandBuffer, backBufferIndex);
	PROFILER_END;


	// Post Render
	PROFILER_BEGIN("Post Render");
	for (const auto& p : mPlugins)
		p->PostRender(frameTime, camera, commandBuffer, backBufferIndex);
	PROFILER_END;
}

void Scene::Clear(){
	mObjects.clear();
	mCameras.clear();
	mRenderers.clear();
}