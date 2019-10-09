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
	for (const auto& o : mChildren)
		o->Parent(mParent);
	mChildren.clear();
	Parent(nullptr);
}

bool Object::UpdateTransform() {
	if (!mTransformDirty) return false;

	mObjectToWorld = translate(mLocalPosition) * float4x4(mLocalRotation) * scale(mLocalScale);

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

bool Object::AddChild(Object* c) {
	mChildren.push_back(c);
	return true;
}

bool Object::Parent(Object* p) {
	if (mParent == p) return true;

	if (mParent)
		for (auto it = mParent->mChildren.begin(); it != mParent->mChildren.end();)
			if (*it == this)
				it = mParent->mChildren.erase(it);
			else
				it++;

	mParent = p;
	if (p && !p->AddChild(this)) {
		mParent = nullptr;
		Dirty();
		return false;
	}
	Dirty();
	return true;
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