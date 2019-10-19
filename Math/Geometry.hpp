#pragma once

#include <Math/Math.hpp>

struct Sphere {
	float3 mCenter;
	float mRadius;
};
struct AABB {
	float3 mCenter;
	float3 mExtents;

	AABB() : mCenter(float3()), mExtents(float3()) {}
	AABB(const float3& center, const float3& extents) : mCenter(center), mExtents(extents) {}
	AABB(const AABB& aabb) : mCenter(aabb.mCenter), mExtents(aabb.mExtents) {}
	AABB(const AABB& aabb, const float4x4& transform) {
		float3 mn = aabb.mCenter - aabb.mExtents;
		float3 mx = aabb.mCenter + aabb.mExtents;
		float3 corners[8] {
			mx,							// 1,1,1
			float3(mn.x, mx.y, mx.z),	// 0,1,1
			float3(mx.x, mx.y, mn.z),	// 1,1,0
			float3(mn.x, mx.y, mn.z),	// 0,1,0
			float3(mx.x, mn.y, mx.z),	// 1,0,1
			float3(mn.x, mn.y, mx.z),	// 0,0,1
			float3(mx.x, mn.y, mn.z),	// 1,0,0
			mn,							// 0,0,0
		};

		for (uint32_t i = 0; i < 8; i++)
			corners[i] = (transform * float4(corners[i], 1)).xyz;

		mn = corners[0];
		mx = corners[0];
		for (uint32_t i = 1; i < 8; i++) {
			mn = vmin(mn, corners[i]);
			mx = vmax(mx, corners[i]);
		}

		mCenter = (mn + mx) * .5f;
		mExtents = (mx - mn) * .5f;
	}

	inline bool Intersects(const float3& point) const {
		float3 s = point - mCenter;
		return
			(s.x <= mExtents.x && s.x >= -mExtents.x) &&
			(s.y <= mExtents.y && s.y >= -mExtents.y) &&
			(s.z <= mExtents.z && s.z >= -mExtents.z);
	}
	inline bool Intersects(const Sphere& sphere) const {
		float3 s = sphere.mCenter - mCenter;
		float3 delta = mExtents - s;
		float sqDist = 0.0f;
		for (int i = 0; i < 3; i++) {
			if (s[i] < -mExtents[i]) sqDist += delta[i];
			if (s[i] > mExtents[i]) sqDist += delta[i];
		}
		return sqDist <= sphere.mRadius * sphere.mRadius;
	}
	inline bool Intersects(const AABB& aabb) const {
		float3 MinA = mCenter - mExtents;
		float3 MaxA = mCenter + mExtents;

		float3 MinB = aabb.mCenter - aabb.mExtents;
		float3 MaxB = aabb.mCenter + aabb.mExtents;

		// for each i in (x, y, z) if a_min(i) > b_max(i) or b_min(i) > a_max(i) then return false
		bool dx = (MinA.x > MaxB.x) || (MinB.x > MaxA.x);
		bool dy = (MinA.y > MaxB.y) || (MinB.y > MaxA.y);
		bool dz = (MinA.z > MaxB.z) || (MinB.z > MaxA.z);

		return !(dx || dy || dz);
	}

	inline void Encapsulate(const AABB& aabb) {
		float3 mn = vmin(mCenter - mExtents, aabb.mCenter - aabb.mExtents);
		float3 mx = vmax(mCenter + mExtents, aabb.mCenter + aabb.mExtents);
		mCenter = (mn + mx) * .5f;
		mExtents = (mx - mn) * .5f;
	}

	inline AABB operator *(const float4x4& transform) {
		return AABB(*this, transform);
	}
	inline AABB operator *=(const float4x4& transform) {
		float3 mn = mCenter - mExtents;
		float3 mx = mCenter + mExtents;
		float3 corners[8]{
			mx,							// 1,1,1
			float3(mn.x, mx.y, mx.z),	// 0,1,1
			float3(mx.x, mx.y, mn.z),	// 1,1,0
			float3(mn.x, mx.y, mn.z),	// 0,1,0
			float3(mx.x, mn.y, mx.z),	// 1,0,1
			float3(mn.x, mn.y, mx.z),	// 0,0,1
			float3(mx.x, mn.y, mn.z),	// 1,0,0
			mn,							// 0,0,0
		};

		for (uint32_t i = 0; i < 8; i++)
			corners[i] = (transform * float4(corners[i], 1)).xyz;

		mn = corners[0];
		mx = corners[0];
		for (uint32_t i = 1; i < 8; i++) {
			mn = vmin(mn, corners[i]);
			mx = vmax(mx, corners[i]);
		}

		mCenter = (mn + mx) * .5f;
		mExtents = (mx - mn) * .5f;

		return *this;
	}
};
struct OBB {
	float3 mCenter;
	float3 mExtents;
	quaternion mOrientation;

	inline OBB() : mCenter({}), mExtents({}), mOrientation(quaternion(1.f, 0.f, 0.f, 0.f)) {}
	inline OBB(const float3& center, const float3& extents) : mCenter(center), mExtents(extents), mOrientation(quaternion(1.f, 0.f, 0.f, 0.f)) {}
	inline OBB(const float3& center, const float3& extents, const quaternion& orientation) : mCenter(center), mExtents(extents), mOrientation(orientation) {}

	inline bool Intersects(const float3& point) const {
		float3 s = inverse(mOrientation) * (point - mCenter);
		return
			(s.x <= mExtents.x && s.x >= -mExtents.x) &&
			(s.y <= mExtents.y && s.y >= -mExtents.y) &&
			(s.z <= mExtents.z && s.z >= -mExtents.z);
	}
	inline bool Intersects(const Sphere& sphere) const {
		float3 s = inverse(mOrientation) * (sphere.mCenter - mCenter);
		float3 delta = mExtents - s;
		float sqDist = 0.0f;
		for (int i = 0; i < 3; i++) {
			if (s[i] < -mExtents[i]) sqDist += delta[i];
			if (s[i] > mExtents[i]) sqDist += delta[i];
		}
		return sqDist <= sphere.mRadius * sphere.mRadius;
	}
};

struct Ray {
	float3 mOrigin;
	float3 mDirection;

	inline float2 Intersect(const AABB& aabb) const {
		float3 m = 1.f / mDirection;
		float3 n = m * (mOrigin - aabb.mCenter);
		float3 k = vabs(m) * aabb.mExtents;
		float3 t1 = -n - k;
		float3 t2 = -n + k;
		float tN = fmaxf(fmaxf(t1.x, t1.y), t1.z);
		float tF = fminf(fminf(t2.x, t2.y), t2.z);
		if (tN > tF || tF < 0.f) return float2(-1.f);
		return float2(tN, tF);
	}
	inline float2 Intersect(const OBB& obb) const {
		quaternion io = inverse(obb.mOrientation);
		float3 m = 1.f / (io * mDirection);
		float3 n = m * (io * (mOrigin - obb.mCenter));
		float3 k = vabs(m) * obb.mExtents;
		float3 t1 = -n - k;
		float3 t2 = -n + k;
		float tN = fmaxf(fmaxf(t1.x, t1.y), t1.z);
		float tF = fminf(fminf(t2.x, t2.y), t2.z);
		if (tN > tF || tF < 0.f) return float2(-1.f);
		return float2(tN, tF);
	}
	inline float2 Intersect(const Sphere& sphere) const {
		float3 pq = mOrigin - sphere.mCenter;
		float b = dot(pq, mDirection);
		float d = b * b - 4.f * dot(pq, pq);
		if (d < 0.f) return float2(-1.f);
		d = sqrt(d);
		return -.5f * float2(b + d, b - d);
	}
};