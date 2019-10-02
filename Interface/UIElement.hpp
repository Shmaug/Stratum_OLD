#pragma once

#include <Scene/Object.hpp>

class UICanvas;

struct UDim2 {
	// Relative to parent
	vec2 mScale;
	// World-space offset
	vec2 mOffset;
};

class UIElement {
public:
	bool mVisible;
	const std::string mName;

	ENGINE_EXPORT UIElement(const std::string& name, UICanvas* mCanvas);
	ENGINE_EXPORT ~UIElement();

	inline UIElement* Parent() const { return mParent; }
	ENGINE_EXPORT bool Parent(UIElement* elem);
	ENGINE_EXPORT virtual bool AddChild(UIElement* elem);

	inline UICanvas* Canvas() const { return mCanvas; }

	inline uint32_t ChildCount() const { return (uint32_t)mChildren.size(); }
	inline UIElement* Child(uint32_t index) const { return mChildren[index]; }

	// Top-left corner
	inline UDim2 Position() const { return mPosition; }
	inline UDim2 Size() const { return mSize; }
	inline float Depth() const { return mDepth; }

	// Top-left corner
	inline void Position(const UDim2& p) { mPosition = p; Dirty(); }
	inline void Size(const UDim2& s) { mSize = s; Dirty(); }
	inline void Depth(float d) { mDepth = d; Dirty(); }

	// Top-left corner, relative to canvas
	inline vec3 CanvasPosition() { UpdateTransform(); return mCanvasPosition; }
	inline vec2 CanvasSize() { UpdateTransform(); return mCanvasSize; }
	inline virtual AABB CanvasBounds() { UpdateTransform(); return mCanvasAABB; }

	inline virtual uint32_t RenderQueue() { return 1000; }
	virtual bool Visible() { return mVisible; }
	virtual void Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, Material* materialOverride) {}

private:
	friend class UICanvas;
	UICanvas* mCanvas;
	bool mTransformDirty;
	
	// Top-left corner
	UDim2 mPosition;
	UDim2 mSize;
	float mDepth;

	AABB mCanvasAABB;
	vec3 mCanvasPosition;
	vec2 mCanvasSize;

	UIElement* mParent;
	std::vector<UIElement*> mChildren;

protected:
	ENGINE_EXPORT virtual void Dirty();
	ENGINE_EXPORT virtual bool UpdateTransform();
};