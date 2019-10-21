#pragma once

#define UI_THICKNESS .001f

#include <Scene/Renderer.hpp>

class UIElement;

// Scene object that holds UIElements
// Acts as a graph of UIElements
class UICanvas : public Renderer {
public:
	ENGINE_EXPORT UICanvas(const std::string& name, const float2& extent);
	ENGINE_EXPORT ~UICanvas();

	ENGINE_EXPORT void AddElement(std::shared_ptr<UIElement> element);
	ENGINE_EXPORT void RemoveElement(UIElement* element);

	inline void Extent(const float2& size) { mExtent = size; Dirty(); }
	inline float2 Extent() const { return mExtent; }

	inline virtual AABB Bounds() { UpdateTransform(); return mAABB; }

	inline void Visible(bool v) { mVisible = v; };
	inline virtual bool Visible() override { return mVisible && EnabledHeirarchy(); };
	inline virtual void RenderQueue(uint32_t rq) { mRenderQueue = rq; }
	inline virtual uint32_t RenderQueue() override { return mRenderQueue; }
	ENGINE_EXPORT virtual void Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, Material* materialOverride) override;

private:
	friend class UIElement;
	uint32_t mRenderQueue;
	bool mVisible;
	AABB mAABB;
	float2 mExtent;
	std::vector<std::shared_ptr<UIElement>> mElements;

	bool mSortedElementsDirty;
	std::vector<UIElement*> mSortedElements;

protected:
	ENGINE_EXPORT virtual bool UpdateTransform() override;
	ENGINE_EXPORT virtual void Dirty() override;
};