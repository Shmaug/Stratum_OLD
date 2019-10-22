#pragma once

#include <set>

#include <Scene/Object.hpp>

class InputPointer;

class Interactable {
public:
    virtual OBB InteractionBounds() = 0;

private:
    friend class InputPointer;
    std::set<InputPointer*> mHover;
    std::set<InputPointer*> mLastHover;
};