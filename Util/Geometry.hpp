#ifndef COLLISION_HPP
#define COLLISION_HPP

#include <Util/Util.hpp>

struct Sphere {
	vec3 mCenter;
	float mRadius;
};
struct AABB {
	vec3 mCenter;
	vec3 mExtents;

	AABB() : mCenter(vec3()), mExtents(vec3()) {}
	AABB(const vec3& center, const vec3& extents) : mCenter(center), mExtents(extents) {}
	AABB(const AABB& aabb) : mCenter(aabb.mCenter), mExtents(aabb.mExtents) {}
	AABB(const AABB& aabb, const mat4& transform) {
		vec3 corners[8] {
			aabb.mCenter + aabb.mExtents * vec3( 1,  1,  1),
			aabb.mCenter + aabb.mExtents * vec3(-1,  1,  1),
			aabb.mCenter + aabb.mExtents * vec3( 1,  1, -1),
			aabb.mCenter + aabb.mExtents * vec3(-1,  1, -1),
			aabb.mCenter + aabb.mExtents * vec3( 1, -1,  1),
			aabb.mCenter + aabb.mExtents * vec3(-1, -1,  1),
			aabb.mCenter + aabb.mExtents * vec3( 1, -1, -1),
			aabb.mCenter + aabb.mExtents * vec3(-1, -1, -1),
		};

		for (uint32_t i = 0; i < 8; i++)
			corners[i] = transform * vec4(corners[i], 1);

		vec3 mn = corners[0];
		vec3 mx = corners[0];

		for (uint32_t i = 1; i < 8; i++) {
			mn = gmin(mn, corners[i]);
			mx = gmax(mx, corners[i]);
		}

		mCenter  = (mn + mx) * .5f;
		mExtents = (mx - mn) * .5f;
	}

	inline bool Intersects(const vec3& point) const {
		vec3 s = point - mCenter;
		return 
			(s.x <= mExtents.x && s.x >= -mExtents.x) &&
			(s.y <= mExtents.y && s.y >= -mExtents.y) &&
			(s.z <= mExtents.z && s.z >= -mExtents.z);
	}
	inline bool Intersects(const Sphere& sphere) const {
		vec3 s = sphere.mCenter - mCenter;
		vec3 delta = mExtents - s;
		float sqDist = 0.0f;
		for (int i = 0; i < 3; i++) {
			if (s[i] < -mExtents[i]) sqDist += delta[i];
			if (s[i] > mExtents[i]) sqDist += delta[i];
		}
		return sqDist <= sphere.mRadius * sphere.mRadius;
	}
	inline bool Intersects(const AABB& aabb) const {
		vec3 MinA = mCenter - mExtents;
		vec3 MaxA = mCenter + mExtents;

		vec3 MinB = aabb.mCenter - aabb.mExtents;
		vec3 MaxB = aabb.mCenter + aabb.mExtents;

		// for each i in (x, y, z) if a_min(i) > b_max(i) or b_min(i) > a_max(i) then return false
		bool dx = (MinA.x > MaxB.x) || (MinB.x > MaxA.x);
		bool dy = (MinA.y > MaxB.y) || (MinB.y > MaxA.y);
		bool dz = (MinA.z > MaxB.z) || (MinB.z > MaxA.z);

		return !(dx || dy || dz);
	}

	inline void Encapsulate(const AABB& aabb) {
		vec3 mn = gmin(mCenter - mExtents, aabb.mCenter - aabb.mExtents);
		vec3 mx = gmax(mCenter + mExtents, aabb.mCenter + aabb.mExtents);
		mCenter = (mn + mx) * .5f;
		mExtents = (mx - mn) * .5f;
	}

	inline AABB operator *(const mat4& transform) {
		return AABB(*this, transform);
	}
	inline AABB operator *=(const mat4& transform) {
		vec3 corners[8]{
			mCenter + mExtents * vec3( 1,  1,  1),
			mCenter + mExtents * vec3(-1,  1,  1),
			mCenter + mExtents * vec3( 1,  1, -1),
			mCenter + mExtents * vec3(-1,  1, -1),
			mCenter + mExtents * vec3( 1, -1,  1),
			mCenter + mExtents * vec3(-1, -1,  1),
			mCenter + mExtents * vec3( 1, -1, -1),
			mCenter + mExtents * vec3(-1, -1, -1),
		};

		for (uint32_t i = 0; i < 8; i++)
			corners[i] = transform * vec4(corners[i], 1);

		vec3 mn = corners[0];
		vec3 mx = corners[0];

		for (uint32_t i = 1; i < 8; i++) {
			mn = gmin(mn, corners[i]);
			mx = gmax(mx, corners[i]);
		}

		mCenter = (mn + mx) * .5f;
		mExtents = (mx - mn) * .5f;

		return *this;
	}
};
struct OBB {
	vec3 mCenter;
	vec3 mExtents;
	quat mOrientation;

	inline OBB() : mCenter({}), mExtents({}), mOrientation(quat(1.f, 0.f, 0.f, 0.f)) {}
	inline OBB(const vec3& center, const vec3& extents) : mCenter(center), mExtents(extents), mOrientation(quat(1.f, 0.f, 0.f, 0.f)) {}
	inline OBB(const vec3& center, const vec3& extents, const quat& orientation) : mCenter(center), mExtents(extents), mOrientation(orientation) {}
	
	inline bool Intersects(const vec3& point) const {
		vec3 s = inverse(mOrientation) * (point - mCenter);
		return
			(s.x <= mExtents.x && s.x >= -mExtents.x) &&
			(s.y <= mExtents.y && s.y >= -mExtents.y) &&
			(s.z <= mExtents.z && s.z >= -mExtents.z);
	}
	inline bool Intersects(const Sphere& sphere) const {
		vec3 s = inverse(mOrientation) * (sphere.mCenter - mCenter);
		vec3 delta = mExtents - s;
		float sqDist = 0.0f;
		for (int i = 0; i < 3; i++) {
			if (s[i] < -mExtents[i]) sqDist += delta[i];
			if (s[i] >  mExtents[i]) sqDist += delta[i];
		}
		return sqDist <= sphere.mRadius * sphere.mRadius;
	}
};

struct Ray {
	vec3 mOrigin;
	vec3 mDirection;

	inline vec2 Intersect(const AABB& aabb) const {
		vec3 m = 1.f / mDirection;
		vec3 n = m * (mOrigin - aabb.mCenter);
		vec3 k = abs(m) * aabb.mExtents;
		vec3 t1 = -n - k;
		vec3 t2 = -n + k;
		float tN = gmax(gmax(t1.x, t1.y), t1.z);
		float tF = gmin(gmin(t2.x, t2.y), t2.z);
		if (tN > tF || tF < 0.f) return vec2(-1.f);
		return vec2(tN, tF);
	}
	inline vec2 Intersect(const OBB& obb) const {
		quat io = inverse(obb.mOrientation);
		vec3 m = 1.f / (io * mDirection);
		vec3 n = m * (io * (mOrigin - obb.mCenter));
		vec3 k = abs(m) * obb.mExtents;
		vec3 t1 = -n - k;
		vec3 t2 = -n + k;
		float tN = gmax(gmax(t1.x, t1.y), t1.z);
		float tF = gmin(gmin(t2.x, t2.y), t2.z);
		if (tN > tF || tF < 0.f) return vec2(-1.f);
		return vec2(tN, tF);
	}
	inline vec2 Intersect(const Sphere& sphere) const {
		vec3 pq = mOrigin - sphere.mCenter;
		float b = dot(pq, mDirection);
		float d = b * b - 4.f * dot(pq, pq);
		if (d < 0.f) return vec2(-1.f);
		d = sqrt(d);
		return -.5f * vec2(b + d, b - d);
	}
};

#endif