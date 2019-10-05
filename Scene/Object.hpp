#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <Core/CommandBuffer.hpp>
#include <Util/Geometry.hpp>
#include <Util/Util.hpp>

class Camera;
class Scene;

class Object {
public:
	const std::string mName;
	bool mEnabled;

	ENGINE_EXPORT Object(const std::string& name);
	ENGINE_EXPORT ~Object();

	inline Object* Parent() const { return mParent; }
	ENGINE_EXPORT bool Parent(Object* obj);
	ENGINE_EXPORT virtual bool AddChild(Object* obj);

	inline uint32_t ChildCount() const { return (uint32_t)mChildren.size(); }
	inline Object* Child(uint32_t index) const { return mChildren[index]; }

	inline vec3 WorldPosition() { UpdateTransform(); return mWorldPosition; }
	inline quat WorldRotation() { UpdateTransform(); return mWorldRotation; }

	inline vec3 LocalPosition() { UpdateTransform(); return mLocalPosition; }
	inline quat LocalRotation() { UpdateTransform(); return mLocalRotation; }
	inline vec3 LocalScale() { UpdateTransform(); return mLocalScale; }

	inline mat4 ObjectToWorld() { UpdateTransform(); return mObjectToWorld; }
	inline mat4 WorldToObject() { UpdateTransform(); return mWorldToObject; }

	inline void LocalPosition(const vec3& p) { mLocalPosition = p; Dirty(); }
	inline void LocalRotation(const quat& r) { mLocalRotation = r; Dirty(); }
	inline void LocalScale(const vec3& s) { mLocalScale = s; Dirty(); }

	inline void LocalPosition(float x, float y, float z) { mLocalPosition.x = x; mLocalPosition.y = y; mLocalPosition.z = z; Dirty(); }
	inline void LocalScale(float x, float y, float z) { mLocalScale.x = x; mLocalScale.y = y; mLocalScale.z = z; Dirty(); }

	ENGINE_EXPORT bool EnabledHeirarchy();
	ENGINE_EXPORT virtual AABB Bounds();
	ENGINE_EXPORT virtual AABB BoundsHeirarchy();

private:
	friend class Scene;
	Scene* mScene;

	bool mTransformDirty;
	vec3 mLocalPosition;
	quat mLocalRotation;
	vec3 mLocalScale;
	mat4 mObjectToWorld;
	mat4 mWorldToObject;

	vec3 mWorldPosition;
	quat mWorldRotation;

	Object* mParent;
	std::vector<Object*> mChildren;

protected:
	ENGINE_EXPORT virtual void Dirty();
	ENGINE_EXPORT virtual bool UpdateTransform();
};