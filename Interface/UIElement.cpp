#include <Interface/UIElement.hpp>

#include <Interface/UICanvas.hpp>

using namespace std;

UIElement::UIElement(const string& name, UICanvas* canvas)
	: mName(name), mCanvas(canvas), mRecieveRaycast(false), mParent(nullptr), mPosition(UDim2()), mExtent(UDim2()), mDepth(0.f), mAbsolutePosition(float2()), mAbsoluteExtent(float2()), mTransformDirty(true), mVisible(true) {}
UIElement::~UIElement() {
	while (mChildren.size())
		RemoveChild(mChildren[0]);
	if (mParent) mParent->RemoveChild(this);
}

bool UIElement::Visible(){
	UIElement* p = this;
	while (p) {
		if (!p->mVisible) return false;
		p = p->mParent;
	}
	return true;
}

void UIElement::AddChild(UIElement* c) {
	if (c->mParent == this) return;

	if (c->mParent)
		for (auto it = c->mParent->mChildren.begin(); it != c->mParent->mChildren.end();)
			if (*it == c)
				it = c->mParent->mChildren.erase(it);
			else
				it++;

	mChildren.push_back(c);
	c->mParent = this;
	c->Dirty();
}
void UIElement::RemoveChild(UIElement* c) {
	if (c->mParent != this) return;

	for (auto it = mChildren.begin(); it != mChildren.end();)
		if (*it == c)
			it = mChildren.erase(it);
		else
			it++;

	c->mParent = nullptr;
	c->Dirty();
}

void UIElement::Depth(float d){
	mDepth = d;
	mCanvas->mSortedElementsDirty = true;
	Dirty();
}

bool UIElement::UpdateTransform() {
	if (!mTransformDirty) return false;

	if (mParent) {
		mAbsolutePosition = mParent->AbsolutePosition() + mParent->AbsoluteExtent() * mPosition.mScale + mPosition.mOffset;
		mAbsoluteExtent   = mParent->AbsoluteExtent() * mExtent.mScale + mExtent.mOffset;
	} else {
		mAbsolutePosition = mCanvas->Extent() * mPosition.mScale + mPosition.mOffset;
		mAbsoluteExtent = mCanvas->Extent() * mExtent.mScale + mExtent.mOffset;
	}
	
	mTransformDirty = false;
	return true;
}

void UIElement::Dirty() {
	mTransformDirty = true;
	for (const auto& o : mChildren)
		o->Dirty();
}