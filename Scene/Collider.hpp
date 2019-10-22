#pragma once

#include <Scene/Object.hpp>

class Collider : public virtual Object {
public:
    virtual uint32_t CollisionMask() = 0;
    virtual OBB ColliderBounds() = 0;
};