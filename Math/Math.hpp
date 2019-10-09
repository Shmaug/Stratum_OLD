#pragma once

#include <cassert>

#define PI 3.1415926535897932384626433832795f

template <class T>
inline void hash_combine(std::size_t& seed, const T& v) {
	std::hash<T> hasher;
	seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

#pragma pack(push)
#pragma pack(1)

struct int2 {
	int32_t x, y;

	inline int2(int32_t x, int32_t y) : x(x), y(y) {};
	inline int2(int32_t s) : x(s), y(s) {};
	inline int2() : int2(0) {};

	inline int2 operator=(uint32_t v) {
		x = y = v;
		return *this;
	}
	inline int2 operator=(const int2& v) {
		x = v.x;
		y = v.y;
		return *this;
	}

	inline int2 operator -() const {
		return int2(-x, -y);
	}
	inline int2 operator -(int32_t s) const {
		return int2(x - s, y - s);
	}
	inline int2 operator -(const int2& v) const {
		return int2(x - v.x, y - v.y);
	}
	inline int2 operator -=(int32_t s) {
		x -= s;
		y -= s;
		return *this;
	}
	inline int2 operator -=(const int2& v) {
		x -= v.x;
		y -= v.y;
		return *this;
	}
	inline friend int2 operator -(int32_t a, const int2& v) { return -v + a; }

	inline int2 operator +(int32_t s) const {
		return int2(x + s, y + s);
	}
	inline int2 operator +(const int2& v) const {
		return int2(x + v.x, y + v.y);
	}
	inline int2 operator +=(int32_t s) {
		x += s;
		y += s;
		return *this;
	}
	inline int2 operator +=(const int2& v) {
		x += v.x;
		y += v.y;
		return *this;
	}
	inline friend int2 operator +(int32_t a, const int2& v) { return v + a; }

	inline int2 operator *(int32_t s) const {
		return int2(x * s, y * s);
	}
	inline int2 operator *(const int2& v) const {
		return int2(x * v.x, y * v.y);
	}
	inline int2 operator *=(int32_t s) {
		x *= s;
		y *= s;
		return *this;
	}
	inline int2 operator *=(const int2& v) {
		x *= v.x;
		y *= v.y;
		return *this;
	}
	inline friend int2 operator *(int32_t a, const int2& v) { return v * a; }

	inline friend int2 operator /(int32_t a, const int2& v) {
		return int2(a / v.x, a / v.y);
	}
	inline int2 operator /(int32_t s) const {
		return int2(x / s, y / s);
	}
	inline int2 operator /(const int2& s) const {
		return int2(x / s.x, y / s.y);
	}
	inline int2 operator /=(int32_t s) {
		x /= s;
		y /= s;
		return *this;
	}
	inline int2 operator /=(const int2& v) {
		x /= v.x;
		y /= v.y;
		return *this;
	}

	inline int32_t& operator[](int32_t i) {
		assert(i >= 0 && i < 2);
		return reinterpret_cast<int32_t*>(this)[i];
	}
	inline int32_t operator[](int32_t i) const {
		assert(i >= 0 && i < 2);
		return reinterpret_cast<const int32_t*>(this)[i];
	}
};
struct int3 {
	union {
		struct { int32_t x, y, z; };
		int2 xy;
	};

	inline int3(const int2& v, int32_t z) : x(v.x), y(v.y), z(z) {};
	inline int3(int32_t x, int32_t y, int32_t z) : x(x), y(y), z(z) {};
	inline int3(int32_t s) : x(s), y(s), z(s) {};
	inline int3() : int3(0) {};

	inline int3 operator=(uint32_t v) {
		x = y = z = v;
		return *this;
	}
	inline int3 operator=(const int3& v) {
		x = v.x;
		y = v.y;
		z = v.z;
		return *this;
	}
	inline int3 operator -() const {
		return int3(-x, -y, -z);
	}
	inline int3 operator -(int32_t s) const {
		return int3(x - s, y - s, z - s);
	}
	inline int3 operator -(const int3& v) const {
		return int3(x - v.x, y - v.y, z - v.z);
	}
	inline int3 operator -=(int32_t s) {
		x -= s;
		y -= s;
		z -= s;
		return *this;
	}
	inline int3 operator -=(const int3& v) {
		x -= v.x;
		y -= v.y;
		z -= v.z;
		return *this;
	}
	inline friend int3 operator -(int32_t a, const int3& v) { return -v + a; }

	inline int3 operator +(int32_t s) const {
		return int3(x + s, y + s, z + s);
	}
	inline int3 operator +(const int3& v) const {
		return int3(x + v.x, y + v.y, z + v.z);
	}
	inline int3 operator +=(int32_t s) {
		x += s;
		y += s;
		z += s;
		return *this;
	}
	inline int3 operator +=(const int3& v) {
		x += v.x;
		y += v.y;
		z += v.z;
		return *this;
	}
	inline friend int3 operator +(int32_t a, const int3& v) { return v + a; }

	inline int3 operator *(int32_t s) const {
		return int3(x * s, y * s, z * s);
	}
	inline int3 operator *(const int3& v) const {
		return int3(x * v.x, y * v.y, z * v.z);
	}
	inline int3 operator *=(int32_t s) {
		x *= s;
		y *= s;
		z *= s;
		return *this;
	}
	inline int3 operator *=(const int3& v) {
		x *= v.x;
		y *= v.y;
		z *= v.z;
		return *this;
	}
	inline friend int3 operator *(int32_t a, const int3& v) { return v * a; }

	inline friend int3 operator /(int32_t a, const int3& v) {
		return int3(a / v.x, a / v.y, a / v.z);
	}
	inline int3 operator /(int32_t s) const {
		return int3(x / s, y / s, z / s);
	}
	inline int3 operator /(const int3& s) const {
		return int3(x / s.x, y / s.y, z / s.z);
	}
	inline int3 operator /=(int32_t s) {
		x /= s;
		y /= s;
		z /= s;
		return *this;
	}
	inline int3 operator /=(const int3& v) {
		x /= v.x;
		y /= v.y;
		z /= v.z;
		return *this;
	}

	inline int32_t& operator[](int32_t i) {
		assert(i >= 0 && i < 3);
		return reinterpret_cast<int32_t*>(this)[i];
	}
	inline int32_t operator[](int32_t i) const {
		assert(i >= 0 && i < 3);
		return reinterpret_cast<const int32_t*>(this)[i];
	}
};
struct int4 {
	union {
		struct { int32_t x, y, z, w; };
		struct { int2 xy; int2 zw; };
		int3 xyz;
	};

	inline int4 operator=(uint32_t v) {
		x = y = z = w = v;
		return *this;
	}
	inline int4 operator=(const int4& v) {
		x = v.x;
		y = v.y;
		z = v.z;
		w = v.w;
		return *this;
	}
	inline int4(const int2& v0, const int2& v1) : x(v0.x), y(v0.y), z(v1.x), w(v1.y) {};
	inline int4(const int3& v, int32_t w) : x(v.x), y(v.y), z(v.z), w(w) {};
	inline int4(const int2& v, int32_t z, int32_t w) : x(v.x), y(v.y), z(z), w(w) {};
	inline int4(int32_t x, int32_t y, int32_t z, int32_t w) : x(x), y(y), z(z), w(w) {};
	inline int4(int32_t s) : x(s), y(s), z(s), w(s) {};
	inline int4() : int4(0) {};

	inline int4 operator -() const {
		return int4(-x, -y, -z, -w);
	}
	inline int4 operator -(int32_t s) const {
		return int4(x - s, y - s, z - s, w - s);
	}
	inline int4 operator -(const int4& v) const {
		return int4(x - v.x, y - v.y, z - v.z, w - v.w);
	}
	inline int4 operator -=(int32_t s) {
		x -= s;
		y -= s;
		z -= s;
		w -= s;
		return *this;
	}
	inline int4 operator -=(const int4& v) {
		x -= v.x;
		y -= v.y;
		z -= v.z;
		w -= v.w;
		return *this;
	}
	inline friend int4 operator -(int32_t a, const int4& v) { return -v + a; }

	inline int4 operator +(int32_t s) const {
		return int4(x + s, y + s, z + s, w + s);
	}
	inline int4 operator +(const int4& v) const {
		return int4(x + v.x, y + v.y, z + v.z, w + v.w);
	}
	inline int4 operator +=(int32_t s) {
		x += s;
		y += s;
		z += s;
		w += s;
		return *this;
	}
	inline int4 operator +=(const int4& v) {
		x += v.x;
		y += v.y;
		z += v.z;
		w += v.w;
		return *this;
	}
	inline friend int4 operator +(int32_t a, const int4& v) { return v + a; }

	inline int4 operator *(int32_t s) const {
		return int4(x * s, y * s, z * s, w * s);
	}
	inline int4 operator *(const int4& v) const {
		return int4(x * v.x, y * v.y, z * v.z, w * v.w);
	}
	inline int4 operator *=(int32_t s) {
		x *= s;
		y *= s;
		z *= s;
		w *= s;
		return *this;
	}
	inline int4 operator *=(const int4& v) {
		x *= v.x;
		y *= v.y;
		z *= v.z;
		w *= v.w;
		return *this;
	}
	inline friend int4 operator *(int32_t a, const int4& v) { return v * a; }

	inline friend int4 operator /(int32_t a, const int4& v) {
		return int4(a / v.x, a / v.y, a / v.z, a / v.w);
	}
	inline int4 operator /(int32_t s) const {
		return int4(x / s, y / s, z / s, w / s);
	}
	inline int4 operator /(const int4& s) const {
		return int4(x / s.x, y / s.y, z / s.z, w / s.w);
	}
	inline int4 operator /=(int32_t s) {
		x /= s;
		y /= s;
		z /= s;
		w /= s;
		return *this;
	}
	inline int4 operator /=(const int4& v) {
		x /= v.x;
		y /= v.y;
		z /= v.z;
		w /= v.w;
		return *this;
	}

	inline int32_t& operator[](int32_t i) {
		assert(i >= 0 && i < 4);
		return reinterpret_cast<int32_t*>(this)[i];
	}
	inline int32_t operator[](int32_t i) const {
		assert(i >= 0 && i < 4);
		return reinterpret_cast<const int32_t*>(this)[i];
	}
};

struct uint2 {
	uint32_t x, y;

	inline uint2(uint32_t x, uint32_t y) : x(x), y(y) {};
	inline uint2(uint32_t s) : x(s), y(s) {};
	inline uint2() : uint2(0) {};

	inline uint2 operator=(uint32_t v) {
		x = y = v;
		return *this;
	}
	inline uint2 operator=(const uint2& v) {
		x = v.x;
		y = v.y;
		return *this;
	}

	inline int2 operator -() const {
		return int2(-(int32_t)x, -(int32_t)y);
	}
	inline uint2 operator -(uint32_t s) const {
		return uint2(x - s, y - s);
	}
	inline uint2 operator -(const uint2& v) const {
		return uint2(x - v.x, y - v.y);
	}
	inline uint2 operator -=(uint32_t s) {
		x -= s;
		y -= s;
		return *this;
	}
	inline uint2 operator -=(const uint2& v) {
		x -= v.x;
		y -= v.y;
		return *this;
	}
	inline friend uint2 operator -(uint32_t a, const uint2& v) { return uint2(a - v.x, a - v.y); }

	inline uint2 operator +(uint32_t s) const {
		return uint2(x + s, y + s);
	}
	inline uint2 operator +(const uint2& v) const {
		return uint2(x + v.x, y + v.y);
	}
	inline uint2 operator +=(uint32_t s) {
		x += s;
		y += s;
		return *this;
	}
	inline uint2 operator +=(const uint2& v) {
		x += v.x;
		y += v.y;
		return *this;
	}
	inline friend uint2 operator +(uint32_t a, const uint2& v) { return v + a; }

	inline uint2 operator *(uint32_t s) const {
		return uint2(x * s, y * s);
	}
	inline uint2 operator *(const uint2& v) const {
		return uint2(x * v.x, y * v.y);
	}
	inline uint2 operator *=(uint32_t s) {
		x *= s;
		y *= s;
		return *this;
	}
	inline uint2 operator *=(const uint2& v) {
		x *= v.x;
		y *= v.y;
		return *this;
	}
	inline friend uint2 operator *(uint32_t a, const uint2& v) { return v * a; }

	inline friend uint2 operator /(uint32_t a, const uint2& v) {
		return uint2(a / v.x, a / v.y);
	}
	inline uint2 operator /(uint32_t s) const {
		return uint2(x / s, y / s);
	}
	inline uint2 operator /(const uint2& s) const {
		return uint2(x / s.x, y / s.y);
	}
	inline uint2 operator /=(uint32_t s) {
		x /= s;
		y /= s;
		return *this;
	}
	inline uint2 operator /=(const uint2& v) {
		x /= v.x;
		y /= v.y;
		return *this;
	}

	inline uint32_t& operator[](int i) {
		assert(i >= 0 && i < 2);
		return reinterpret_cast<uint32_t*>(this)[i];
	}
	inline uint32_t operator[](int i) const {
		assert(i >= 0 && i < 2);
		return reinterpret_cast<const uint32_t*>(this)[i];
	}
};
struct uint3 {
	union {
		struct { uint32_t x, y, z; };
		uint2 xy;
	};

	inline uint3(const uint2& v, uint32_t z) : x(v.x), y(v.y), z(z) {};
	inline uint3(uint32_t x, uint32_t y, uint32_t z) : x(x), y(y), z(z) {};
	inline uint3(uint32_t s) : x(s), y(s), z(s) {};
	inline uint3() : uint3(0) {};

	inline uint3 operator=(uint32_t v) {
		x = y = z = v;
		return *this;
	}
	inline uint3 operator=(const uint3& v) {
		x = v.x;
		y = v.y;
		z = v.z;
		return *this;
	}

	inline int3 operator -() const {
		return int3(-(int32_t)x, -(int32_t)y, -(int32_t)z);
	}
	inline uint3 operator -(uint32_t s) const {
		return uint3(x - s, y - s, z - s);
	}
	inline uint3 operator -(const uint3& v) const {
		return uint3(x - v.x, y - v.y, z - v.z);
	}
	inline uint3 operator -=(uint32_t s) {
		x -= s;
		y -= s;
		z -= s;
		return *this;
	}
	inline uint3 operator -=(const uint3& v) {
		x -= v.x;
		y -= v.y;
		z -= v.z;
		return *this;
	}
	inline friend uint3 operator -(uint32_t a, const uint3& v) { return uint3(a - v.x, a - v.y, a - v.z); }

	inline uint3 operator +(uint32_t s) const {
		return uint3(x + s, y + s, z + s);
	}
	inline uint3 operator +(const uint3& v) const {
		return uint3(x + v.x, y + v.y, z + v.z);
	}
	inline uint3 operator +=(uint32_t s) {
		x += s;
		y += s;
		z += s;
		return *this;
	}
	inline uint3 operator +=(const uint3& v) {
		x += v.x;
		y += v.y;
		z += v.z;
		return *this;
	}
	inline friend uint3 operator +(uint32_t a, const uint3& v) { return v + a; }

	inline uint3 operator *(uint32_t s) const {
		return uint3(x * s, y * s, z * s);
	}
	inline uint3 operator *(const uint3& v) const {
		return uint3(x * v.x, y * v.y, z * v.z);
	}
	inline uint3 operator *=(uint32_t s) {
		x *= s;
		y *= s;
		z *= s;
		return *this;
	}
	inline uint3 operator *=(const uint3& v) {
		x *= v.x;
		y *= v.y;
		z *= v.z;
		return *this;
	}
	inline friend uint3 operator *(uint32_t a, const uint3& v) { return v * a; }

	inline friend uint3 operator /(uint32_t a, const uint3& v) {
		return uint3(a / v.x, a / v.y, a / v.z);
	}
	inline uint3 operator /(uint32_t s) const {
		return uint3(x / s, y / s, z / s);
	}
	inline uint3 operator /(const uint3& s) const {
		return uint3(x / s.x, y / s.y, z / s.z);
	}
	inline uint3 operator /=(uint32_t s) {
		x /= s;
		y /= s;
		z /= s;
		return *this;
	}
	inline uint3 operator /=(const uint3& v) {
		x /= v.x;
		y /= v.y;
		z /= v.z;
		return *this;
	}

	inline uint32_t& operator[](int i) {
		assert(i >= 0 && i < 3);
		return reinterpret_cast<uint32_t*>(this)[i];
	}
	inline uint32_t operator[](int i) const {
		assert(i >= 0 && i < 3);
		return reinterpret_cast<const uint32_t*>(this)[i];
	}
};
struct uint4 {
	union {
		struct { uint32_t x, y, z, w; };
		struct { uint2 xy; uint2 zw; };
		uint3 xyz;
	};

	inline uint4(const uint2& v0, const uint2& v1) : x(v0.x), y(v0.y), z(v1.x), w(v1.y) {};
	inline uint4(const uint3& v, uint32_t w) : x(v.x), y(v.y), z(v.z), w(w) {};
	inline uint4(const uint2& v, uint32_t z, uint32_t w) : x(v.x), y(v.y), z(z), w(w) {};
	inline uint4(uint32_t x, uint32_t y, uint32_t z, uint32_t w) : x(x), y(y), z(z), w(w) {};
	inline uint4(uint32_t s) : x(s), y(s), z(s), w(s) {};
	inline uint4() : uint4(0) {};

	inline uint4 operator=(uint32_t v) {
		x = y = z = w = v;
		return *this;
	}
	inline uint4 operator=(const uint4& v) {
		x = v.x;
		y = v.y;
		z = v.z;
		w = v.w;
		return *this;
	}

	inline int4 operator -() const {
		return int4(-(int32_t)x, -(int32_t)y, -(int32_t)z, -(int32_t)w);
	}
	inline uint4 operator -(uint32_t s) const {
		return uint4(x - s, y - s, z - s, w - s);
	}
	inline uint4 operator -(const uint4& v) const {
		return uint4(x - v.x, y - v.y, z - v.z, w - v.w);
	}
	inline uint4 operator -=(uint32_t s) {
		x -= s;
		y -= s;
		z -= s;
		w -= s;
		return *this;
	}
	inline uint4 operator -=(const uint4& v) {
		x -= v.x;
		y -= v.y;
		z -= v.z;
		w -= v.w;
		return *this;
	}
	inline friend uint4 operator -(uint32_t a, const uint4& v) { return uint4(a - v.x, a - v.y, a - v.z, a - v.w); }

	inline uint4 operator +(uint32_t s) const {
		return uint4(x + s, y + s, z + s, w + s);
	}
	inline uint4 operator +(const uint4& v) const {
		return uint4(x + v.x, y + v.y, z + v.z, w + v.w);
	}
	inline uint4 operator +=(uint32_t s) {
		x += s;
		y += s;
		z += s;
		w += s;
		return *this;
	}
	inline uint4 operator +=(const uint4& v) {
		x += v.x;
		y += v.y;
		z += v.z;
		w += v.w;
		return *this;
	}
	inline friend uint4 operator +(uint32_t a, const uint4& v) { return v + a; }

	inline uint4 operator *(uint32_t s) const {
		return uint4(x * s, y * s, z * s, w * s);
	}
	inline uint4 operator *(const uint4& v) const {
		return uint4(x * v.x, y * v.y, z * v.z, w * v.w);
	}
	inline uint4 operator *=(uint32_t s) {
		x *= s;
		y *= s;
		z *= s;
		w *= s;
		return *this;
	}
	inline uint4 operator *=(const uint4& v) {
		x *= v.x;
		y *= v.y;
		z *= v.z;
		w *= v.w;
		return *this;
	}
	inline friend uint4 operator *(uint32_t a, const uint4& v) { return v * a; }

	inline friend uint4 operator /(uint32_t a, const uint4& v) {
		return uint4(a / v.x, a / v.y, a / v.z, a / v.w);
	}
	inline uint4 operator /(uint32_t s) const {
		return uint4(x / s, y / s, z / s, w / s);
	}
	inline uint4 operator /(const uint4& s) const {
		return uint4(x / s.x, y / s.y, z / s.z, w / s.w);
	}
	inline uint4 operator /=(uint32_t s) {
		x /= s;
		y /= s;
		z /= s;
		w /= s;
		return *this;
	}
	inline uint4 operator /=(const uint4& v) {
		x /= v.x;
		y /= v.y;
		z /= v.z;
		w /= v.w;
		return *this;
	}

	inline uint32_t& operator[](int i) {
		assert(i >= 0 && i < 4);
		return reinterpret_cast<uint32_t*>(this)[i];
	}
	inline uint32_t operator[](int i) const {
		assert(i >= 0 && i < 4);
		return reinterpret_cast<const uint32_t*>(this)[i];
	}
};

struct float2 {
	float x, y;

	inline float2(const uint2& v) : x((float)v.x), y((float)v.y) {};
	inline float2(const int2& v) : x((float)v.x), y((float)v.y) {};
	inline float2(float x, float y) : x(x), y(y) {};
	inline float2(float s) : x(s), y(s) {};
	inline float2() : float2(0) {};

	inline float2 operator=(float v) {
		x = y = v;
		return *this;
	}
	inline float2 operator=(const float2& v) {
		x = v.x;
		y = v.y;
		return *this;
	}

	inline float2 operator -() const {
		return float2(-x, -y);
	}
	inline float2 operator -(float s) const {
		return float2(x - s, y - s);
	}
	inline float2 operator -(const float2& v) const {
		return float2(x - v.x, y - v.y);
	}
	inline float2 operator -=(float s) {
		x -= s;
		y -= s;
		return *this;
	}
	inline float2 operator -=(const float2& v) {
		x -= v.x;
		y -= v.y;
		return *this;
	}
	inline friend float2 operator -(float a, const float2& v) { return -v + a; }

	inline float2 operator +(float s) const {
		return float2(x + s, y + s);
	}
	inline float2 operator +(const float2& v) const {
		return float2(x + v.x, y + v.y);
	}
	inline float2 operator +=(float s) {
		x += s;
		y += s;
		return *this;
	}
	inline float2 operator +=(const float2& v) {
		x += v.x;
		y += v.y;
		return *this;
	}
	inline friend float2 operator +(float a, const float2& v) { return v + a; }

	inline float2 operator *(float s) const {
		return float2(x * s, y * s);
	}
	inline float2 operator *(const float2& v) const {
		return float2(x * v.x, y * v.y);
	}
	inline float2 operator *=(float s) {
		x *= s;
		y *= s;
		return *this;
	}
	inline float2 operator *=(const float2& v) {
		x *= v.x;
		y *= v.y;
		return *this;
	}
	inline friend float2 operator *(float a, const float2& v) { return v * a; }

	inline friend float2 operator /(float a, const float2& v) {
		return float2(a / v.x, a / v.y);
	}
	inline float2 operator /(float s) const { return operator*(1.f / s); }
	inline float2 operator /(const float2& s) const { return operator*(1.f / s); }
	inline float2 operator /=(float s) { return operator*=(1.f / s); }
	inline float2 operator /=(const float2& v) { return operator*=(1.f / v); }

	inline float& operator[](int i) {
		assert(i >= 0 && i < 2);
		return reinterpret_cast<float*>(this)[i];
	}
	inline float operator[](int i) const {
		assert(i >= 0 && i < 2);
		return reinterpret_cast<const float*>(this)[i];
	}
};
struct float3 {
	union {
		struct { float x, y, z; };
		struct { float r, g, b; };
		float2 xy;
		float2 rg;
	};

	inline float3(const uint3& v) : x((float)v.x), y((float)v.y), z((float)v.z) {};
	inline float3(const int3& v) : x((float)v.x), y((float)v.y), z((float)v.z) {};
	inline float3(const float2& v, float z) : x(v.x), y(v.y), z(z) {};
	inline float3(float x, float y, float z) : x(x), y(y), z(z) {};
	inline float3(float s) : x(s), y(s), z(s) {};
	inline float3() : float3(0) {};

	inline float3 operator=(float v) {
		x = y = z = v;
		return *this;
	}
	inline float3 operator=(const float3& v) {
		x = v.x;
		y = v.y;
		z = v.z;
		return *this;
	}

	inline float3 operator -() const {
		return float3(-x, -y, -z);
	}
	inline float3 operator -(float s) const {
		return float3(x - s, y - s, z - s);
	}
	inline float3 operator -(const float3& v) const {
		return float3(x - v.x, y - v.y, z - v.z);
	}
	inline float3 operator -=(float s) {
		x -= s;
		y -= s;
		z -= s;
		return *this;
	}
	inline float3 operator -=(const float3& v) {
		x -= v.x;
		y -= v.y;
		z -= v.z;
		return *this;
	}
	inline friend float3 operator -(float a, const float3& v) { return -v + a; }

	inline float3 operator +(float s) const {
		return float3(x + s, y + s, z + s);
	}
	inline float3 operator +(const float3& v) const {
		return float3(x + v.x, y + v.y, z + v.z);
	}
	inline float3 operator +=(float s) {
		x += s;
		y += s;
		z += s;
		return *this;
	}
	inline float3 operator +=(const float3& v) {
		x += v.x;
		y += v.y;
		z += v.z;
		return *this;
	}
	inline friend float3 operator +(float a, const float3& v) { return v + a; }

	inline float3 operator *(float s) const {
		return float3(x * s, y * s, z * s);
	}
	inline float3 operator *(const float3& v) const {
		return float3(x * v.x, y * v.y, z * v.z);
	}
	inline float3 operator *=(float s) {
		x *= s;
		y *= s;
		z *= s;
		return *this;
	}
	inline float3 operator *=(const float3& v) {
		x *= v.x;
		y *= v.y;
		z *= v.z;
		return *this;
	}
	inline friend float3 operator *(float a, const float3& v) { return v * a; }

	inline friend float3 operator /(float a, const float3& v) {
		return float3(a / v.x, a / v.y, a / v.z);
	}
	inline float3 operator /(float s) const { return operator*(1.f / s); }
	inline float3 operator /(const float3& s) const { return operator*(1.f / s); }
	inline float3 operator /=(float s) { return operator*=(1.f / s); }
	inline float3 operator /=(const float3& v) { return operator*=(1.f / v); }

	inline float& operator[](int i) {
		assert(i >= 0 && i < 3);
		return reinterpret_cast<float*>(this)[i];
	}
	inline float operator[](int i) const {
		assert(i >= 0 && i < 3);
		return reinterpret_cast<const float*>(this)[i];
	}
};
struct float4 {
	union {
		struct { float x, y, z, w; };
		struct { float r, g, b, a; };
		struct { float2 xy; float2 zw; };
		struct { float2 rg; float2 ba; };
		float3 xyz;
		float3 rgb;
	};

	inline float4(const uint4& v) : x((float)v.x), y((float)v.y), z((float)v.z), w((float)v.w) {};
	inline float4(const int4& v) : x((float)v.x), y((float)v.y), z((float)v.z), w((float)v.w) {};
	inline float4(const float2& v0, const float2& v1) : x(v0.x), y(v0.y), z(v1.x), w(v1.y) {};
	inline float4(const float3& v, float w) : x(v.x), y(v.y), z(v.z), w(w) {};
	inline float4(const float2& v, float z, float w) : x(v.x), y(v.y), z(z), w(w) {};
	inline float4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {};
	inline float4(float s) : x(s), y(s), z(s), w(s) {};
	inline float4() : float4(0) {};

	inline float4 operator=(float v) {
		x = y = z = w = v;
		return *this;
	}
	inline float4 operator=(const float4& v) {
		x = v.x;
		y = v.y;
		z = v.z;
		w = v.w;
		return *this;
	}

	inline float4 operator -() const {
		return float4(-x, -y, -z, -w);
	}
	inline float4 operator -(float s) const {
		return float4(x - s, y - s, z - s, w - s);
	}
	inline float4 operator -(const float4& v) const {
		return float4(x - v.x, y - v.y, z - v.z, w - v.w);
	}
	inline float4 operator -=(float s) {
		x -= s;
		y -= s;
		z -= s;
		w -= s;
		return *this;
	}
	inline float4 operator -=(const float4& v) {
		x -= v.x;
		y -= v.y;
		z -= v.z;
		w -= v.w;
		return *this;
	}
	inline friend float4 operator -(float a, const float4& v) { return -v + a; }

	inline float4 operator +(float s) const {
		return float4(x + s, y + s, z + s, w + s);
	}
	inline float4 operator +(const float4& v) const {
		return float4(x + v.x, y + v.y, z + v.z, w + v.w);
	}
	inline float4 operator +=(float s) {
		x += s;
		y += s;
		z += s;
		w += s;
		return *this;
	}
	inline float4 operator +=(const float4& v) {
		x += v.x;
		y += v.y;
		z += v.z;
		w += v.w;
		return *this;
	}
	inline friend float4 operator +(float a, const float4& v) { return v + a; }

	inline float4 operator *(float s) const {
		return float4(x * s, y * s, z * s, w * s);
	}
	inline float4 operator *(const float4& v) const {
		return float4(x * v.x, y * v.y, z * v.z, w * v.w);
	}
	inline float4 operator *=(float s) {
		x *= s;
		y *= s;
		z *= s;
		w *= s;
		return *this;
	}
	inline float4 operator *=(const float4& v) {
		x *= v.x;
		y *= v.y;
		z *= v.z;
		w *= v.w;
		return *this;
	}
	inline friend float4 operator *(float a, const float4& v) { return v * a; }

	inline friend float4 operator /(float a, const float4& v) {
		return float4(a / v.x, a / v.y, a / v.z, a / v.w);
	}
	inline float4 operator /(float s) const { return operator*(1.f / s); }
	inline float4 operator /(const float4& s) const { return operator*(1.f / s); }
	inline float4 operator /=(float s) { return operator*=(1.f / s); }
	inline float4 operator /=(const float4& v) { return operator*=(1.f / v); }

	inline float& operator[](int i) {
		assert(i >= 0 && i < 4);
		return reinterpret_cast<float*>(this)[i];
	}
	inline float operator[](int i) const {
		assert(i >= 0 && i < 4);
		return reinterpret_cast<const float*>(this)[i];
	}
};
#pragma pack(pop)

#pragma region trigonometry
inline float2 sin(const float2& v) {
	return float2(
		sinf(v.x),
		sinf(v.y) );
}
inline float3 sin(const float3& v) {
	return float3(
		sinf(v.x),
		sinf(v.y),
		sinf(v.z));
}
inline float4 sin(const float4& v) {
	return float4(
		sinf(v.x),
		sinf(v.y),
		sinf(v.z),
		sinf(v.w));
}
inline float2 cos(const float2& v) {
	return float2(
		cosf(v.x),
		cosf(v.y));
}
inline float3 cos(const float3& v) {
	return float3(
		cosf(v.x),
		cosf(v.y),
		cosf(v.z));
}
inline float4 cos(const float4& v) {
	return float4(
		cosf(v.x),
		cosf(v.y),
		cosf(v.z),
		cosf(v.w));
}
inline float2 tan(const float2& v) {
	return float2(
		tanf(v.x),
		tanf(v.y));
}
inline float3 tan(const float3& v) {
	return float3(
		tanf(v.x),
		tanf(v.y),
		tanf(v.z));
}
inline float4 tan(const float4& v) {
	return float4(
		tanf(v.x),
		tanf(v.y),
		tanf(v.z),
		tanf(v.w));
}

inline float radians(float d) {
	return d * PI / 180.f;
}
inline float2 radians(const float2& d) {
	return d * PI / 180.f;
}
inline float3 radians(const float3& d) {
	return d * PI / 180.f;
}
inline float4 radians(const float4& d) {
	return d * PI / 180.f;
}
inline float degrees(float r) {
	return r * 180.f / PI;
}
inline float2 degrees(const float2& r) {
	return r * 180.f / PI;
}
inline float3 degrees(const float3& r) {
	return r * 180.f / PI;
}
inline float4 degrees(const float4& r) {
	return r * 180.f / PI;
}
#pragma endregion

inline float dot(const float4& a, const float4& b) {
	return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}
inline float dot(const float3& a, const float3& b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline float dot(const float2& a, const float2& b) {
	return a.x * b.x + a.y * b.y;
}

inline float3 cross(const float3& a, const float3& b) {
	return float3(a.y * b.z - b.y * a.z, a.z * b.x - b.z * a.x, a.x * b.y - a.y * b.x);
}

inline float length(const float2& v) {
	return sqrtf(dot(v, v));
}
inline float length(const float3& v) {
	return sqrtf(dot(v, v));
}
inline float length(const float4& v) {
	return sqrtf(dot(v, v));
}

inline float2 normalize(const float2& v) {
	return v / length(v);
}
inline float3 normalize(const float3& v) {
	return v / length(v);
}
inline float4 normalize(const float4& v) {
	return v / length(v);
}

#pragma pack(push)
#pragma pack(1)
struct quaternion {
	union {
		struct { float x, y, z, w; };
		float3 xyz;
		float4 xyzw;
	};

	inline quaternion(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {};
	inline quaternion(const float3& euler) {
		float3 c = cos(euler * .5f);
		float3 s = sin(euler * .5f);

		x = s.x * c.y * c.z - c.x * s.y * s.z;
		y = c.x * s.y * c.z + s.x * c.y * s.z;
		z = c.x * c.y * s.z - s.x * s.y * c.z;
		w = c.x * c.y * c.z + s.x * s.y * s.z;
	};
	inline quaternion() : quaternion(0, 0, 0, 1) {};

	inline quaternion operator =(const quaternion& q) {
		x = q.x;
		y = q.y;
		z = q.z;
		w = q.w;
		return *this;
	}

	inline quaternion operator *(const quaternion& s) const {
		return quaternion(
			w * s.x + s.w * x + y * s.z - s.y * z,
			w * s.y + s.w * y + z * s.x - s.z * x,
			w * s.z + s.w * z + x * s.y - s.x * y,
			w * s.w - x * s.x - y * s.y - z * s.z);
	}
	inline quaternion operator *=(const quaternion& s) {
		*this = *this * s;
		return *this;
	}

	inline quaternion operator *(float s) const {
		return quaternion(x * s, y * s, z * s, w * s);
	}
	inline quaternion operator *=(float s) {
		x *= s;
		y *= s;
		z *= s;
		w *= s;
		return *this;
	}

	inline quaternion operator /(float s) const { return operator*(1.f / s); }
	inline quaternion operator /=(float s) { return operator*=(1.f / s); }

	inline float3 operator *(float3 vec) const {
		return 2 * dot(xyz, vec) * xyz + (w * w - dot(xyz, xyz)) * vec + 2 * w * cross(xyz, vec);
	}
};

// Column-major 2x2 matrix
struct float2x2 {
	float
		m11, m21,
		m12, m22;

	inline float2x2(float m11, float m21, float m12, float m22)
		: m11(m11), m21(m21),
		  m12(m12), m22(m22) {};
	inline float2x2(const float2& r1, const float2& r2) : float2x2(
		r1.x, r1.y,
		r2.x, r2.y) {};
	inline float2x2(float s) : float2x2(
		s, 0,
		0, s) {};
	inline float2x2() : float2x2(1) {};

	inline float2& operator[](int i) {
		assert(i >= 0 && i < 2);
		return reinterpret_cast<float2*>(this)[i];
	}
	inline float2 operator[](int i) const {
		assert(i >= 0 && i < 2);
		return reinterpret_cast<const float2*>(this)[i];
	}

	inline float2x2 operator=(const float2x2& m) {
		memcpy(this, &m, sizeof(float2x2));
		return *this;
	}

	inline float2x2 operator*(const float& s) const {
		return float2x2(
			m11 * s, m21 * s,
			m12 * s, m22 * s );
	}
	inline float2x2 operator*=(const float& s) {
		m11 *= s; m21 *= s;
		m12 *= s; m22 *= s;
		return *this;
	}
	inline float2x2 operator/(const float& s) const { return operator *(1.f / s); }
	inline float2x2 operator/=(const float& s) { return operator *=(1.f / s); }

};
// Column-major 3x3 matrix
struct float3x3 {
	float
		m11, m21, m31,
		m12, m22, m32,
		m13, m23, m33;

	inline float3x3(
		float m11, float m21, float m31,
		float m12, float m22, float m32,
		float m13, float m23, float m33)
		: m11(m11), m21(m21), m31(m31),
		m12(m12), m22(m22), m32(m32),
		m13(m13), m23(m23), m33(m33) {};
	inline float3x3(const float3& r1, const float3& r2, const float3& r3) : float3x3(
		r1.x, r1.y, r1.z,
		r2.x, r2.y, r2.z,
		r3.x, r3.y, r3.z) {};
	inline float3x3(float s) : float3x3(
		s, 0, 0,
		0, s, 0,
		0, 0, s) {};
	inline float3x3() : float3x3(1) {};
	inline float3x3(const quaternion& q) : float3x3(1) {
		float qxx = q.x * q.x;
		float qyy = q.y * q.y;
		float qzz = q.z * q.z;
		float qxz = q.x * q.z;
		float qxy = q.x * q.y;
		float qyz = q.y * q.z;
		float qwx = q.w * q.x;
		float qwy = q.w * q.y;
		float qwz = q.w * q.z;
		m11 = 1 - 2 * (qyy + qzz);
		m12 = 2 * (qxy + qwz);
		m13 = 2 * (qxz - qwy);
		m21 = 2 * (qxy - qwz);
		m22 = 1 - 2 * (qxx + qzz);
		m23 = 2 * (qyz + qwx);
		m31 = 2 * (qxz + qwy);
		m32 = 2 * (qyz - qwx);
		m33 = 1 - 2 * (qxx + qyy);
	}

	// column-major vector index
	inline float3& operator[](int i) {
		assert(i >= 0 && i < 3);
		return reinterpret_cast<float3*>(this)[i];
	}
	// column-major vector index
	inline float3 operator[](int i) const {
		assert(i >= 0 && i < 3);
		return reinterpret_cast<const float3*>(this)[i];
	}

	inline float3x3 operator=(const float3x3& m) {
		memcpy(this, &m, sizeof(float3x3));
		return *this;
	}

	inline float3x3 operator*(const float& s) const {
		return float3x3(
			m11 * s, m21 * s, m31 * s,
			m12 * s, m22 * s, m32 * s,
			m13 * s, m23 * s, m33 * s );
	}
	inline float3x3 operator*=(const float& s) {
		m11 *= s; m21 *= s; m31 *= s;
		m12 *= s; m22 *= s; m32 *= s;
		m13 *= s; m23 *= s; m33 *= s;
		return *this;
	}
	inline float3x3 operator/(const float& s) const { return operator *(1.f / s); }
	inline float3x3 operator/=(const float& s) { return operator *=(1.f / s); }

};
// Column-major 4x4 matrix
struct float4x4 {
	float
		m11, m12, m13, m14,
		m21, m22, m23, m24,
		m31, m32, m33, m34,
		m41, m42, m43, m44;

	inline float4x4(
		float m11, float m21, float m31, float m41,
		float m12, float m22, float m32, float m42,
		float m13, float m23, float m33, float m43,
		float m14, float m24, float m34, float m44)
		: m11(m11), m21(m21), m31(m31), m41(m41),
		  m12(m12), m22(m22), m32(m32), m42(m42),
		  m13(m13), m23(m23), m33(m33), m43(m43),
		  m14(m14), m24(m24), m34(m34), m44(m44) {};
	inline float4x4(const float4& c1, const float4& c2, const float4& c3, const float4& c4) : float4x4(
		c1.x, c1.y, c1.z, c1.w,
		c2.x, c2.y, c2.z, c2.w,
		c3.x, c3.y, c3.z, c3.w,
		c4.x, c4.y, c4.z, c4.w) {};
	inline float4x4(float s) : float4x4(
		s, 0, 0, 0,
		0, s, 0, 0,
		0, 0, s, 0,
		0, 0, 0, s) {};
	inline float4x4() : float4x4(1) {};
	inline float4x4(const quaternion& q) : float4x4(1) {
		float qxx = q.x * q.x;
		float qyy = q.y * q.y;
		float qzz = q.z * q.z;
		float qxz = q.x * q.z;
		float qxy = q.x * q.y;
		float qyz = q.y * q.z;
		float qwx = q.w * q.x;
		float qwy = q.w * q.y;
		float qwz = q.w * q.z;
		m11 = 1 - 2 * (qyy + qzz);
		m12 = 2 * (qxy + qwz);
		m13 = 2 * (qxz - qwy);
		m21 = 2 * (qxy - qwz);
		m22 = 1 - 2 * (qxx + qzz);
		m23 = 2 * (qyz + qwx);
		m31 = 2 * (qxz + qwy);
		m32 = 2 * (qyz - qwx);
		m33 = 1 - 2 * (qxx + qyy);
		m44 = 1;
	}
	inline static float4x4 Look(const float3& eye, const float3& fwd, const float3& up) {
		float3 right = normalize(cross(up, fwd));

		float4x4 m(1.f);
		m[0][0] = right.x;
		m[1][0] = right.y;
		m[2][0] = right.z;
		m[0][1] = up.x;
		m[1][1] = up.y;
		m[2][1] = up.z;
		m[0][2] = fwd.x;
		m[1][2] = fwd.y;
		m[2][2] = fwd.z;
		m[3][0] = -dot(right, eye);
		m[3][1] = -dot(up, eye);
		m[3][2] = -dot(fwd, eye);
		return m;
	}
	inline static float4x4 PerspectiveFov(float fovy, float aspect, float near, float far) {
		float tanHalfFovy = tan(fovy / 2);
		float4x4 r(0);
		r[0][0] = 1 / (aspect * tanHalfFovy);
		r[1][1] = 1 / (tanHalfFovy);
		r[2][2] = far / (far - near);
		r[2][3] = 1;
		r[3][2] = -(far * near) / (far - near);
		return r;
	}
	inline static float4x4 PerspectiveBounds(float left, float right, float bottom, float top, float near, float far) {
		float4x4 r(0);
		r[0][0] = (2 * near) / (right - left);
		r[1][1] = (2 * near) / (top - bottom);
		r[2][0] = (right + left) / (right - left);
		r[2][1] = (top + bottom) / (top - bottom);
		r[2][2] = far / (far - near);
		r[2][3] = 1;
		r[3][2] = -(far * near) / (far - near);
		return r;
	}
	inline static float4x4 Orthographic(float left, float right, float bottom, float top, float near, float far) {
		float4x4 r(0);
		r[0][0] = 2 / (right - left);
		r[1][1] = 2 / (top - bottom);
		r[2][2] = 1 / (far - near);
		r[3][0] = -(right + left) / (right - left);
		r[3][1] = -(top + bottom) / (top - bottom);
		r[3][2] = -near / (far - near);
		return r;
	}

	inline float4& operator[](int i) {
		assert(i >= 0 && i < 4);
		return reinterpret_cast<float4*>(this)[i];
	}
	inline float4 operator[](int i) const {
		assert(i >= 0 && i < 4);
		return reinterpret_cast<const float4*>(this)[i];
	}

	inline float4x4 operator=(const float4x4& m) {
		memcpy(this, &m, sizeof(float4x4));
		return *this;
	}

	inline float4x4 operator*(const float& s) const {
		return float4x4(
			m11 * s, m21 * s, m31 * s, m41 * s,
			m12 * s, m22 * s, m32 * s, m42 * s,
			m13 * s, m23 * s, m33 * s, m43 * s,
			m14 * s, m24 * s, m34 * s, m44 * s );
	}
	inline float4x4 operator*=(const float& s) {
			m11 *= s; m21 *= s; m31 *= s; m41 *= s;
			m12 *= s; m22 *= s; m32 *= s; m42 *= s;
			m13 *= s; m23 *= s; m33 *= s; m43 *= s;
			m14 *= s; m24 *= s; m34 *= s; m44 *= s;
		return *this;
	}
	inline float4x4 operator/(const float& s) const { return operator *(1.f / s); }
	inline float4x4 operator/=(const float& s) { return operator *=(1.f / s); }

	inline float4 operator*(const float4& v) const {
		float4 Mov0(v[0]);
		float4 Mov1(v[1]);
		float4 Mul0 = operator[](0) * Mov0;
		float4 Mul1 = operator[](1) * Mov1;
		float4 Add0 = Mul0 + Mul1;
		float4 Mov2(v[2]);
		float4 Mov3(v[3]);
		float4 Mul2 = operator[](2) * Mov2;
		float4 Mul3 = operator[](3) * Mov3;
		float4 Add1 = Mul2 + Mul3;
		float4 Add2 = Add0 + Add1;
		return Add2;
	}
	inline float4x4 operator*(const float4x4& m) const {
		float4 a0 = operator[](0);
		float4 a1 = operator[](1);
		float4 a2 = operator[](2);
		float4 a3 = operator[](3);

		float4 b0 = m[0];
		float4 b1 = m[1];
		float4 b2 = m[2];
		float4 b3 = m[3];

		return float4x4(
			a0 * b0[0] + a1 * b0[1] + a2 * b0[2] + a3 * b0[3],
			a0 * b1[0] + a1 * b1[1] + a2 * b1[2] + a3 * b1[3],
			a0 * b2[0] + a1 * b2[1] + a2 * b2[2] + a3 * b2[3],
			a0 * b3[0] + a1 * b3[1] + a2 * b3[2] + a3 * b3[3] );
	}
	inline float4x4 operator*=(const float4x4& m) {
		*this = operator*(m);
		return *this;
	}
};
#pragma pack(pop)

inline float determinant(const float2x2& m) {
	return m[0][0] * m[1][1] - m[1][0] * m[0][1];
}
inline float determinant(const float3x3& m) {
	return
		+ m[0][0] * (m[1][1] * m[2][2] - m[2][1] * m[1][2])
		- m[1][0] * (m[0][1] * m[2][2] - m[2][1] * m[0][2])
		+ m[2][0] * (m[0][1] * m[1][2] - m[1][1] * m[0][2]);
}
inline float determinant(const float4x4& m) {
	float f00 = m[2][2] * m[3][3] - m[3][2] * m[2][3];
	float f01 = m[2][1] * m[3][3] - m[3][1] * m[2][3];
	float f02 = m[2][1] * m[3][2] - m[3][1] * m[2][2];
	float f03 = m[2][0] * m[3][3] - m[3][0] * m[2][3];
	float f04 = m[2][0] * m[3][2] - m[3][0] * m[2][2];
	float f05 = m[2][0] * m[3][1] - m[3][0] * m[2][1];

	float4 coef(
		+(m[1][1] * f00 - m[1][2] * f01 + m[1][3] * f02),
		-(m[1][0] * f00 - m[1][2] * f03 + m[1][3] * f04),
		+(m[1][0] * f01 - m[1][1] * f03 + m[1][3] * f05),
		-(m[1][0] * f02 - m[1][1] * f04 + m[1][2] * f05));

	return
		m[0][0] * coef[0] + m[0][1] * coef[1] +
		m[0][2] * coef[2] + m[0][3] * coef[3];
}

inline float2x2 inverse(float2x2& m) {
	return float2x2(
		 m.m22, -m.m12,
		-m.m21,  m.m11) / determinant(m);
}
inline float3x3 inverse(float3x3& m) {
	float3x3 result;
	result[0][0] =  m[1][1] * m[2][2] - m[2][1] * m[1][2];
	result[1][0] = -m[1][0] * m[2][2] - m[2][0] * m[1][2];
	result[2][0] =  m[1][0] * m[2][1] - m[2][0] * m[1][1];
	result[0][1] = -m[0][1] * m[2][2] - m[2][1] * m[0][2];
	result[1][1] =  m[0][0] * m[2][2] - m[2][0] * m[0][2];
	result[2][1] = -m[0][0] * m[2][1] - m[2][0] * m[0][1];
	result[0][2] =  m[0][1] * m[1][2] - m[1][1] * m[0][2];
	result[1][2] = -m[0][0] * m[1][2] - m[1][0] * m[0][2];
	result[2][2] =  m[0][0] * m[1][1] - m[1][0] * m[0][1];
	return result / determinant(m);
}
inline float4x4 inverse(float4x4& m) {
	float c00 = m[2][2] * m[3][3] - m[3][2] * m[2][3];
	float c02 = m[1][2] * m[3][3] - m[3][2] * m[1][3];
	float c03 = m[1][2] * m[2][3] - m[2][2] * m[1][3];
	
	float c04 = m[2][1] * m[3][3] - m[3][1] * m[2][3];
	float c06 = m[1][1] * m[3][3] - m[3][1] * m[1][3];
	float c07 = m[1][1] * m[2][3] - m[2][1] * m[1][3];
	
	float c08 = m[2][1] * m[3][2] - m[3][1] * m[2][2];
	float c10 = m[1][1] * m[3][2] - m[3][1] * m[1][2];
	float c11 = m[1][1] * m[2][2] - m[2][1] * m[1][2];
	
	float c12 = m[2][0] * m[3][3] - m[3][0] * m[2][3];
	float c14 = m[1][0] * m[3][3] - m[3][0] * m[1][3];
	float c15 = m[1][0] * m[2][3] - m[2][0] * m[1][3];
	
	float c16 = m[2][0] * m[3][2] - m[3][0] * m[2][2];
	float c18 = m[1][0] * m[3][2] - m[3][0] * m[1][2];
	float c19 = m[1][0] * m[2][2] - m[2][0] * m[1][2];
	
	float c20 = m[2][0] * m[3][1] - m[3][0] * m[2][1];
	float c22 = m[1][0] * m[3][1] - m[3][0] * m[1][1];
	float c23 = m[1][0] * m[2][1] - m[2][0] * m[1][1];

	float4 f0(c00, c00, c02, c03);
	float4 f1(c04, c04, c06, c07);
	float4 f2(c08, c08, c10, c11);
	float4 f3(c12, c12, c14, c15);
	float4 f4(c16, c16, c18, c19);
	float4 f5(c20, c20, c22, c23);

	float4 v0(m[1][0], m[0][0], m[0][0], m[0][0]);
	float4 v1(m[1][1], m[0][1], m[0][1], m[0][1]);
	float4 v2(m[1][2], m[0][2], m[0][2], m[0][2]);
	float4 v3(m[1][3], m[0][3], m[0][3], m[0][3]);

	float4 i0(v1 * f0 - v2 * f1 + v3 * f2);
	float4 i1(v0 * f0 - v2 * f3 + v3 * f4);
	float4 i2(v0 * f1 - v1 * f3 + v3 * f5);
	float4 i3(v0 * f2 - v1 * f4 + v2 * f5);

	float4 sa(1, -1,  1, -1);
	float4 sb(-1, 1, -1, 1);
	float4x4 result(i0 * sa, i1 * sb, i2 * sa, i3 * sb);

	float4 d = m[0] * result[0];
	return result / ((d.x + d.y) + (d.z + d.w));
}

inline float4x4 translate(const float3& v) {
	return float4x4(
		1, 0, 0, v[0],
		0, 1, 0, v[1],
		0, 0, 1, v[2],
		0, 0, 0, 1 );
}
inline float4x4 scale(const float3& v) {
	return float4x4(
		v[0], 0, 0, 0,
		0, v[1], 0, 0,
		0, 0, v[2], 0,
		0, 0, 0, 1 );
}

inline quaternion inverse(const quaternion& q) {
	float s = 1.f / dot(q.xyzw, q.xyzw);
	return quaternion(-q.x, -q.y, -q.z, q.w) / s;
}

namespace std {
	template<>
	struct hash<int2> {
		inline std::size_t operator()(const int2& v) const {
			std::size_t h = 0;
			hash_combine(h, v.x);
			hash_combine(h, v.y);
			return h;
		}
	};
	template<>
	struct hash<int3> {
		inline std::size_t operator()(const int3& v) const {
			std::size_t h = 0;
			hash_combine(h, v.x);
			hash_combine(h, v.y);
			hash_combine(h, v.z);
			return h;
		}
	};
	template<>
	struct hash<int4> {
		inline std::size_t operator()(const int4& v) const {
			std::size_t h = 0;
			hash_combine(h, v.x);
			hash_combine(h, v.y);
			hash_combine(h, v.z);
			hash_combine(h, v.w);
			return h;
		}
	};

	template<>
	struct hash<uint2> {
		inline std::size_t operator()(const uint2& v) const {
			std::size_t h = 0;
			hash_combine(h, v.x);
			hash_combine(h, v.y);
			return h;
		}
	};
	template<>
	struct hash<uint3> {
		inline std::size_t operator()(const uint3& v) const {
			std::size_t h = 0;
			hash_combine(h, v.x);
			hash_combine(h, v.y);
			hash_combine(h, v.z);
			return h;
		}
	};
	template<>
	struct hash<uint4> {
		inline std::size_t operator()(const uint4& v) const {
			std::size_t h = 0;
			hash_combine(h, v.x);
			hash_combine(h, v.y);
			hash_combine(h, v.z);
			hash_combine(h, v.w);
			return h;
		}
	};

	template<>
	struct hash<float2> {
		inline std::size_t operator()(const float2& v) const {
			std::size_t h = 0;
			hash_combine(h, v.x);
			hash_combine(h, v.y);
			return h;
		}
	};
	template<>
	struct hash<float3> {
		inline std::size_t operator()(const float3& v) const {
			std::size_t h = 0;
			hash_combine(h, v.x);
			hash_combine(h, v.y);
			hash_combine(h, v.z);
			return h;
		}
	};
	template<>
	struct hash<float4> {
		inline std::size_t operator()(const float4& v) const {
			std::size_t h = 0;
			hash_combine(h, v.x);
			hash_combine(h, v.y);
			hash_combine(h, v.z);
			hash_combine(h, v.w);
			return h;
		}
	};
}

#pragma region min, max, clamp
inline int32_t min(int32_t a, int32_t b) { return a < b ? a : b; }
inline int32_t max(int32_t a, int32_t b) { return a > b ? a : b; }
inline int32_t clamp(int32_t x, int32_t l, int32_t h) { return max(min(x, l), h); }
inline int32_t abs(int32_t a) { return a < 0 ? -a : a; }
inline int2 vmin(const int2& a, const int2& b) {
	return int2(min(a.x, b.x), min(a.y, b.y));
}
inline int3 vmin(const int3& a, const int3& b) {
	return int3(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z));
}
inline int4 vmin(const int4& a, const int4& b) {
	return int4(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z), min(a.w, b.w));
}
inline int2 vmax(const int2& a, const int2& b) {
	return int2(max(a.x, b.x), max(a.y, b.y));
}
inline int3 vmax(const int3& a, const int3& b) {
	return int3(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z));
}
inline int4 vmax(const int4& a, const int4& b) {
	return int4(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z), max(a.w, b.w));
}
inline int2 vclamp(const int2& v, const int2& l, const int2& h) {
	return int2(clamp(v.x, l.x, h.x), clamp(v.y, l.y, h.y));
}
inline int3 vclamp(const int3& v, const int3& l, const int3& h) {
	return int3(clamp(v.x, l.x, h.x), clamp(v.y, l.y, h.y), clamp(v.z, l.z, h.z));
}
inline int4 vclamp(const int4& v, const int4& l, const int4& h) {
	return int4(clamp(v.x, l.x, h.x), clamp(v.y, l.y, h.y), clamp(v.z, l.z, h.z), clamp(v.w, l.w, h.w));
}
inline int2 vabs(const int2& a) {
	return int2(abs(a.x), abs(a.y));
}
inline int3 vabs(const int3& a) {
	return int3(abs(a.x), abs(a.y), abs(a.z));
}
inline int4 vabs(const int4& a) {
	return int4(abs(a.x), abs(a.y), abs(a.z), abs(a.w));
}

inline uint32_t min(uint32_t a, uint32_t b) { return a < b ? a : b; }
inline uint32_t max(uint32_t a, uint32_t b) { return a > b ? a : b; }
inline uint32_t clamp(uint32_t x, uint32_t l, uint32_t h) { return max(min(x, l), h); }
inline uint2 vmin(const uint2& a, const uint2& b) {
	return uint2(min(a.x, b.x), min(a.y, b.y));
}
inline uint3 vmin(const uint3& a, const uint3& b) {
	return uint3(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z));
}
inline uint4 vmin(const uint4& a, const uint4& b) {
	return uint4(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z), min(a.w, b.w));
}
inline uint2 vmax(const uint2& a, const uint2& b) {
	return uint2(max(a.x, b.x), max(a.y, b.y));
}
inline uint3 vmax(const uint3& a, const uint3& b) {
	return uint3(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z));
}
inline uint4 vmax(const uint4& a, const uint4& b) {
	return uint4(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z), max(a.w, b.w));
}
inline uint2 vclamp(const uint2& v, const uint2& l, const uint2& h) {
	return uint2(clamp(v.x, l.x, h.x), clamp(v.y, l.y, h.y));
}
inline uint3 vclamp(const uint3& v, const uint3& l, const uint3& h) {
	return uint3(clamp(v.x, l.x, h.x), clamp(v.y, l.y, h.y), clamp(v.z, l.z, h.z));
}
inline uint4 vclamp(const uint4& v, const uint4& l, const uint4& h) {
	return uint4(clamp(v.x, l.x, h.x), clamp(v.y, l.y, h.y), clamp(v.z, l.z, h.z), clamp(v.w, l.w, h.w));
}

inline float fclampf(float x, float l, float h) {
	return fminf(fmaxf(x, l), h);
}
inline float2 vmin(const float2& a, const float2& b) {
	return float2(fminf(a.x, b.x), fminf(a.y, b.y));
}
inline float3 vmin(const float3& a, const float3& b) {
	return float3(fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z));
}
inline float4 vmin(const float4& a, const float4& b) {
	return float4(fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z), fminf(a.w, b.w));
}
inline float2 vmax(const float2& a, const float2& b) {
	return float2(fmaxf(a.x, b.x), fmaxf(a.y, b.y));
}
inline float3 vmax(const float3& a, const float3& b) {
	return float3(fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z));
}
inline float4 vmax(const float4& a, const float4& b) {
	return float4(fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z), fmaxf(a.w, b.w));
}
inline float2 vclamp(const float2& v, const float2& l, const float2& h) {
	return float2(fclampf(v.x, l.x, h.x), fclampf(v.y, l.y, h.y));
}
inline float3 vclamp(const float3& v, const float3& l, const float3& h) {
	return float3(fclampf(v.x, l.x, h.x), fclampf(v.y, l.y, h.y), fclampf(v.z, l.z, h.z));
}
inline float4 vclamp(const float4& v, const float4& l, const float4& h) {
	return float4(fclampf(v.x, l.x, h.x), fclampf(v.y, l.y, h.y), fclampf(v.z, l.z, h.z), fclampf(v.w, l.w, h.w));
}
inline float2 vabs(const float2& a) {
	return float2(fabsf(a.x), fabsf(a.y));
}
inline float3 vabs(const float3& a) {
	return float3(fabsf(a.x), fabsf(a.y), fabsf(a.z));
}
inline float4 vabs(const float4& a) {
	return float4(fabsf(a.x), fabsf(a.y), fabsf(a.z), fabsf(a.w));
}
#pragma endregion