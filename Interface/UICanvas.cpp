#include <Interface/UICanvas.hpp>
#include <Interface/UIElement.hpp>

#include <queue>

using namespace std;

UICanvas::UICanvas(const string& name, const float2& extent) : Renderer(name), mVisible(true), mExtent(extent), mSortedElementsDirty(true), mRenderQueue(5000) {};
UICanvas::~UICanvas() {}

void UICanvas::AddElement(std::shared_ptr<UIElement> element) {
	mElements.push_back(element);
	mSortedElementsDirty = true;
	element->mCanvas = this;
	element->Dirty();
}
void UICanvas::RemoveElement(UIElement* element) {
	if (element->mCanvas != this) return;
	mSortedElementsDirty = true;
	element->mCanvas = nullptr;

	for (auto it = mSortedElements.begin(); it != mSortedElements.end();)
		if (*it == element)
		it = mSortedElements.erase(it);
	else
		it++;

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

void UICanvas::Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, Material* materialOverride) {
	if (mSortedElementsDirty) {
		mSortedElements.clear();
		for (const shared_ptr<UIElement>& e : mElements)
			mSortedElements.push_back(e.get());
		std::sort(mSortedElements.begin(), mSortedElements.end(), [&](UIElement* a, UIElement* b) {
			return a->AbsolutePosition().z < b->AbsolutePosition().z;
		});
		mSortedElementsDirty = false;
	}

	for (UIElement* e : mSortedElements)
		e->Draw(frameTime, camera, commandBuffer, backBufferIndex, materialOverride);
}