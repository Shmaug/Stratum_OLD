#include <Interface/UICanvas.hpp>
#include <Interface/UIElement.hpp>

#include <queue>

using namespace std;

UICanvas::UICanvas(const string& name, const vec2& extent) : Renderer(name), mExtent(extent) {};
UICanvas::~UICanvas() {}

bool UICanvas::UpdateTransform() {
	if (!Object::UpdateTransform()) return false;
	mAABB = AABB(vec3(0), vec3(mExtent, UI_THICKNESS * .5f));
	for (UIElement*& e : mRootElements)
		mAABB.Encapsulate(e->AbsoluteBounds());
	mAABB *= ObjectToWorld();
	return true;
}
void UICanvas::Dirty() {
	Object::Dirty();
	for (UIElement*& e : mRootElements)
		e->Dirty();
}

void UICanvas::Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, Material* materialOverride) {
	queue<UIElement*> elems;
	for (UIElement* e : mRootElements) elems.push(e);
	while (elems.size()) {
		UIElement* e = elems.front();
		elems.pop();
		e->Draw(frameTime, camera, commandBuffer, backBufferIndex, materialOverride);
		for (UIElement* c : e->mChildren)
			elems.push(c);
	}
}