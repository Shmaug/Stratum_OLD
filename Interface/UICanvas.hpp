#pragma once

#define UI_THICKNESS .001f

#include <Scene/Renderer.hpp>

class UIElement;

class UICanvas : public Renderer {
public:
	ENGINE_EXPORT UICanvas(const std::string& name, const vec2& extent);
	ENGINE_EXPORT ~UICanvas();

	inline vec2 Extent(const vec2& size) { mExtent = size; Dirty(); }
	inline vec2 Extent() const { return mExtent; }

	inline virtual AABB Bounds() { UpdateTransform(); return mAABB; }

	inline void Visible(bool v) { mVisible = v; };
	inline virtual bool Visible() override { return mVisible; };
	ENGINE_EXPORT virtual void Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, Material* materialOverride) override;

private:
	bool mVisible;
	AABB mAABB;
	vec2 mExtent;
	std::vector<UIElement*> mRootElements;

protected:
	ENGINE_EXPORT virtual bool UpdateTransform() override;
	ENGINE_EXPORT virtual void Dirty() override;
};