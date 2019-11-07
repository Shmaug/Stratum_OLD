#pragma once

#ifdef WINDOWS
#pragma warning(push)
#pragma warning(disable:26495)
#endif

#include <cassert>
#include <math.h>

#ifdef far
#undef far
#endif
#ifdef near
#undef near
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#ifdef abs
#undef abs
#endif

#define PI 3.1415926535897932384626433832795f

#define rpt2(i) for (int i = 0; i < 2; i++)
#define rpt3(i) for (int i = 0; i < 3; i++)
#define rpt4(i) for (int i = 0; i < 4; i++)

template <class T>
inline void hash_combine(std::size_t& seed, const T& v) {
	std::hash<T> hasher;
	seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

#pragma pack(push)
#pragma pack(1)
struct int2 {
	union {
		int32_t v[2];
		struct { int32_t x, y; };
		struct { int32_t r, g; };
	};

	inline int2(int32_t x, int32_t y) : x(x), y(y) {};
	inline int2(int32_t s) { rpt2(i) v[i] = s;};
	inline int2() : int2(0) {};

	inline int2 operator=(int32_t s) {
		rpt2(i) v[i] = s;
		return *this;
	}
	inline int2 operator=(const int2& s) {
		rpt2(i) v[i] = s.v[i];
		return *this;
	}

	inline int2 operator -() const {
		int2 r;
		rpt2(i) r.v[i] = -v[i];
		return r;
	}
	inline int2 operator -(int32_t s) const {
		int2 r;
		rpt2(i) r.v[i] = v[i] - s;
		return r;
	}
	inline int2 operator -(const int2& s) const {
		int2 r;
		rpt2(i) r.v[i] = v[i] - s.v[i];
		return r;
	}
	inline int2 operator -=(int32_t s) {
		rpt2(i) v[i] -= s;
		return *this;
	}
	inline int2 operator -=(const int2& s) {
		rpt2(i) v[i] -= s.v[i];
		return *this;
	}
	inline friend int2 operator -(int32_t a, const int2& s) {
		int2 r;
		rpt2(i) r.v[i] = a - s.v[i];
		return r;
	}

	inline int2 operator +(int32_t s) const {
		int2 r;
		rpt2(i) r.v[i] = v[i] + s;
		return r;
	}
	inline int2 operator +(const int2& s) const {
		int2 r;
		rpt2(i) r.v[i] = v[i] + s.v[i];
		return r;
	}
	inline int2 operator +=(int32_t s) {
		rpt2(i) v[i] += s;
		return *this;
	}
	inline int2 operator +=(const int2& s) {
		rpt2(i) v[i] += s.v[i];
		return *this;
	}
	inline friend int2 operator +(int32_t a, const int2& s) { return s + a; }

	inline int2 operator *(int32_t s) const {
		int2 r;
		rpt2(i) r.v[i] = v[i] * s;
		return r;
	}
	inline int2 operator *(const int2& s) const {
		int2 r;
		rpt2(i) r.v[i] = v[i] * s.v[i];
		return r;
	}
	inline int2 operator *=(int32_t s) {
		rpt2(i) v[i] *= s;
		return *this;
	}
	inline int2 operator *=(const int2& s) {
		rpt2(i) v[i] *= s.v[i];
		return *this;
	}
	inline friend int2 operator *(int32_t a, const int2& s) { return s * a; }

	inline friend int2 operator /(int32_t a, const int2& s) {
		int2 r;
		rpt2(i) r.v[i] = a / s.v[i];
		return r;
	}
	inline int2 operator /(int32_t s) const {
		int2 r;
		rpt2(i) r.v[i] = v[i] / s;
		return r;
	}
	inline int2 operator /(const int2& s) const { 
		int2 r;
		rpt2(i) r.v[i] = v[i] / s.v[i];
		return r;
	}
	inline int2 operator /=(int32_t s) {
		rpt2(i) v[i] /= s;
		return *this;
	}
	inline int2 operator /=(const int2& s) { 
		rpt2(i) v[i] /= s.v[i];
		return *this;
	}

	inline int32_t& operator[](int i) {
		return v[i];
	}
	inline int32_t operator[](int i) const {
		return v[i];
	}
};
struct int3 {
	union {
		int32_t v[3];
		struct { int32_t x, y, z; };
		struct { int32_t r, g, b; };
		int2 xy;
		int2 rg;
	};

	inline int3(const int2& v, int32_t z) : x(v.x), y(v.y), z(z) {};
	inline int3(int32_t x, int32_t y, int32_t z) : x(x), y(y), z(z) {};
	inline int3(int32_t s) { rpt3(i) v[i] = s;};
	inline int3() : int3(0) {};

	inline int3 operator=(int32_t s) {
		rpt3(i) v[i] = s;
		return *this;
	}
	inline int3 operator=(const int3& s) {
		rpt3(i) v[i] = s.v[i];
		return *this;
	}

	inline int3 operator -() const {
		int3 r;
		rpt3(i) r.v[i] = -v[i];
		return r;
	}
	inline int3 operator -(int32_t s) const {
		int3 r;
		rpt3(i) r.v[i] = v[i] - s;
		return r;
	}
	inline int3 operator -(const int3& s) const {
		int3 r;
		rpt3(i) r.v[i] = v[i] - s.v[i];
		return r;
	}
	inline int3 operator -=(int32_t s) {
		rpt3(i) v[i] -= s;
		return *this;
	}
	inline int3 operator -=(const int3& s) {
		rpt3(i) v[i] -= s.v[i];
		return *this;
	}
	inline friend int3 operator -(int32_t a, const int3& s) {
		int3 r;
		rpt3(i) r.v[i] = a - s.v[i];
		return r;
	}

	inline int3 operator +(int32_t s) const {
		int3 r;
		rpt3(i) r.v[i] = v[i] + s;
		return r;
	}
	inline int3 operator +(const int3& s) const {
		int3 r;
		rpt3(i) r.v[i] = v[i] + s.v[i];
		return r;
	}
	inline int3 operator +=(int32_t s) {
		rpt3(i) v[i] += s;
		return *this;
	}
	inline int3 operator +=(const int3& s) {
		rpt3(i) v[i] += s.v[i];
		return *this;
	}
	inline friend int3 operator +(int32_t a, const int3& s) { return s + a; }

	inline int3 operator *(int32_t s) const {
		int3 r;
		rpt3(i) r.v[i] = v[i] * s;
		return r;
	}
	inline int3 operator *(const int3& s) const {
		int3 r;
		rpt3(i) r.v[i] = v[i] * s.v[i];
		return r;
	}
	inline int3 operator *=(int32_t s) {
		rpt3(i) v[i] *= s;
		return *this;
	}
	inline int3 operator *=(const int3& s) {
		rpt3(i) v[i] *= s.v[i];
		return *this;
	}
	inline friend int3 operator *(int32_t a, const int3& s) { return s * a; }

	inline friend int3 operator /(int32_t a, const int3& s) {
		int3 r;
		rpt3(i) r.v[i] = a / s.v[i];
		return r;
	}
	inline int3 operator /(int32_t s) const {
		int3 r;
		rpt3(i) r.v[i] = v[i] / s;
		return r;
	}
	inline int3 operator /(const int3& s) const { 
		int3 r;
		rpt3(i) r.v[i] = v[i] / s.v[i];
		return r;
	}
	inline int3 operator /=(int32_t s) {
		rpt3(i) v[i] /= s;
		return *this;
	}
	inline int3 operator /=(const int3& s) { 
		rpt3(i) v[i] /= s.v[i];
		return *this;
	}

	inline int32_t& operator[](int i) {
		return v[i];
	}
	inline int32_t operator[](int i) const {
		return v[i];
	}
};
struct int4 {
	union {
		int32_t v[4];
		struct { int32_t x, y, z, w; };
		struct { int32_t r, g, b, a; };
		int3 xyz;
		int3 rgb;
		int2 xy;
		int2 rg;
	};

	inline int4(const int2& v0, const int2& v1) : x(v0.x), y(v0.y), z(v1.x), w(v1.y) {};
	inline int4(const int3& v, int32_t w) : x(v.x), y(v.y), z(v.z), w(w) {};
	inline int4(const int2& v, int32_t z, int32_t w) : x(v.x), y(v.y), z(z), w(w) {};
	inline int4(int32_t x, int32_t y, int32_t z, int32_t w) : x(x), y(y), z(z), w(w) {};
	inline int4(int32_t s) { rpt4(i) v[i] = s;};
	inline int4() : int4(0) {};

	inline int4 operator=(int32_t s) {
		rpt4(i) v[i] = s;
		return *this;
	}
	inline int4 operator=(const int4& s) {
		rpt4(i) v[i] = s.v[i];
		return *this;
	}

	inline int4 operator -() const {
		int4 r;
		rpt4(i) r.v[i] = -v[i];
		return r;
	}
	inline int4 operator -(int32_t s) const {
		int4 r;
		rpt4(i) r.v[i] = v[i] - s;
		return r;
	}
	inline int4 operator -(const int4& s) const {
		int4 r;
		rpt4(i) r.v[i] = v[i] - s.v[i];
		return r;
	}
	inline int4 operator -=(int32_t s) {
		rpt4(i) v[i] -= s;
		return *this;
	}
	inline int4 operator -=(const int4& s) {
		rpt4(i) v[i] -= s.v[i];
		return *this;
	}
	inline friend int4 operator -(int32_t a, const int4& s) {
		int4 r;
		rpt4(i) r.v[i] = a - s.v[i];
		return r;
	}

	inline int4 operator +(int32_t s) const {
		int4 r;
		rpt4(i) r.v[i] = v[i] + s;
		return r;
	}
	inline int4 operator +(const int4& s) const {
		int4 r;
		rpt4(i) r.v[i] = v[i] + s.v[i];
		return r;
	}
	inline int4 operator +=(int32_t s) {
		rpt4(i) v[i] += s;
		return *this;
	}
	inline int4 operator +=(const int4& s) {
		rpt4(i) v[i] += s.v[i];
		return *this;
	}
	inline friend int4 operator +(int32_t a, const int4& s) { return s + a; }

	inline int4 operator *(int32_t s) const {
		int4 r;
		rpt4(i) r.v[i] = v[i] * s;
		return r;
	}
	inline int4 operator *(const int4& s) const {
		int4 r;
		rpt4(i) r.v[i] = v[i] * s.v[i];
		return r;
	}
	inline int4 operator *=(int32_t s) {
		rpt4(i) v[i] *= s;
		return *this;
	}
	inline int4 operator *=(const int4& s) {
		rpt4(i) v[i] *= s.v[i];
		return *this;
	}
	inline friend int4 operator *(int32_t a, const int4& s) { return s * a; }

	inline friend int4 operator /(int32_t a, const int4& s) {
		int4 r;
		rpt4(i) r.v[i] = a / s.v[i];
		return r;
	}
	inline int4 operator /(int32_t s) const {
		int4 r;
		rpt4(i) r.v[i] = v[i] / s;
		return r;
	}
	inline int4 operator /(const int4& s) const { 
		int4 r;
		rpt4(i) r.v[i] = v[i] / s.v[i];
		return r;
	}
	inline int4 operator /=(int32_t s) {
		rpt4(i) v[i] /= s;
		return *this;
	}
	inline int4 operator /=(const int4& s) { 
		rpt4(i) v[i] /= s.v[i];
		return *this;
	}
	inline int32_t& operator[](int i) {
		return v[i];
	}
	inline int32_t operator[](int i) const {
		return v[i];
	}
};

struct uint2 {
	union {
		uint32_t v[2];
		struct { uint32_t x, y; };
		struct { uint32_t r, g; };
	};

	inline uint2(const int2& s) { rpt2(i) v[i] = (uint32_t)s.v[i]; };
	inline uint2(uint32_t x, uint32_t y) : x(x), y(y) {};
	inline uint2(uint32_t s) { rpt2(i) v[i] = s;};
	inline uint2() : uint2(0) {};

	inline uint2 operator=(uint32_t s) {
		rpt2(i) v[i] = s;
		return *this;
	}
	inline uint2 operator=(const uint2& s) {
		rpt2(i) v[i] = s.v[i];
		return *this;
	}

	inline uint2 operator -(uint32_t s) const {
		uint2 r;
		rpt2(i) r.v[i] = v[i] - s;
		return r;
	}
	inline uint2 operator -(const uint2& s) const {
		uint2 r;
		rpt2(i) r.v[i] = v[i] - s.v[i];
		return r;
	}
	inline uint2 operator -=(uint32_t s) {
		rpt2(i) v[i] -= s;
		return *this;
	}
	inline uint2 operator -=(const uint2& s) {
		rpt2(i) v[i] -= s.v[i];
		return *this;
	}
	inline friend uint2 operator -(uint32_t a, const uint2& s) {
		uint2 r;
		rpt2(i) r.v[i] = a - s.v[i];
		return r;
	}

	inline uint2 operator +(uint32_t s) const {
		uint2 r;
		rpt2(i) r.v[i] = v[i] + s;
		return r;
	}
	inline uint2 operator +(const uint2& s) const {
		uint2 r;
		rpt2(i) r.v[i] = v[i] + s.v[i];
		return r;
	}
	inline uint2 operator +=(uint32_t s) {
		rpt2(i) v[i] += s;
		return *this;
	}
	inline uint2 operator +=(const uint2& s) {
		rpt2(i) v[i] += s.v[i];
		return *this;
	}
	inline friend uint2 operator +(uint32_t a, const uint2& s) { return s + a; }

	inline uint2 operator *(uint32_t s) const {
		uint2 r;
		rpt2(i) r.v[i] = v[i] * s;
		return r;
	}
	inline uint2 operator *(const uint2& s) const {
		uint2 r;
		rpt2(i) r.v[i] = v[i] * s.v[i];
		return r;
	}
	inline uint2 operator *=(uint32_t s) {
		rpt2(i) v[i] *= s;
		return *this;
	}
	inline uint2 operator *=(const uint2& s) {
		rpt2(i) v[i] *= s.v[i];
		return *this;
	}
	inline friend uint2 operator *(uint32_t a, const uint2& s) { return s * a; }

	inline friend uint2 operator /(uint32_t a, const uint2& s) {
		uint2 r;
		rpt2(i) r.v[i] = a / s.v[i];
		return r;
	}
	inline uint2 operator /(uint32_t s) const {
		uint2 r;
		rpt2(i) r.v[i] = v[i] / s;
		return r;
	}
	inline uint2 operator /(const uint2& s) const { 
		uint2 r;
		rpt2(i) r.v[i] = v[i] / s.v[i];
		return r;
	}
	inline uint2 operator /=(uint32_t s) {
		rpt2(i) v[i] /= s;
		return *this;
	}
	inline uint2 operator /=(const uint2& s) { 
		rpt2(i) v[i] /= s.v[i];
		return *this;
	}

	inline uint32_t& operator[](int i) {
		return v[i];
	}
	inline uint32_t operator[](int i) const {
		return v[i];
	}
};
struct uint3 {
	union {
		uint32_t v[3];
		struct { uint32_t x, y, z; };
		struct { uint32_t r, g, b; };
		uint2 xy;
		uint2 rg;
	};

	inline uint3(const int3& s) { rpt3(i) v[i] = (uint32_t)s.v[i]; };
	inline uint3(const uint2& v, uint32_t z) : x(v.x), y(v.y), z(z) {};
	inline uint3(uint32_t x, uint32_t y, uint32_t z) : x(x), y(y), z(z) {};
	inline uint3(uint32_t s) { rpt3(i) v[i] = s;};
	inline uint3() : uint3(0) {};

	inline uint3 operator=(uint32_t s) {
		rpt3(i) v[i] = s;
		return *this;
	}
	inline uint3 operator=(const uint3& s) {
		rpt3(i) v[i] = s.v[i];
		return *this;
	}

	inline uint3 operator -(uint32_t s) const {
		uint3 r;
		rpt3(i) r.v[i] = v[i] - s;
		return r;
	}
	inline uint3 operator -(const uint3& s) const {
		uint3 r;
		rpt3(i) r.v[i] = v[i] - s.v[i];
		return r;
	}
	inline uint3 operator -=(uint32_t s) {
		rpt3(i) v[i] -= s;
		return *this;
	}
	inline uint3 operator -=(const uint3& s) {
		rpt3(i) v[i] -= s.v[i];
		return *this;
	}
	inline friend uint3 operator -(uint32_t a, const uint3& s) {
		uint3 r;
		rpt3(i) r.v[i] = a - s.v[i];
		return r;
	}

	inline uint3 operator +(uint32_t s) const {
		uint3 r;
		rpt3(i) r.v[i] = v[i] + s;
		return r;
	}
	inline uint3 operator +(const uint3& s) const {
		uint3 r;
		rpt3(i) r.v[i] = v[i] + s.v[i];
		return r;
	}
	inline uint3 operator +=(uint32_t s) {
		rpt3(i) v[i] += s;
		return *this;
	}
	inline uint3 operator +=(const uint3& s) {
		rpt3(i) v[i] += s.v[i];
		return *this;
	}
	inline friend uint3 operator +(uint32_t a, const uint3& s) { return s + a; }

	inline uint3 operator *(uint32_t s) const {
		uint3 r;
		rpt3(i) r.v[i] = v[i] * s;
		return r;
	}
	inline uint3 operator *(const uint3& s) const {
		uint3 r;
		rpt3(i) r.v[i] = v[i] * s.v[i];
		return r;
	}
	inline uint3 operator *=(uint32_t s) {
		rpt3(i) v[i] *= s;
		return *this;
	}
	inline uint3 operator *=(const uint3& s) {
		rpt3(i) v[i] *= s.v[i];
		return *this;
	}
	inline friend uint3 operator *(uint32_t a, const uint3& s) { return s * a; }

	inline friend uint3 operator /(uint32_t a, const uint3& s) {
		uint3 r;
		rpt3(i) r.v[i] = a / s.v[i];
		return r;
	}
	inline uint3 operator /(uint32_t s) const {
		uint3 r;
		rpt3(i) r.v[i] = v[i] / s;
		return r;
	}
	inline uint3 operator /(const uint3& s) const { 
		uint3 r;
		rpt3(i) r.v[i] = v[i] / s.v[i];
		return r;
	}
	inline uint3 operator /=(uint32_t s) {
		rpt3(i) v[i] /= s;
		return *this;
	}
	inline uint3 operator /=(const uint3& s) { 
		rpt3(i) v[i] /= s.v[i];
		return *this;
	}

	inline uint32_t& operator[](int i) {
		return v[i];
	}
	inline uint32_t operator[](int i) const {
		return v[i];
	}
};
struct uint4 {
	union {
		uint32_t v[4];
		struct { uint32_t x, y, z, w; };
		struct { uint32_t r, g, b, a; };
		uint3 xyz;
		uint3 rgb;
		uint2 xy;
		uint2 rg;
	};

	inline uint4(const int4& s) { rpt4(i) v[i] = (uint32_t)s.v[i]; };
	inline uint4(const uint2& v0, const uint2& v1) : x(v0.x), y(v0.y), z(v1.x), w(v1.y) {};
	inline uint4(const uint3& v, uint32_t w) : x(v.x), y(v.y), z(v.z), w(w) {};
	inline uint4(const uint2& v, uint32_t z, uint32_t w) : x(v.x), y(v.y), z(z), w(w) {};
	inline uint4(uint32_t x, uint32_t y, uint32_t z, uint32_t w) : x(x), y(y), z(z), w(w) {};
	inline uint4(uint32_t s) { rpt4(i) v[i] = s;};
	inline uint4() : uint4(0) {};

	inline uint4 operator=(uint32_t s) {
		rpt4(i) v[i] = s;
		return *this;
	}
	inline uint4 operator=(const uint4& s) {
		rpt4(i) v[i] = s.v[i];
		return *this;
	}

	inline uint4 operator -(uint32_t s) const {
		uint4 r;
		rpt4(i) r.v[i] = v[i] - s;
		return r;
	}
	inline uint4 operator -(const uint4& s) const {
		uint4 r;
		rpt4(i) r.v[i] = v[i] - s.v[i];
		return r;
	}
	inline uint4 operator -=(uint32_t s) {
		rpt4(i) v[i] -= s;
		return *this;
	}
	inline uint4 operator -=(const uint4& s) {
		rpt4(i) v[i] -= s.v[i];
		return *this;
	}
	inline friend uint4 operator -(uint32_t a, const uint4& s) {
		uint4 r;
		rpt4(i) r.v[i] = a - s.v[i];
		return r;
	}

	inline uint4 operator +(uint32_t s) const {
		uint4 r;
		rpt4(i) r.v[i] = v[i] + s;
		return r;
	}
	inline uint4 operator +(const uint4& s) const {
		uint4 r;
		rpt4(i) r.v[i] = v[i] + s.v[i];
		return r;
	}
	inline uint4 operator +=(uint32_t s) {
		rpt4(i) v[i] += s;
		return *this;
	}
	inline uint4 operator +=(const uint4& s) {
		rpt4(i) v[i] += s.v[i];
		return *this;
	}
	inline friend uint4 operator +(uint32_t a, const uint4& s) { return s + a; }

	inline uint4 operator *(uint32_t s) const {
		uint4 r;
		rpt4(i) r.v[i] = v[i] * s;
		return r;
	}
	inline uint4 operator *(const uint4& s) const {
		uint4 r;
		rpt4(i) r.v[i] = v[i] * s.v[i];
		return r;
	}
	inline uint4 operator *=(uint32_t s) {
		rpt4(i) v[i] *= s;
		return *this;
	}
	inline uint4 operator *=(const uint4& s) {
		rpt4(i) v[i] *= s.v[i];
		return *this;
	}
	inline friend uint4 operator *(uint32_t a, const uint4& s) { return s * a; }

	inline friend uint4 operator /(uint32_t a, const uint4& s) {
		uint4 r;
		rpt4(i) r.v[i] = a / s.v[i];
		return r;
	}
	inline uint4 operator /(uint32_t s) const {
		uint4 r;
		rpt4(i) r.v[i] = v[i] / s;
		return r;
	}
	inline uint4 operator /(const uint4& s) const { 
		uint4 r;
		rpt4(i) r.v[i] = v[i] / s.v[i];
		return r;
	}
	inline uint4 operator /=(uint32_t s) {
		rpt4(i) v[i] /= s;
		return *this;
	}
	inline uint4 operator /=(const uint4& s) { 
		rpt4(i) v[i] /= s.v[i];
		return *this;
	}
	inline uint32_t& operator[](int i) {
		return v[i];
	}
	inline uint32_t operator[](int i) const {
		return v[i];
	}
};

struct float2 {
	union {
		float v[2];
		struct { float x, y; };
		struct { float r, g; };
	};

	inline float2(const uint2& s) { rpt2(i) v[i] = (float)s.v[i]; };
	inline float2(const int2& s) { rpt2(i) v[i] = (float)s.v[i]; };
	inline float2(float x, float y) : x(x), y(y) {};
	inline float2(float s) { rpt2(i) v[i] = s;};
	inline float2() : float2(0) {};

	inline float2 operator=(float s) {
		rpt2(i) v[i] = s;
		return *this;
	}
	inline float2 operator=(const float2& s) {
		rpt2(i) v[i] = s.v[i];
		return *this;
	}

	inline float2 operator -() const {
		float2 r;
		rpt2(i) r.v[i] = -v[i];
		return r;
	}
	inline float2 operator -(float s) const {
		float2 r;
		rpt2(i) r.v[i] = v[i] - s;
		return r;
	}
	inline float2 operator -(const float2& s) const {
		float2 r;
		rpt2(i) r.v[i] = v[i] - s.v[i];
		return r;
	}
	inline float2 operator -=(float s) {
		rpt2(i) v[i] -= s;
		return *this;
	}
	inline float2 operator -=(const float2& s) {
		rpt2(i) v[i] -= s.v[i];
		return *this;
	}
	inline friend float2 operator -(float a, const float2& s) {
		float2 r;
		rpt2(i) r.v[i] = a - s.v[i];
		return r;
	}

	inline float2 operator +(float s) const {
		float2 r;
		rpt2(i) r.v[i] = v[i] + s;
		return r;
	}
	inline float2 operator +(const float2& s) const {
		float2 r;
		rpt2(i) r.v[i] = v[i] + s.v[i];
		return r;
	}
	inline float2 operator +=(float s) {
		rpt2(i) v[i] += s;
		return *this;
	}
	inline float2 operator +=(const float2& s) {
		rpt2(i) v[i] += s.v[i];
		return *this;
	}
	inline friend float2 operator +(float a, const float2& s) { return s + a; }

	inline float2 operator *(float s) const {
		float2 r;
		rpt2(i) r.v[i] = v[i] * s;
		return r;
	}
	inline float2 operator *(const float2& s) const {
		float2 r;
		rpt2(i) r.v[i] = v[i] * s.v[i];
		return r;
	}
	inline float2 operator *=(float s) {
		rpt2(i) v[i] *= s;
		return *this;
	}
	inline float2 operator *=(const float2& s) {
		rpt2(i) v[i] *= s.v[i];
		return *this;
	}
	inline friend float2 operator *(float a, const float2& s) { return s * a; }

	inline friend float2 operator /(float a, const float2& s) {
		float2 r;
		rpt2(i) r.v[i] = a / s.v[i];
		return r;
	}
	inline float2 operator /(float s) const { return operator*(1.f / s); }
	inline float2 operator /(const float2& s) const { return operator*(1.f / s); }
	inline float2 operator /=(float s) { return operator*=(1.f / s); }
	inline float2 operator /=(const float2& v) { return operator*=(1.f / v); }

	inline float& operator[](int i) {
		return v[i];
	}
	inline float operator[](int i) const {
		return v[i];
	}
};
struct float3 {
	union {
		float v[3];
		struct { float x, y, z; };
		struct { float r, g, b; };
		float2 xy;
		float2 rg;
	};

	inline float3(const uint3& s) { rpt3(i) v[i] = (float)s.v[i]; };
	inline float3(const int3& s) { rpt3(i) v[i] = (float)s.v[i]; };
	inline float3(const float2& v, float z) : x(v.x), y(v.y), z(z) {};
	inline float3(float x, float y, float z) : x(x), y(y), z(z) {};
	inline float3(float s) { rpt3(i) v[i] = s;};
	inline float3() : float3(0) {};

	inline float3 operator=(float s) {
		rpt3(i) v[i] = s;
		return *this;
	}
	inline float3 operator=(const float3& s) {
		rpt3(i) v[i] = s.v[i];
		return *this;
	}

	inline float3 operator -() const {
		float3 r;
		rpt3(i) r.v[i] = -v[i];
		return r;
	}
	inline float3 operator -(float s) const {
		float3 r;
		rpt3(i) r.v[i] = v[i] - s;
		return r;
	}
	inline float3 operator -(const float3& s) const {
		float3 r;
		rpt3(i) r.v[i] = v[i] - s.v[i];
		return r;
	}
	inline float3 operator -=(float s) {
		rpt3(i) v[i] -= s;
		return *this;
	}
	inline float3 operator -=(const float3& s) {
		rpt3(i) v[i] -= s.v[i];
		return *this;
	}
	inline friend float3 operator -(float a, const float3& s) {
		float3 r;
		rpt3(i) r.v[i] = a - s.v[i];
		return r;
	}

	inline float3 operator +(float s) const {
		float3 r;
		rpt3(i) r.v[i] = v[i] + s;
		return r;
	}
	inline float3 operator +(const float3& s) const {
		float3 r;
		rpt3(i) r.v[i] = v[i] + s.v[i];
		return r;
	}
	inline float3 operator +=(float s) {
		rpt3(i) v[i] += s;
		return *this;
	}
	inline float3 operator +=(const float3& s) {
		rpt3(i) v[i] += s.v[i];
		return *this;
	}
	inline friend float3 operator +(float a, const float3& s) { return s + a; }

	inline float3 operator *(float s) const {
		float3 r;
		rpt3(i) r.v[i] = v[i] * s;
		return r;
	}
	inline float3 operator *(const float3& s) const {
		float3 r;
		rpt3(i) r.v[i] = v[i] * s.v[i];
		return r;
	}
	inline float3 operator *=(float s) {
		rpt3(i) v[i] *= s;
		return *this;
	}
	inline float3 operator *=(const float3& s) {
		rpt3(i) v[i] *= s.v[i];
		return *this;
	}
	inline friend float3 operator *(float a, const float3& s) { return s * a; }

	inline friend float3 operator /(float a, const float3& s) {
		float3 r;
		rpt3(i) r.v[i] = a / s.v[i];
		return r;
	}
	inline float3 operator /(float s) const { return operator*(1.f / s); }
	inline float3 operator /(const float3& s) const { return operator*(1.f / s); }
	inline float3 operator /=(float s) { return operator*=(1.f / s); }
	inline float3 operator /=(const float3& v) { return operator*=(1.f / v); }

	inline float& operator[](int i) {
		return v[i];
	}
	inline float operator[](int i) const {
		return v[i];
	}
};
struct float4 {
	union {
		float v[4];
		struct { float x, y, z, w; };
		struct { float r, g, b, a; };
		float3 xyz;
		float3 rgb;
		float2 xy;
		float2 rg;
	};

	inline float4(const uint4& s) { rpt4(i) v[i] = (float)s.v[i]; };
	inline float4(const int4& s) { rpt4(i) v[i] = (float)s.v[i]; };
	inline float4(const float2& v0, const float2& v1) : x(v0.x), y(v0.y), z(v1.x), w(v1.y) {};
	inline float4(const float3& v, float w) : x(v.x), y(v.y), z(v.z), w(w) {};
	inline float4(const float2& v, float z, float w) : x(v.x), y(v.y), z(z), w(w) {};
	inline float4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {};
	inline float4(float s) { rpt4(i) v[i] = s;};
	inline float4() : float4(0) {};

	inline float4 operator=(float s) {
		rpt4(i) v[i] = s;
		return *this;
	}
	inline float4 operator=(const float4& s) {
		rpt4(i) v[i] = s.v[i];
		return *this;
	}

	inline float4 operator -() const {
		float4 r;
		rpt4(i) r.v[i] = -v[i];
		return r;
	}
	inline float4 operator -(float s) const {
		float4 r;
		rpt4(i) r.v[i] = v[i] - s;
		return r;
	}
	inline float4 operator -(const float4& s) const {
		float4 r;
		rpt4(i) r.v[i] = v[i] - s.v[i];
		return r;
	}
	inline float4 operator -=(float s) {
		rpt4(i) v[i] -= s;
		return *this;
	}
	inline float4 operator -=(const float4& s) {
		rpt4(i) v[i] -= s.v[i];
		return *this;
	}
	inline friend float4 operator -(float a, const float4& s) {
		float4 r;
		rpt4(i) r.v[i] = a - s.v[i];
		return r;
	}

	inline float4 operator +(float s) const {
		float4 r;
		rpt4(i) r.v[i] = v[i] + s;
		return r;
	}
	inline float4 operator +(const float4& s) const {
		float4 r;
		rpt4(i) r.v[i] = v[i] + s.v[i];
		return r;
	}
	inline float4 operator +=(float s) {
		rpt4(i) v[i] += s;
		return *this;
	}
	inline float4 operator +=(const float4& s) {
		rpt4(i) v[i] += s.v[i];
		return *this;
	}
	inline friend float4 operator +(float a, const float4& s) { return s + a; }

	inline float4 operator *(float s) const {
		float4 r;
		rpt4(i) r.v[i] = v[i] * s;
		return r;
	}
	inline float4 operator *(const float4& s) const {
		float4 r;
		rpt4(i) r.v[i] = v[i] * s.v[i];
		return r;
	}
	inline float4 operator *=(float s) {
		rpt4(i) v[i] *= s;
		return *this;
	}
	inline float4 operator *=(const float4& s) {
		rpt4(i) v[i] *= s.v[i];
		return *this;
	}
	inline friend float4 operator *(float a, const float4& s) { return s * a; }

	inline friend float4 operator /(float a, const float4& s) {
		float4 r;
		rpt4(i) r.v[i] = a / s.v[i];
		return r;
	}
	inline float4 operator /(float s) const { return operator*(1.f / s); }
	inline float4 operator /(const float4& s) const { return operator*(1.f / s); }
	inline float4 operator /=(float s) { return operator*=(1.f / s); }
	inline float4 operator /=(const float4& v) { return operator*=(1.f / v); }

	inline float& operator[](int i) {
		return v[i];
	}
	inline float operator[](int i) const {
		return v[i];
	}
};

#pragma pack(pop)

#pragma region trigonometry
inline float2 sin(const float2& s) {
	float2 r;
	rpt2(i) r.v[i] = sinf(s.v[i]);
	return r;
}
inline float3 sin(const float3& s) {
	float3 r;
	rpt3(i) r.v[i] = sinf(s.v[i]);
	return r;
}
inline float4 sin(const float4& s) {
	float4 r;
	rpt4(i) r.v[i] = sinf(s.v[i]);
	return r;
}

inline float2 cos(const float2& s) {
	float2 r;
	rpt2(i) r.v[i] = cosf(s.v[i]);
	return r;
}
inline float3 cos(const float3& s) {
	float3 r;
	rpt3(i) r.v[i] = cosf(s.v[i]);
	return r;
}
inline float4 cos(const float4& s) {
	float4 r;
	rpt4(i) r.v[i] = cosf(s.v[i]);
	return r;
}

inline float2 tan(const float2& s) {
	float2 r;
	rpt2(i) r.v[i] = tanf(s.v[i]);
	return r;
}
inline float3 tan(const float3& s) {
	float3 r;
	rpt3(i) r.v[i] = tanf(s.v[i]);
	return r;
}
inline float4 tan(const float4& s) {
	float4 r;
	rpt4(i) r.v[i] = tanf(s.v[i]);
	return r;
}

inline float radians(float d) {
	return d * (PI / 180.f);
}
inline float2 radians(const float2& d) {
	return d * (PI / 180.f);
}
inline float3 radians(const float3& d) {
	return d * (PI / 180.f);
}
inline float4 radians(const float4& d) {
	return d * (PI / 180.f);
}
inline float degrees(float r) {
	return r * (180.f / PI);
}
inline float2 degrees(const float2& r) {
	return r * (180.f / PI);
}
inline float3 degrees(const float3& r) {
	return r * (180.f / PI);
}
inline float4 degrees(const float4& r) {
	return r * (180.f / PI);
}
#pragma endregion

inline float dot(const float2& a, const float2& b) {
	float r = 0.f;
	rpt2(i) r += a[i] * b[i];
	return r;
}
inline float dot(const float3& a, const float3& b) {
	float r = 0.f;
	rpt3(i) r += a[i] * b[i];
	return r;
}
inline float dot(const float4& a, const float4& b) {
	float r = 0.f;
	rpt4(i) r += a[i] * b[i];
	return r;
}

inline float3 cross(const float3& a, const float3& b) {
	float3 m1(a[1], a[2], a[0]);
	float3 m2(b[2], b[0], b[1]);
	float3 m3(b[1], b[2], b[0]);
	float3 m4(a[2], a[0], a[1]);
	return m1 * m2 - m3 * m4;
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
		float v[4];
		struct { float x, y, z, w; };
		float3 xyz;
		float4 xyzw;
	};

	inline quaternion(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {};
	inline quaternion(float3 euler) {
		euler *= .5f;
		float3 c = cos(euler);
		float3 s = sin(euler);
		
		float4 m0(s.x, c.x, c.x, c.x);
		float4 m1(c.y, s.y, c.y, c.y);
		float4 m2(c.z, c.z, s.z, c.z);

		float4 m3(-c.x, s.x, -s.x, s.x);
		float4 m4(s.y, c.y, s.y, s.y);
		float4 m5(s.z, s.z, c.z, s.z);

		xyzw = m0*m1*m2 + m3*m4*m5;
	};
	inline quaternion(float angle, const float3& axis) {
		angle *= .5f;
		xyz = axis * sinf(angle);
		w = cosf(angle);
	};

	inline quaternion() : quaternion(0, 0, 0, 1) {};

	inline float3 forward() const {
		return 2 * z * xyz + float3(0, 0, w * w - dot(xyz, xyz)) + 2 * w * float3(y, -x, 0);
	}

	inline quaternion operator =(const quaternion& q) {
		rpt4(i) v[i] = q.v[i];
		return *this;
	}

	inline quaternion operator +(const quaternion& s) const {
		quaternion r;
		rpt4(i) r.v[i] = v[i] + s.v[i];
		return r;
	}
	inline quaternion operator -(const quaternion& s) const {
		quaternion r;
		rpt4(i) r.v[i] = v[i] - s.v[i];
		return r;
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
		quaternion r;
		rpt4(i) r.v[i] = v[i] * s;
		return r;
	}
	inline quaternion operator *=(float s) {
		rpt4(i) v[i] *= s;
		return *this;
	}

	inline quaternion operator /(float s) const { return operator*(1.f / s); }
	inline quaternion operator /=(float s) { return operator*=(1.f / s); }

	inline float3 operator *(const float3& vec) const {
		return 2 * dot(xyz, vec) * xyz + (w * w - dot(xyz, xyz)) * vec + 2 * w * cross(xyz, vec);
	}
};

// Column-major 4x4 matrix
struct float4x4 {
	union {
		float4 v[4];
		struct { float4 c1, c2, c3, c4; };
	};

	inline float4x4(
		float m11, float m21, float m31, float m41,
		float m12, float m22, float m32, float m42,
		float m13, float m23, float m33, float m43,
		float m14, float m24, float m34, float m44)
		: c1(float4(m11,m12,m13,m14)), c2(float4(m21,m22,m23,m24)), c3(float4(m31,m32,m33,m34)), c4(float4(m41,m42,m43,m44)) {};
	inline float4x4(const float4& c1, const float4& c2, const float4& c3, const float4& c4)
		: c1(c1), c2(c2), c3(c3), c4(c4) {};
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
		v[0][0] = 1 - 2 * (qyy + qzz);
		v[0][1] = 2 * (qxy + qwz);
		v[0][2] = 2 * (qxz - qwy);
		v[1][0] = 2 * (qxy - qwz);
		v[1][1] = 1 - 2 * (qxx + qzz);
		v[1][2] = 2 * (qyz + qwx);
		v[2][0] = 2 * (qxz + qwy);
		v[2][1] = 2 * (qyz - qwx);
		v[2][2] = 1 - 2 * (qxx + qyy);
	}

	inline static float4x4 Look(const float3& p, const float3& fwd, const float3& up) {
		float3 f[3];
		f[0] = normalize(cross(up, fwd));
		f[1] = cross(fwd, f[0]);
		f[2] = fwd;
		float4x4 r(1);
		rpt3(i) r.v[0][i] = f[0].v[i];
		rpt3(i) r.v[1][i] = f[1].v[i];
		rpt3(i) r.v[2][i] = f[2].v[i];
		rpt3(i) r.v[i][3] = -dot(f[i], p);
		return r;
	}
	inline static float4x4 PerspectiveFov(float fovy, float aspect, float near, float far) {
		float df = 1 / (far - near);
		float sy = 1 / tan(fovy / 2);
		float4x4 r(0);
		r[0][0] = sy / aspect;
		r[1][1] = -sy;
		r[2][2] = far * df;
		r[2][3] = -far * near * df;
		r[3][2] = 1;
		return r;
	}
	inline static float4x4 Perspective(float width, float height, float near, float far) {
		float df = 1 / (far - near);
		float4x4 r(0);
		r[0][0] = 2 * near / width;
		r[1][1] = -2 * near / height;
		r[2][2] = far * df;
		r[2][3] = far * near * df;
		r[3][2] = 1;
		return r;
	}
	inline static float4x4 Orthographic(float width, float height, float near, float far) {
		float df = 1 / (far - near);
		float4x4 r(1);
		r[0][0] = 2 / width;
		r[1][1] = 2 / height;
		r[2][2] = df;
		r[2][3] = -near * df;
		return r;
	}

	inline static float4x4 Translate(const float3& p) {
		float4x4 r(1);
		r.v[3].xyz = p;
		return r;
	}
	inline static float4x4 Scale(const float3& p) {
		return float4x4(float4(p[0], 0, 0, 0), float4(0, p[1], 0, 0), float4(0, 0, p[2], 0), float4(0, 0, 0, 1));
	}

	inline float4& operator[](int i) {
		return v[i];
	}
	inline float4 operator[](int i) const {
		return v[i];
	}

	inline float4x4 operator=(const float4x4& m) {
		rpt4(i) v[i] = m.v[i];
		return *this;
	}

	inline float4x4 operator*(const float& s) const {
		float4x4 r;
		rpt4(i) r.v[i] = v[i] * s;
		return r;
	}
	inline float4x4 operator*=(const float& s) {
		rpt4(i) v[i] += s;
		return *this;
	}
	inline float4x4 operator/(const float& s) const { return operator *(1.f / s); }
	inline float4x4 operator/=(const float& s) { return operator *=(1.f / s); }

	inline float4 operator*(const float4& s) const {
		float4 r = 0;
		rpt4(i) r += v[i] * s.v[i];
		return r;
	}
	inline float4x4 operator*(const float4x4& m) const {
		float4x4 r;
		rpt4(i) r.v[i] = (*this) * m.v[i];
		return r;
	}
	inline float4x4 operator*=(const float4x4& m) {
		*this = operator*(m);
		return *this;
	}
};
#pragma pack(pop)

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
inline float4x4 inverse(const float4x4& m) {
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

	float4 sa(+1, -1, +1, -1);
	float4 sb(-1, +1, -1, +1);
	float4x4 inv(i0 * sa, i1 * sb, i2 * sa, i3 * sb);

	float4 r0(inv[0][0], inv[1][0], inv[2][0], inv[3][0]);

	float4 d0(m[0] * r0);
	return inv / ((d0.x + d0.y) + (d0.z + d0.w));
}
inline quaternion inverse(const quaternion& q) {
	float s = 1.f / dot(q.xyzw, q.xyzw);
	return quaternion(-q.x, -q.y, -q.z, q.w) * s;
}

inline quaternion normalize(const quaternion& q){
	return q / length(q.xyzw);
}

inline float4x4 transpose(const float4x4& m) {
	return float4x4(
		m.c1[0], m.c1[1], m.c1[2], m.c1[3],
		m.c2[0], m.c2[1], m.c2[2], m.c2[3],
		m.c3[0], m.c3[1], m.c3[2], m.c3[3],
		m.c4[0], m.c4[1], m.c4[2], m.c4[3] );
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
inline int32_t clamp(int32_t x, int32_t l, int32_t h) { return min(max(x, l), h); }

inline int2 min(const int2& a, const int2& b) {
	int2 r;
	rpt2(i) r.v[i] = min(a.v[i], b.v[i]);
	return r;
}
inline int2 max(const int2& a, const int2& b) {
	int2 r;
	rpt2(i) r.v[i] = max(a.v[i], b.v[i]);
	return r;
}
inline int2 clamp(const int2& a, const int2& l, const int2& h) {
	int2 r;
	rpt2(i) r.v[i] = clamp(a.v[i], l.v[i], h.v[i]);
	return r;
}
inline int2 abs(const int2& a) {
	int2 r;
	rpt2(i) r.v[i] = abs(a.v[i]);
	return r;
}

inline int3 min(const int3& a, const int3& b) {
	int3 r;
	rpt3(i) r.v[i] = min(a.v[i], b.v[i]);
	return r;
}
inline int3 max(const int3& a, const int3& b) {
	int3 r;
	rpt3(i) r.v[i] = max(a.v[i], b.v[i]);
	return r;
}
inline int3 clamp(const int3& a, const int3& l, const int3& h) {
	int3 r;
	rpt3(i) r.v[i] = clamp(a.v[i], l.v[i], h.v[i]);
	return r;
}
inline int3 abs(const int3& a) {
	int3 r;
	rpt3(i) r.v[i] = abs(a.v[i]);
	return r;
}

inline int4 min(const int4& a, const int4& b) {
	int4 r;
	rpt4(i) r.v[i] = min(a.v[i], b.v[i]);
	return r;
}
inline int4 max(const int4& a, const int4& b) {
	int4 r;
	rpt4(i) r.v[i] = max(a.v[i], b.v[i]);
	return r;
}
inline int4 clamp(const int4& a, const int4& l, const int4& h) {
	int4 r;
	rpt4(i) r.v[i] = clamp(a.v[i], l.v[i], h.v[i]);
	return r;
}
inline int4 abs(const int4& a) {
	int4 r;
	rpt4(i) r.v[i] = abs(a.v[i]);
	return r;
}

inline uint32_t min(uint32_t a, uint32_t b) { return a < b ? a : b; }
inline uint32_t max(uint32_t a, uint32_t b) { return a > b ? a : b; }
inline uint32_t clamp(uint32_t x, uint32_t l, uint32_t h) { return min(max(x, l), h); }

inline uint2 min(const uint2& a, const uint2& b) {
	uint2 r;
	rpt2(i) r.v[i] = min(a.v[i], b.v[i]);
	return r;
}
inline uint2 max(const uint2& a, const uint2& b) {
	uint2 r;
	rpt2(i) r.v[i] = max(a.v[i], b.v[i]);
	return r;
}
inline uint2 clamp(const uint2& a, const uint2& l, const uint2& h) {
	uint2 r;
	rpt2(i) r.v[i] = clamp(a.v[i], l.v[i], h.v[i]);
	return r;
}

inline uint3 min(const uint3& a, const uint3& b) {
	uint3 r;
	rpt3(i) r.v[i] = min(a.v[i], b.v[i]);
	return r;
}
inline uint3 max(const uint3& a, const uint3& b) {
	uint3 r;
	rpt3(i) r.v[i] = max(a.v[i], b.v[i]);
	return r;
}
inline uint3 clamp(const uint3& a, const uint3& l, const uint3& h) {
	uint3 r;
	rpt3(i) r.v[i] = clamp(a.v[i], l.v[i], h.v[i]);
	return r;
}

inline uint4 min(const uint4& a, const uint4& b) {
	uint4 r;
	rpt4(i) r.v[i] = min(a.v[i], b.v[i]);
	return r;
}
inline uint4 max(const uint4& a, const uint4& b) {
	uint4 r;
	rpt4(i) r.v[i] = max(a.v[i], b.v[i]);
	return r;
}
inline uint4 clamp(const uint4& a, const uint4& l, const uint4& h) {
	uint4 r;
	rpt4(i) r.v[i] = clamp(a.v[i], l.v[i], h.v[i]);
	return r;
}

inline float clamp(float x, float l, float h) {
	return fminf(fmaxf(x, l), h);
}

inline float2 min(const float2& a, const float2& b) {
	float2 r;
	rpt2(i) r.v[i] = fminf(a.v[i], b.v[i]);
	return r;
}
inline float2 max(const float2& a, const float2& b) {
	float2 r;
	rpt2(i) r.v[i] = fmaxf(a.v[i], b.v[i]);
	return r;
}
inline float2 clamp(const float2& a, const float2& l, const float2& h) {
	float2 r;
	rpt2(i) r.v[i] = clamp(a.v[i], l.v[i], h.v[i]);
	return r;
}
inline float2 abs(const float2& a) {
	float2 r;
	rpt2(i) r.v[i] = fabs(a.v[i]);
	return r;
}

inline float3 min(const float3& a, const float3& b) {
	float3 r;
	rpt3(i) r.v[i] = fminf(a.v[i], b.v[i]);
	return r;
}
inline float3 max(const float3& a, const float3& b) {
	float3 r;
	rpt3(i) r.v[i] = fmaxf(a.v[i], b.v[i]);
	return r;
}
inline float3 clamp(const float3& a, const float3& l, const float3& h) {
	float3 r;
	rpt3(i) r.v[i] = clamp(a.v[i], l.v[i], h.v[i]);
	return r;
}
inline float3 abs(const float3& a) {
	float3 r;
	rpt3(i) r.v[i] = fabs(a.v[i]);
	return r;
}


inline float4 min(const float4& a, const float4& b) {
	float4 r;
	rpt4(i) r.v[i] = fminf(a.v[i], b.v[i]);
	return r;
}
inline float4 max(const float4& a, const float4& b) {
	float4 r;
	rpt4(i) r.v[i] = fmaxf(a.v[i], b.v[i]);
	return r;
}
inline float4 clamp(const float4& a, const float4& l, const float4& h) {
	float4 r;
	rpt4(i) r.v[i] = clamp(a.v[i], l.v[i], h.v[i]);
	return r;
}
inline float4 abs(const float4& a) {
	float4 r;
	rpt4(i) r.v[i] = fabs(a.v[i]);
	return r;
}
#pragma endregion

inline float lerp(float a, float b, float t) {
	return a + (b - a) * t;
}
inline float2 lerp(const float2& a, const float2& b, float t) {
	float2 ba = b - a;
	ba *= t;
	return a + ba;
}
inline float3 lerp(const float3& a, const float3& b, float t) {
	float3 ba = b - a;
	ba *= t;
	return a + ba;
}
inline float4 lerp(const float4& a, const float4& b, float t) {
	float4 ba = b - a;
	ba *= t;
	return a + ba;
}
inline quaternion lerp(const quaternion& a, const quaternion& b, float t) {
	quaternion ba = b - a;
	ba *= t;
	return a + ba;
}
inline quaternion slerp(const quaternion& v0, quaternion v1, float t){
	float d = dot(v0.xyzw, v1.xyzw);
	if (d < 0) {
		v1.xyzw = -v1.xyzw;
		d = -d;
	}

    if (d > .9995f) return normalize(lerp(v0, v1, t));

    float theta_0 = acosf(d);
    float theta = theta_0*t;
    float sin_theta = sin(theta);
    float sin_theta_0 = sin(theta_0);

    float s0 = cosf(theta) - d * sin_theta / sin_theta_0;
    float s1 = sin_theta / sin_theta_0;
	return v0 * s0 + v1 * s1;
}

#undef rpt2
#undef rpt3
#undef rpt4

#ifdef WINDOWS
#pragma warning(pop)
#endif