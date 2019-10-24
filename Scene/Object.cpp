#include <Scene/Object.hpp>
#include <Scene/Camera.hpp>

using namespace std;

Object::Object(const string& name)
	: mName(name), mParent(nullptr),
	mLocalPosition(float3()), mLocalRotation(quaternion(0, 0, 0, 1)), mLocalScale(float3(1)),
	mWorldPosition(float3()), mWorldRotation(quaternion(0, 0, 0, 1)),
	mObjectToWorld(float4x4(1)), mWorldToObject(float4x4(1)), mTransformDirty(true), mEnabled(true) {
	Dirty();
}
Object::~Object() {
	while (mChildren.size())
		RemoveChild(mChildren[0]);
	if (mParent) mParent->RemoveChild(this);
}

bool Object::UpdateTransform() {
	if (!mTransformDirty) return false;

	mObjectToWorld = float4x4::Translate(mLocalPosition) * float4x4(mLocalRotation) * float4x4::Scale(mLocalScale);

	if (mParent) {
		mObjectToWorld = mParent->ObjectToWorld() * mObjectToWorld;

		mWorldPosition = (mParent->mObjectToWorld * float4(mLocalPosition, 1.f)).xyz;
		mWorldRotation = mParent->mWorldRotation * mLocalRotation;
	} else {
		mWorldPosition = mLocalPosition;
		mWorldRotation = mLocalRotation;
	}

	mWorldToObject = inverse(mObjectToWorld);
	mWorldScale.x = length(mObjectToWorld.c1.xyz);
	mWorldScale.y = length(mObjectToWorld.c2.xyz);
	mWorldScale.z = length(mObjectToWorld.c3.xyz);
	mBounds = AABB(mWorldPosition, float3());

	mTransformDirty = false;
	return true;
}

void Object::AddChild(Object* c) {
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
void Object::RemoveChild(Object* c) {
	if (c->mParent != this) return;

	for (auto it = mChildren.begin(); it != mChildren.end();)
		if (*it == c)
			it = mChildren.erase(it);
		else
			it++;

	c->mParent = nullptr;
	c->Dirty();
}

void Object::DirtyNoHierarchy() {
	mTransformDirty = true;
	for (const auto& o : mChildren)
		o->DirtyNoHierarchy();
}
void Object::Dirty() {
	mTransformDirty = true;
	for (const auto& o : mChildren)
		o->DirtyNoHierarchy();
	
	mHierarchyBoundsDirty = true;
	Object* p = mParent;
	while (p){
		p->mHierarchyBoundsDirty = true;
		p = p->mParent;
	}
}

AABB Object::Bounds() {
	UpdateTransform();
	return mBounds;
}

AABB Object::BoundsHierarchy() {
	if (mHierarchyBoundsDirty){
		mHierarchyBounds = Bounds();
		for (Object* c : mChildren){
			AABB tmp = c->BoundsHierarchy();
			if (tmp.mExtents.x != 0 || tmp.mExtents.y != 0 || tmp.mExtents.z != 0)
				mHierarchyBounds.Encapsulate(tmp);
		}
		mHierarchyBoundsDirty = false;
	}
	return mHierarchyBounds;
}
bool Object::EnabledHierarchy() {
	Object* o = this;
	while (o) {
		if (!o->mEnabled) return false;
		o = o->mParent;
	}
	return true;
}