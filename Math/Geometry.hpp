#pragma once

#include <Math/Math.hpp>

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

	inline bool Intersects(const float4 frustum[6]) const {
		float3 center = Center();
		float3 extent = Extents();
		for (uint32_t i = 0; i < 6; i++) {
			float r = dot(extent, abs(frustum[i].xyz));
			float d = dot(center, frustum[i].xyz) - frustum[i].w;
			if (d <= -r) return false;
		}
		return true;
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

	inline float Intersect(const float4& plane) const {
		return -(dot(mOrigin, plane.xyz) + plane.w) / dot(mDirection, plane.xyz);
	}
	inline float Intersect(const float3& planeNormal, const float3& planePoint) const {
		return -dot(mOrigin - planePoint, planeNormal) / dot(mDirection, planeNormal);
	}

	inline bool Intersect(const AABB& aabb, float2& t) const {
		float3 id = 1.f / mDirection;

		float3 pmin = (aabb.mMin - mOrigin) * id;
		float3 pmax = (aabb.mMax - mOrigin) * id;

		float3 mn, mx;
		mn.x = id.x >= 0.f ? pmin.x : pmax.x;
		mn.y = id.y >= 0.f ? pmin.y : pmax.y;
		mn.z = id.z >= 0.f ? pmin.z : pmax.z;
		
		mx.x = id.x >= 0.f ? pmax.x : pmin.x;
		mx.y = id.y >= 0.f ? pmax.y : pmin.y;
		mx.z = id.z >= 0.f ? pmax.z : pmin.z;

		t = float2(fmaxf(fmaxf(mn.x, mn.y), mn.z), fminf(fminf(mx.x, mx.y), mx.z));
		return t.y > t.x;
	}
	inline bool Intersect(const Sphere& sphere, float2& t) const {
		float3 pq = mOrigin - sphere.mCenter;
		float a = dot(mDirection, mDirection);
		float b = 2 * dot(pq, mDirection);
		float c = dot(pq, pq) - sphere.mRadius * sphere.mRadius;
		float d = b * b - 4 * a * c;
		if (d < 0.f) return false;
		d = sqrt(d);
		t = -.5f * float2(b + d, b - d) / a;
		return true;
	}

	inline bool Intersect(float3 v0, float3 v1, float3 v2, float3* tuv) const {
		// http://jcgt.org/published/0002/01/05/paper.pdf

		v0 -= mOrigin;
		v1 -= mOrigin;
		v2 -= mOrigin;

		float3 rd = mDirection;
		float3 ad = abs(mDirection);

		uint32_t largesti = 0;
		if (ad[largesti] < ad[1]) largesti = 1;
		if (ad[largesti] < ad[2]) largesti = 2;
		 
		float idz;
		float2 rdz;

		if (largesti == 0) {
			v0 = float3(v0.y, v0.z, v0.x);
			v1 = float3(v1.y, v1.z, v1.x);
			v2 = float3(v2.y, v2.z, v2.x);
			idz = 1.f / rd.x;
			rdz = float2(rd.y, rd.z) * idz;
		} else if (largesti == 1) {
			v0 = float3(v0.z, v0.x, v0.y);
			v1 = float3(v1.z, v1.x, v1.y);
			v2 = float3(v2.z, v2.x, v2.y);
			idz = 1.f / rd.y;
			rdz = float2(rd.z, rd.x) * idz;
		} else {
			idz = 1.f / rd.z;
			rdz = float2(rd.x, rd.y) * idz;
		}

		v0 = float3(v0.x - v0.z * rdz.x, v0.y - v0.z * rdz.y, v0.z * idz);
		v1 = float3(v1.x - v1.z * rdz.x, v1.y - v1.z * rdz.y, v1.z * idz);
		v2 = float3(v2.x - v2.z * rdz.x, v2.y - v2.z * rdz.y, v2.z * idz);

		float u = v2.x * v1.y - v2.y * v1.x;
		float v = v0.x * v2.y - v0.y * v2.x;
		float w = v1.x * v0.y - v1.y * v0.x;

		if ((u < 0 || v < 0 || w < 0) && (u > 0 || v > 0 || w > 0)) return false;

		float det = u + v + w;
		if (det == 0) return false; // co-planar

		float t = u * v0.z + v * v1.z + w * v2.z;
		if (tuv) *tuv = float3(t, u, v) / det;
		return true;
	}
};