#pragma once

#include <Scene/Renderer.hpp>
#include <Scene/Collider.hpp>

class UIElement;

// Scene object that holds UIElements
// Acts as a graph of UIElements
class UICanvas : public Renderer, public Collider {
public:
	ENGINE_EXPORT UICanvas(const std::string& name, const float2& extent);
	ENGINE_EXPORT ~UICanvas();

  	template<typename T, typename... _Args>
	inline std::shared_ptr<T> AddElement(_Args&&... __args) {
		static_assert(std::is_base_of<UIElement, T>::value, "T must be a UIElement!");
		std::shared_ptr<T> element = std::make_shared<T>(std::forward<_Args>(__args)...);
		mElements.push_back(element);
		element->mCanvas = this;
		element->Dirty();
		mSortedElementsDirty = true;
		return element;
	}
	ENGINE_EXPORT void RemoveElement(UIElement* element);
	
	inline void Extent(const float2& size) { mExtent = size; Dirty(); }
	inline float2 Extent() const { return mExtent; }

	inline virtual void CollisionMask(uint32_t m) { mCollisionMask = m; }
	inline virtual uint32_t CollisionMask() override { return mCollisionMask; }

	inline virtual AABB Bounds() { UpdateTransform(); return mAABB; }
	inline virtual OBB ColliderBounds() { UpdateTransform(); return mOBB; }

	ENGINE_EXPORT virtual UIElement* Raycast(const Ray& worldRay);

	inline void Visible(bool v) { mVisible = v; };
	inline virtual bool Visible() override { return mVisible && EnabledHierarchy(); };
	inline virtual void RenderQueue(uint32_t rq) { mRenderQueue = rq; }
	inline virtual uint32_t RenderQueue() override { return mRenderQueue; }
	ENGINE_EXPORT virtual void Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, ::Material* materialOverride) override;
	ENGINE_EXPORT virtual void DrawGizmos(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, ::Material* materialOverride) override;

private:
	float3 mLastRaycastPos;
	friend class UIElement;
	uint32_t mRenderQueue;
	bool mVisible;
	uint32_t mCollisionMask;
	OBB mOBB;
	AABB mAABB;
	float2 mExtent;
	std::vector<std::shared_ptr<UIElement>> mElements;

	bool mSortedElementsDirty;
	std::vector<UIElement*> mSortedElements;

protected:
	ENGINE_EXPORT virtual bool UpdateTransform() override;
	ENGINE_EXPORT virtual void Dirty() override;
};