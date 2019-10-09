#include <Interface/UICanvas.hpp>
#include <Interface/UIElement.hpp>

#include <queue>

using namespace std;

UICanvas::UICanvas(const string& name, const float2& extent) : Renderer(name), mVisible(true), mExtent(extent) {};
UICanvas::~UICanvas() {}

void UICanvas::AddElement(std::shared_ptr<UIElement> element) {
	mElements.push_back(element);
	element->mCanvas = this;
	element->Dirty();
}
void UICanvas::RemoveElement(UIElement* element) {
	if (element->mCanvas != this) return;
	element->mCanvas = nullptr;
	for (auto it = mElements.begin(); it != mElements.end();)
		if (it->get() == element)
			it = mElements.erase(it);
		else
			it++;
}

bool UICanvas::UpdateTransform() {
	if (!Object::UpdateTransform()) return false;
	mAABB = AABB(float3(0), float3(mExtent, UI_THICKNESS * .5f));
	for (auto e : mElements)
		mAABB.Encapsulate(e->AbsoluteBounds());
	mAABB *= ObjectToWorld();
	return true;
}
void UICanvas::Dirty() {
	Object::Dirty();
	for (auto e : mElements)
		e->Dirty();
}

uint32_t UICanvas::RenderQueue() {
	uint32_t q = 0;
	for (auto e : mElements)
		q = max(q, e->RenderQueue());
	return q;
}

void UICanvas::Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, Material* materialOverride) {
	for (auto e : mElements)
		e->Draw(frameTime, camera, commandBuffer, backBufferIndex, materialOverride);
}