#include <Interface/UICanvas.hpp>
#include <Interface/UIElement.hpp>
#include <Scene/Scene.hpp>

#include <queue>

using namespace std;

UICanvas::UICanvas(const string& name, const float2& extent)
	: Object(name), mVisible(true), mExtent(extent), mSortedElementsDirty(true), mRenderQueue(5000) {};
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
	mAABB = AABB(-float3(mExtent, 0), float3(mExtent, 0)) * ObjectToWorld();
	return true;
}
void UICanvas::Dirty() {
	Object::Dirty();
	for (auto e : mElements)
		e->Dirty();
}

bool UICanvas::Intersect(const Ray& worldRay, float* t, bool any) {
	Ray transformedRay = Ray(
		(WorldToObject() * float4(worldRay.mOrigin, 1)).xyz,
		(transpose(ObjectToWorld()) * float4(worldRay.mDirection, 1)).xyz
	);
	float ht = transformedRay.Intersect(AABB(-float3(mExtent, 0), float3(mExtent, 0))).x;
	if (ht < 0) return false;
	*t = ht;
	return true;
}
UIElement* UICanvas::Raycast(const Ray& worldRay) {
	Ray transformedRay = Ray(
		(WorldToObject() * float4(worldRay.mOrigin, 1)).xyz,
		(transpose(ObjectToWorld()) * float4(worldRay.mDirection, 1)).xyz
	);
	float t = transformedRay.Intersect(AABB(-float3(mExtent, 0), float3(mExtent, 0))).x;
	if (t < 0) return nullptr;

	float3 cp = transformedRay.mOrigin + transformedRay.mDirection * t;

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
	if (pass != PASS_MAIN) return;

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
			e->Draw(commandBuffer, camera, pass);
}