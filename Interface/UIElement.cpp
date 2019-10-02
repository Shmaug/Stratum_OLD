#include <Interface/UIElement.hpp>

#include <Interface/UICanvas.hpp>

using namespace std;

UIElement::UIElement(const string& name, UICanvas* canvas)
	: mName(name), mCanvas(canvas), mParent(nullptr), mPosition({}), mSize({}), mCanvasPosition({}), mCanvasSize({}), mTransformDirty(true) {}
UIElement::~UIElement() {}

bool UIElement::Parent(UIElement* p) {
	if (mParent == p) return true;

	if (mParent)
		for (auto it = mParent->mChildren.begin(); it != mParent->mChildren.end();)
			if (*it == this)
				it = mParent->mChildren.erase(it);
			else
				it++;

	mParent = p;
	if (!p || !p->AddChild(this)) {
		Dirty();
		return false;
	}
	Dirty();
	return true;
}
bool UIElement::AddChild(UIElement* e) {
	mChildren.push_back(e);

	mCanvasAABB = AABB(mCanvasPosition + vec3(mCanvasSize * .5f, 0), vec3(mCanvasSize * .5f, UI_THICKNESS));
	for (UIElement*& e : mChildren)
		mCanvasAABB.Encapsulate(e->CanvasBounds());

	return true;
}

bool UIElement::UpdateTransform() {
	if (!mTransformDirty) return false;

	if (mParent) {
		mCanvasPosition = mParent->CanvasPosition() + vec3(mParent->CanvasSize() * mPosition.mScale + mPosition.mOffset, mDepth);
		mCanvasSize = mParent->CanvasSize() * mSize.mScale + mSize.mOffset;
	} else {
		mCanvasPosition = vec3(mCanvas->Size() * mPosition.mScale + mPosition.mOffset, mDepth);
		mCanvasSize = mCanvas->Size() * mSize.mScale + mSize.mOffset;
	}

	mCanvasAABB = AABB(mCanvasPosition + vec3(mCanvasSize * .5f, 0), vec3(mCanvasSize * .5f, UI_THICKNESS));
	for (UIElement*& e : mChildren)
		mCanvasAABB.Encapsulate(e->CanvasBounds());

	mTransformDirty = false;
	return true;
}

void UIElement::Dirty() {
	mTransformDirty = true;
	for (const auto& o : mChildren)
		o->Dirty();
}