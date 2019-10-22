#pragma once

#include <set>
#include <Scene/Object.hpp>

class UICanvas;
class InputPointer;

struct UDim2 {
	// Relative to parent
	float2 mScale;
	// World-space offset
	float2 mOffset;
	UDim2() : mScale(float2()), mOffset(float2()) {}
	UDim2(const float2& scale, const float2& offset) : mScale(scale), mOffset(offset) {}
};

class UIElement {
public:
	bool mVisible;
	bool mRecieveRaycast;
	const std::string mName;

	ENGINE_EXPORT UIElement(const std::string& name, UICanvas* canvas);
	ENGINE_EXPORT ~UIElement();

	inline UIElement* Parent() const { return mParent; }
	ENGINE_EXPORT void AddChild(UIElement* elem);
	ENGINE_EXPORT void RemoveChild(UIElement* elem);

	inline UICanvas* Canvas() const { return mCanvas; }

	inline uint32_t ChildCount() const { return (uint32_t)mChildren.size(); }
	inline UIElement* Child(uint32_t index) const { return mChildren[index]; }

	// Top-left corner, relative to parent
	inline UDim2 Position() const { return mPosition; }
	// Relative to parent
	inline UDim2 Extent() const { return mExtent; }
	// Relative to the canvas. Only controls draw order within the canvas. Higher depth = later draw
	inline float Depth() const { return mDepth; }

	// Top-left corner, relative to canvas
	inline void Position(const UDim2& p) { mPosition = p; Dirty(); }
	inline void Position(float sx, float sy, float ox, float oy) { mPosition.mScale.x = sx; mPosition.mScale.y = sy; mPosition.mOffset.x = ox; mPosition.mOffset.y = oy; Dirty(); }
	inline void Extent(const UDim2& s) { mExtent = s; Dirty(); }
	inline void Extent(float sx, float sy, float ox, float oy) { mExtent.mScale.x = sx; mExtent.mScale.y = sy; mExtent.mOffset.x = ox; mExtent.mOffset.y = oy; Dirty(); }
	// Relative to the canvas. Only controls draw order within the canvas. Higher depth = later draw
	ENGINE_EXPORT void Depth(float d);

	// Top-left corner, relative to canvas
	inline float2 AbsolutePosition() { UpdateTransform(); return mAbsolutePosition; }
	// Relative to canvas
	inline float2 AbsoluteExtent() { UpdateTransform(); return mAbsoluteExtent; }
	
	virtual bool Visible() { return mVisible; }
	virtual void Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, Material* materialOverride) {}
	
private:
	friend class UICanvas;
	UICanvas* mCanvas;
	bool mTransformDirty;
	
	// Top-left corner, relative to parent
	UDim2 mPosition;
	// Relative to parent
	UDim2 mExtent;
	// Relative to parent
	float mDepth;

	// Top-left corner, relative to parent
	float2 mAbsolutePosition;
	// Relative to canvas
	float2 mAbsoluteExtent;

	UIElement* mParent;
	std::vector<UIElement*> mChildren;

protected:
	ENGINE_EXPORT virtual void Dirty();
	ENGINE_EXPORT virtual bool UpdateTransform();
};