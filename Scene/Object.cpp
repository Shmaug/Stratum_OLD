#include <Scene/Object.hpp>
#include <Scene/Camera.hpp>

using namespace std;

Object::Object(const string& name)
	: mName(name), mParent(nullptr), mScene(nullptr),
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

	mObjectToParent = float4x4::TRS(mLocalPosition, mLocalRotation, mLocalScale);

	if (mParent) {
		mObjectToWorld = mParent->ObjectToWorld() * mObjectToParent;
		mWorldPosition = (mParent->mObjectToWorld * float4(mLocalPosition, 1.f)).xyz;
		mWorldRotation = mParent->mWorldRotation * mLocalRotation;
	} else {
		mObjectToWorld = mObjectToParent;
		mWorldPosition = mLocalPosition;
		mWorldRotation = mLocalRotation;
	}

	mWorldToObject = inverse(mObjectToWorld);
	mWorldScale.x = length(mObjectToWorld[0].xyz);
	mWorldScale.y = length(mObjectToWorld[1].xyz);
	mWorldScale.z = length(mObjectToWorld[2].xyz);
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

void Object::Dirty() {
	mTransformDirty = true;
	queue<Object*> objs;
	for (Object* c : mChildren) {
		c->mTransformDirty = true;
		for (Object* o : c->mChildren)
			if (o == this) cerr << "Loop in heirarchy! " << c->mName << " -> " << mName << endl;
			else objs.push(o);
	}
	while (!objs.empty()) {
		Object* c = objs.front();
		objs.pop();
		c->mTransformDirty = true;
		for (Object* o : c->mChildren)
			if (o == this) cerr << "Loop in heirarchy! " << c->mName << " -> " << mName << endl;
			else objs.push(o);
	}
}

AABB Object::Bounds() {
	UpdateTransform();
	return mBounds;
}

bool Object::EnabledHierarchy() {
	Object* o = this;
	while (o) {
		if (!o->mEnabled) return false;
		o = o->mParent;
	}
	return true;
}