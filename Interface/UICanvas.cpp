#include <Interface/UICanvas.hpp>
#include <Interface/UIElement.hpp>

using namespace std;

UICanvas::UICanvas(const string& name, const vec3& size) : Renderer(name), mSize(size) {};
UICanvas::~UICanvas() {}

bool UICanvas::UpdateTransform() {
	if (!Object::UpdateTransform()) return false;
	mAABB = AABB(vec3(mSize * .5f, 0), vec3(mSize * .5f, UI_THICKNESS));
	for (UIElement*& e : mRootElements)
		mAABB.Encapsulate(e->CanvasBounds());
	mAABB *= ObjectToWorld();
	return true;
}
void UICanvas::Dirty() {
	Object::Dirty();
	for (UIElement*& e : mRootElements)
		e->Dirty();
}

void UICanvas::Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, Material* materialOverride) {

}