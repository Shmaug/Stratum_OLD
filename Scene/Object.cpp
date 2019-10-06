#include <Scene/Object.hpp>

#include <glm/gtx/quaternion.hpp>
#include <Scene/Camera.hpp>

using namespace std;

Object::Object(const string& name)
	: mName(name), mParent(nullptr),
	mLocalPosition(vec3()), mLocalRotation(quat(1.f, 0.f, 0.f, 0.f)), mLocalScale(vec3(1.f, 1.f, 1.f)),
	mWorldPosition(vec3()), mWorldRotation(quat(1.f, 0.f, 0.f, 0.f)),
	mObjectToWorld(mat4(1.f)), mWorldToObject(mat4(1.f)), mTransformDirty(true), mEnabled(true) {
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

	mObjectToWorld = translate(mat4(1.f), mLocalPosition) * toMat4(mLocalRotation) * scale(mat4(1.f), mLocalScale);

	if (mParent) {
		mObjectToWorld = mParent->ObjectToWorld() * mObjectToWorld;

		mWorldPosition = mParent->mObjectToWorld * vec4(mLocalPosition, 1.f);
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
	return AABB(WorldPosition(), vec3());
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