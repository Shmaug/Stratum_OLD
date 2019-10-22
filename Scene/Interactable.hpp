#pragma once

#include <set>

#include <Scene/Collider.hpp>

class InputPointer;

class Interactable : public Collider {
public:

private:
    friend class InputPointer;
    std::set<InputPointer*> mHover;
    std::set<InputPointer*> mLastHover;
};