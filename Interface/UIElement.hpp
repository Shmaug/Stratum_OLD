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

	// Top-left corner, relative to parent
	inline UDim2 Position() const { return mPosition; }
	// Relative to parent
	inline UDim2 Extent() const { return mExtent; }
	// Relative to parent
	inline float Depth() const { return mDepth; }

	// Top-left corner, relative to canvas
	inline void Position(const UDim2& p) { mPosition = p; Dirty(); }
	inline void Extent(const UDim2& s) { mExtent = s; Dirty(); }
	inline void Depth(float d) { mDepth = d; Dirty(); }

	// Top-left corner, relative to canvas
	inline vec3 AbsolutePosition() { UpdateTransform(); return mAbsolutePosition; }
	// Relative to canvas
	inline vec2 AbsoluteExtent() { UpdateTransform(); return mAbsoluteExtent; }
	// Relative to canvas
	inline virtual AABB AbsoluteBounds() { UpdateTransform(); return mAbsoluteAABB; }

	inline virtual uint32_t RenderQueue() { return 1000; }
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
	vec3 mAbsolutePosition;
	// Relative to canvas
	vec2 mAbsoluteExtent;
	// Relative to canvas
	AABB mAbsoluteAABB;

	UIElement* mParent;
	std::vector<UIElement*> mChildren;

protected:
	ENGINE_EXPORT virtual void Dirty();
	ENGINE_EXPORT virtual bool UpdateTransform();
};