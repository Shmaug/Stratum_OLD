#pragma once

#include <Math/Math.hpp>

struct Plane {
	float3 mNormal;
	float mDistance;
	
	inline Plane() : mNormal(float3()), mDistance(0) {}
	inline Plane(const float3& normal, float distance) : mNormal(normal), mDistance(distance) {}
	inline Plane(const float3& point, const float3& normal) : mNormal(normal), mDistance(dot(point, normal)) {}
};
struct Sphere {
	float3 mCenter;
	float mRadius;
	
	inline Sphere() : mCenter(float3()), mRadius(0) {}
	inline Sphere(const float3& center, float radius) : mCenter(center), mRadius(radius) {}
};
struct AABB {
	float3 mMin;
	float3 mMax;

	AABB() : mMin(float3()), mMax(float3()) {}
	AABB(const float3& min, const float3& max) : mMin(min), mMax(max) {}
	AABB(const AABB& aabb) : mMin(aabb.mMin), mMax(aabb.mMax) {}
	AABB(const AABB& aabb, const float4x4& transform) : AABB(aabb) {
		*this *= transform;
	}

	inline float3 Center() const { return (mMax + mMin) * .5f; }
	inline float3 Extents() const { return (mMax - mMin) * .5f; }

	inline bool Intersects(const float3& point) const {
		float3 e = (mMax - mMin) * .5f;
		float3 s = point - (mMax + mMin) * .5f;
		return
			(s.x <= e.x && s.x >= -e.x) &&
			(s.y <= e.y && s.y >= -e.y) &&
			(s.z <= e.z && s.z >= -e.z);
	}
	inline bool Intersects(const Sphere& sphere) const {
		float3 e = (mMax - mMin) * .5f;
		float3 s = sphere.mCenter - (mMax + mMin) * .5f;
		float3 delta = e - s;
		float sqDist = 0.0f;
		for (int i = 0; i < 3; i++) {
			if (s[i] < -e[i]) sqDist += delta[i];
			if (s[i] >  e[i]) sqDist += delta[i];
		}
		return sqDist <= sphere.mRadius * sphere.mRadius;
	}
	inline bool Intersects(const AABB& aabb) const {
		// for each i in (x, y, z) if a_min(i) > b_max(i) or b_min(i) > a_max(i) then return false
		bool dx = (mMin.x > aabb.mMax.x) || (aabb.mMin.x > mMax.x);
		bool dy = (mMin.y > aabb.mMax.y) || (aabb.mMin.y > mMax.y);
		bool dz = (mMin.z > aabb.mMax.z) || (aabb.mMin.z > mMax.z);
		return !(dx || dy || dz);
	}

	inline void Encapsulate(const float3& p) {
		mMin = min(mMin, p);
		mMax = max(mMax, p);
	}
	inline void Encapsulate(const AABB& aabb) {
		mMin = min(aabb.mMin, mMin);
		mMax = max(aabb.mMax, mMax);
	}

	inline AABB operator *(const float4x4& transform) {
		return AABB(*this, transform);
	}
	inline AABB operator *=(const float4x4& transform) {
		float3 corners[8]{
			mMax,							// 1,1,1
			float3(mMin.x, mMax.y, mMax.z),	// 0,1,1
			float3(mMax.x, mMax.y, mMin.z),	// 1,1,0
			float3(mMin.x, mMax.y, mMin.z),	// 0,1,0
			float3(mMax.x, mMin.y, mMax.z),	// 1,0,1
			float3(mMin.x, mMin.y, mMax.z),	// 0,0,1
			float3(mMax.x, mMin.y, mMin.z),	// 1,0,0
			mMin,							// 0,0,0
		};
		for (uint32_t i = 0; i < 8; i++)
			corners[i] = (transform * float4(corners[i], 1)).xyz;
		mMin = corners[0];
		mMax = corners[0];
		for (uint32_t i = 1; i < 8; i++) {
			mMin = min(mMin, corners[i]);
			mMax = max(mMax, corners[i]);
		}
		return *this;
	}
};
struct Ray {
	float3 mOrigin;
	float3 mDirection;

	inline Ray() : mOrigin(float3()), mDirection(float3(0,0,1)) {};
	inline Ray(const float3& ro, const float3& rd) : mOrigin(ro), mDirection(rd) {};

	inline float Intersect(const Plane& plane) const {
		return -(dot(mOrigin, plane.mNormal) + plane.mDistance) / dot(mDirection, plane.mNormal);
	}
	inline float Intersect(const float3& planeNormal, const float3& planePoint) const {
		return -dot(mOrigin - planePoint, planeNormal) / dot(mDirection, planeNormal);
	}

	inline float2 Intersect(const AABB& aabb) const {
		float3 m = 1.f / mDirection;
		float3 n = m * (mOrigin - (aabb.mMax + aabb.mMin) * .5f);
		float3 k = abs(m) * (aabb.mMax - aabb.mMin) * .5f;
		float3 t1 = -n - k;
		float3 t2 = -n + k;
		float tN = fmaxf( fmaxf( t1.x, t1.y ), t1.z );
		float tF = fminf( fminf( t2.x, t2.y ), t2.z );
		if (tN > tF || tF < 0) return -1.f;
		return float2(tN, tF);
	}
	inline float2 Intersect(const Sphere& sphere) const {
		float3 pq = mOrigin - sphere.mCenter;
		float a = dot(mDirection, mDirection);
		float b = 2 * dot(pq, mDirection);
		float c = dot(pq, pq) - sphere.mRadius * sphere.mRadius;
		float d = b * b - 4 * a * c;
		if (d < 0.f) return -1.f;
		d = sqrt(d);
		return -.5f * float2(b + d, b - d) / a;
	}
};