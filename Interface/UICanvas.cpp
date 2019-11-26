#include <Interface/UICanvas.hpp>
#include <Interface/UIElement.hpp>
#include <Scene/Scene.hpp>

#include <queue>

using namespace std;

UICanvas::UICanvas(const string& name, const float2& extent)
	: Object(name), mVisible(true), mExtent(extent), mSortedElementsDirty(true), mRenderQueue(5000), mCollisionMask(0x02) {};
UICanvas::~UICanvas() {}

void UICanvas::RemoveElement(UIElement* element) {
	if (element->mCanvas != this) return;
	mSortedElementsDirty = true;
	element->mCanvas = nullptr;

	for (auto it = mSortedElements.begin(); it != mSortedElements.end();)
		if (*it == element)
		it = mSortedElements.erase(it);
	else
		it++;

	for (auto it = mElements.begin(); it != mElements.end();)
		if (it->get() == element)
			it = mElements.erase(it);
		else
			it++;
}

bool UICanvas::UpdateTransform() {
	if (!Object::UpdateTransform()) return false;
	mOBB = OBB(WorldPosition(), float3(mExtent, .001f) * WorldScale(), WorldRotation());
	mAABB = mOBB;
	return true;
}
void UICanvas::Dirty() {
	Object::Dirty();
	for (auto e : mElements)
		e->Dirty();
}

UIElement* UICanvas::Raycast(const Ray& worldRay) {
	float t = worldRay.Intersect(ColliderBounds()).x;
	if (t < 0) return nullptr;
	float3 wp = worldRay.mOrigin + worldRay.mDirection * t;
	float3 cp = (WorldToObject() * float4(wp, 1)).xyz;

	float minDepth = 0;
	UIElement* hit = nullptr;
	for (const shared_ptr<UIElement>& e : mElements) {
		if (e->mRecieveRaycast && e->mVisible && (!hit || e->Depth() < minDepth)){
			float2 rp = abs(cp.xy - e->AbsolutePosition());
			if (rp.x < e->AbsoluteExtent().x && rp.y < e->AbsoluteExtent().y) {
				hit = e.get();
				minDepth = e->Depth();
			}
		}
	}
	return hit;
}
void UICanvas::Draw(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {
	if (pass != Main) return;

	if (mSortedElementsDirty) {
		mSortedElements.clear();
		for (const shared_ptr<UIElement>& e : mElements)
			mSortedElements.push_back(e.get());
		sort(mSortedElements.begin(), mSortedElements.end(), [&](UIElement* a, UIElement* b) {
			return a->Depth() > b->Depth();
		});
		mSortedElementsDirty = false;
	}

	for (UIElement* e : mSortedElements)
		if (e->Visible())
			e->Draw(commandBuffer, camera);
}