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

void Object::Dirty() {
	mTransformDirty = true;
	for (const auto& o : mChildren)
		o->Dirty();
}

AABB Object::Bounds() {
	return AABB(WorldPosition(), float3());
}

AABB Object::BoundsHeirarchy() {
	AABB aabb = Bounds();
	for (Object* c : mChildren)
		aabb.Encapsulate(c->BoundsHeirarchy());
	return aabb;
}
bool Object::EnabledHeirarchy() {
	Object* o = this;
	while (o) {
		if (!o->mEnabled) return false;
		o = o->mParent;
	}
	return true;
}