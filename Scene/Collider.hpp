#pragma once

#include <Scene/Object.hpp>

class Collider : public virtual Object {
public:
    /// Returns true when an intersection occurs, assigns t to the intersection time if t is not null
    virtual bool Intersect(const  Ray& ray, float* t) = 0;
    virtual uint32_t CollisionMask() = 0;
    virtual OBB ColliderBounds() = 0;
};