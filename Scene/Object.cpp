#include <Scene/Object.hpp>

#include <glm/gtx/quaternion.hpp>
#include <Scene/Camera.hpp>

using namespace std;

Object::Object(const string& name)
	: mName(name), mParent(nullptr),
	mLocalPosition(vec3()), mLocalRotation(quat(1.f, 0.f, 0.f, 0.f)), mLocalScale(vec3(1.f, 1.f, 1.f)),
	mWorldPosition(vec3()), mWorldRotation(quat(1.f, 0.f, 0.f, 0.f)),
	mObjectToWorld(mat4(1.f)), mWorldToObject(mat4(1.f)), mTransformDirty(true) {
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

	mObjectToWorld = scale(mat4(1.f), mLocalScale) * toMat4(mLocalRotation) * translate(mat4(1.f), mLocalPosition);

	if (mParent) {
		mObjectToWorld = mObjectToWorld * mParent->ObjectToWorld();

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
	if (!p || !p->AddChild(this)) {
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