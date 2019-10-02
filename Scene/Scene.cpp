#include <Scene/Scene.hpp>
#include <Core/DeviceManager.hpp>
#include <Util/Profiler.hpp>

using namespace std;

Scene::Scene(::DeviceManager* deviceManager) : mDeviceManager(deviceManager) {}

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
	PROFILER_BEGIN("Draw");
	for (const auto& r : mRenderers)
		if (r->Visible())
			r->Draw(frameTime, camera, commandBuffer, backBufferIndex, nullptr);
	PROFILER_END;
}

void Scene::Clear(){
	mObjects.clear();
	mCameras.clear();
	mRenderers.clear();
}