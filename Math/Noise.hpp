#pragma once

#include <Math/Math.hpp>

const uint8_t perm[512]{
    253, 233, 174, 96, 107, 220, 49, 195,
    116, 28, 140, 147, 48, 69, 227, 246,
    83, 183, 72, 0, 86, 115, 106, 202,
    252, 203, 152, 122, 176, 241, 40, 79,

    111, 179, 23, 154,  26, 70, 236, 209,
    156, 35, 97, 94, 2, 7, 51, 200,
    25, 221, 213, 66, 248, 141, 163, 170,
    77, 157, 75, 239, 62, 110, 117, 197,

    54, 212, 34, 232, 81, 14, 39, 59,
    112, 168, 46, 240, 158, 235, 128, 71,
    42, 150, 187, 142, 4, 193, 224, 78,
    229, 43, 119, 219, 16, 216, 103, 1,
    
    151, 124, 50, 184, 161, 181, 180, 211,
    133, 68, 135, 196, 6, 60,  192, 38,
    108, 164, 186, 76, 102, 74,  207, 190,
    17, 215, 129, 12, 113, 87,  57, 146,
    
    130, 247, 217, 53, 61, 145, 165, 95,
    52, 182, 208, 24, 64, 126, 159, 33,
    194, 98, 29, 249, 56, 109, 47, 15,
    45, 105, 8, 21, 167, 148, 41, 22,
    
    188, 237, 242, 91, 134, 226, 73, 177,
    198, 214, 9, 178, 36, 238, 231, 255,
    189, 121, 160, 244, 3, 228, 204, 206,
    169, 149, 100, 44, 37, 89,  84, 13,
    
    234, 201, 210, 143, 82, 88, 120, 173,
    199, 225, 20, 172, 123, 230, 18, 67,
    138, 171, 131, 205, 55, 104, 132, 166,
    162, 58, 144, 251, 114, 63, 254, 118,
    
    155, 27, 5, 153, 136, 65, 222, 80,
    11, 90, 137, 125, 185, 85, 127, 92,
    19, 101, 245, 31, 243, 250, 32, 191,
    139, 218, 10, 30, 175, 93, 223, 99,

    219, 71, 217,   82,  202 , 155, 118, 216,
    115, 247, 56,   57,  208  ,139, 22, 7,
    25, 50, 125,  179,   24  , 84, 97, 123,
    212, 172, 176,  145,  238 , 143, 152, 132,

    15, 104, 234,   40,  166  ,180, 128, 203,
    214, 225, 224,  222,   42 , 135, 187, 61,
    250, 16, 119,  131,  171 , 204, 37, 20,
    193, 173, 30,  215,  191 , 227, 167, 96,

    18, 129, 116,  164,   63 , 237, 163, 76,
    244, 68, 178,  185,   48 , 106, 51, 88,
    233, 105, 239,  251 , 102 ,  49, 221, 177,
    136, 72, 161, 110  , 38 ,  80, 0, 103,

    113, 64, 196, 21  , 98,  109, 150, 23,
    43, 91, 242, 78, 142, 52, 2, 69,
    3, 158, 183, 9 ,  35, 108, 200, 157,
    156, 70, 27, 245 , 159 , 235, 67, 77,

    1, 46, 198, 60, 89 , 229, 188, 246,
    13, 58, 210, 168,  65 , 120 ,  34, 253,
    79, 83, 151, 111, 232,  240 , 248, 149,
    141, 59, 170, 165, 53 , 209 , 154, 252,
    
    41, 241, 101, 31, 122,   29 , 189,   26,
    134, 181, 11, 75, 236,  201 , 130,  231,
    195, 90, 12, 213, 160,   17 ,  6,  223,
    32, 162, 19, 133, 39,  10  ,211,   14,
    
    45, 117, 8,  33, 138, 126  ,182,  107,
    66, 174, 140, 197, 148, 190, 220, 127,
    92, 192, 186, 124, 112, 114, 4,  81,
    194, 255, 85 , 199 , 254, 5, 44, 184,
    
    175, 243, 74 , 137, 230, 121, 228,  95,
    62, 93, 146, 205, 144, 153 ,47, 94,
    36, 226, 28, 249, 55, 99, 218, 206,
    54, 147, 100, 73, 86, 207, 169, 87
};

inline float gradf(int hash, float x) {
    int h = hash & 15;
    float grad = 1.0 + (h & 7);   // Gradient value 1.0, 2.0, ..., 8.0
    if ((h & 8) != 0) grad = -grad;         // Set a random sign for the gradient
    return (grad * x);           // Multiply the gradient with the distance
}
inline float gradf(int hash, const float2& p) {
    int h = hash & 7;      // Convert low 3 bits of hash code
    float u = h < 4 ? p.x : p.y;  // into 8 simple gradient directions,
    float v = h < 4 ? p.y : p.x;  // and compute the dot product with (x,y).
    return ((h & 1) != 0 ? -u : u) + ((h & 2) != 0 ? -2 * v : 2 * v);
}
inline float gradf(int hash, const float3& p) {
    int h = hash & 15;     // Convert low 4 bits of hash code into 12 simple
    float u = h < 8 ? p.x : p.y; // gradient directions, and compute dot product.
    float v = h < 4 ? p.y : h == 12 || h == 14 ? p.x : p.z; // Fix repeats at h = 12 to 15
    return ((h & 1) != 0 ? -u : u) + ((h & 2) != 0 ? -v : v);
}

inline double grad(int hash, double x) {
    int h = hash & 15;
    double grad = 1.0 + (h & 7);   // Gradient value 1.0, 2.0, ..., 8.0
    if ((h & 8) != 0) grad = -grad;         // Set a random sign for the gradient
    return (grad * x);           // Multiply the gradient with the distance
}
inline double grad(int hash, const double2& p) {
    int h = hash & 7;      // Convert low 3 bits of hash code
    double u = h < 4 ? p.x : p.y;  // into 8 simple gradient directions,
    double v = h < 4 ? p.y : p.x;  // and compute the dot product with (x,y).
    return ((h & 1) != 0 ? -u : u) + ((h & 2) != 0 ? -2 * v : 2 * v);
}
inline double grad(int hash, const double3& p) {
    int h = hash & 15;     // Convert low 4 bits of hash code into 12 simple
    double u = h < 8 ? p.x : p.y; // gradient directions, and compute dot product.
    double v = h < 4 ? p.y : h == 12 || h == 14 ? p.x : p.z; // Fix repeats at h = 12 to 15
    return ((h & 1) != 0 ? -u : u) + ((h & 2) != 0 ? -v : v);
}

inline float simplexf(float p) {
    int i0 = (int)floor(p);
    int i1 = i0 + 1;
    int x0 = p - i0;
    float2 x(x0, x0 - 1);
    float2 t = 1 - x*x;
    t *= t;
    t *= t;
    float2 g(gradf(perm[i0 & 0xff], x.x), gradf(perm[i1 & 0xff], x.y));
    float2 n = t * n * g;
    return 0.395 * dot(n, 1);
}
inline float simplexf(const float2& p) {
    static const float F2 = 0.36602540378f; // F2 = 0.5*(sqrt(3.0)-1.0)
    static const float G2 = 0.2113248654f;  // G2 = (3.0-sqrt(3.0))/6.0
    float s = dot(p, 1) * F2;
    float2 ps = p + s;
    int2 ij = floor(ps);
    float t = (float)dot((float2)ij, 1.0) * G2;
    float2 p0 = p - (ij - t);
    int2 ij1(0, 1);
    if (p0.x > p0.y) { ij1 = int2(1,0); }

    float2 p1 = p0 - ij1 + G2;
    float2 p2 = p0 - 1 + 2 * G2;

    ij = ij % 256;
    float t0 = .5f - dot(p0 * p0, 1);
    float t1 = .5f - dot(p1 * p1, 1);
    float t2 = .5f - dot(p2 * p2, 1);

    float3 n = 0;
    if (t0 >= 0){
        t0 *= t0;
        n.x = t0 * t0 * gradf(perm[ij.x + perm[ij.y]], p0);
    }
    if (t1 >= 0) {
        t1 *= t1;
        n.y = t1 * t1 * gradf(perm[ij.x + ij1.x + perm[ij.y + ij1.y]], p1);
    }
    if (t2 >= 0) {
        t2 *= t2;
        n.z = t2 * t2 * gradf(perm[ij.x + 1 + perm[ij.y + 1]], p2);
    }
    return 40.0 * dot(n, 1);
}
inline float simplexf(const float3& p) {
    static const float F3 = 1.f / 3.f;
    static const float G3 = 1.f / 6.f;
    float s = dot(p, 1) * F3;
    float3 ps = p + s;
    int3 ijk = floor(ps);

    float3 P0 = ijk - dot(ijk, 1) * G3;
    float3 p0 = p - P0;
    int3 ijk1, ijk2;
    if (p0.x >= p0.y) {
        if (p0.y >= p0.z) { ijk1 = int3(1,0,0); ijk2 = int3(1,1,0); } // X Y Z order
        else if (p0.x >= p0.z) { ijk1 = int3(1,0,0); ijk2 = int3(1,0,1); } // X Z Y order
        else { ijk1 = int3(0,0,1); ijk2 = int3(1,0,1); } // Z X Y order
    } else { // x0<y0
        if (p0.y < p0.z) {ijk1 = int3(0,0,1); ijk2 = int3(0,1,1); } // Z Y X order
        else if (p0.x < p0.z) { ijk1 = int3(0,1,0); ijk2 = int3(0,1,1); } // Y Z X order
        else { ijk1 = int3(0,1,0); ijk2 = int3(1,1,0); } // Y X Z order
    }

    float3 p1 = p0 - ijk1 + G3;
    float3 p2 = p0 - ijk2 + 2*G3;
    float3 p3 = p0 - 1 + 3*G3;

    int3 a = ijk % 256;
    for (int i = 0; i < 3; i++) ijk.v[i] = a.v[i] < 0 ? a.v[i] + 256 : a.v[i];

    // Calculate the contribution from the four corners
    float4 n = 0;
    float4 t = 0.6 - dot(p0, p0);
    if (t.x >= 0){
        t.x *= t.x;
        n.x = t.x * t.x * gradf(perm[ijk.x + perm[ijk.y + perm[ijk.z]]], p0);
    }
    if (t.y >= 0){
        t.y *= t.y;
        n.y = t.y * t.y * gradf(perm[ijk.x + ijk1.x + perm[ijk.y + ijk1.y + perm[ijk.z + ijk1.z]]], p1);
    }
    if (t.z >= 0){
        t.z *= t.z;
        n.z = t.z * t.z * gradf(perm[ijk.x + ijk2.x + perm[ijk.y + ijk2.y + perm[ijk.z + ijk2.z]]], p2);
    }
    if (t.w >= 0) {
        t.w *= t.w;
        n.w = t.w * t.w * gradf(perm[ijk.x + 1 + perm[ijk.y + 1 + perm[ijk.z + 1]]], p3);
    }
    return 32.f * dot(n, 1);
}

inline double simplex(double p) {
    int i0 = (int)floor(p);
    int i1 = i0 + 1;
    int x0 = p - i0;
    double2 x(x0, x0 - 1);
    double2 t = 1 - x*x;
    t *= t;
    t *= t;
    double2 g(grad(perm[i0 & 0xff], x.x), grad(perm[i1 & 0xff], x.y));
    double2 n = t * n * g;
    return 0.395 * dot(n, 1);
}
inline double simplex(const double2& p) {
    static const double F2 = 0.36602540378; // F2 = 0.5*(sqrt(3.0)-1.0)
    static const double G2 = 0.2113248654;  // G2 = (3.0-sqrt(3.0))/6.0
    double s = dot(p, 1) * F2;
    double2 ps = p + s;
    int2 ij = floor(ps);
    double t = (double)dot((double2)ij, 1.0) * G2;
    double2 p0 = p - (ij - t);
    int2 ij1(0, 1);
    if (p0.x > p0.y) { ij1 = int2(1,0); }

    double2 p1 = p0 - ij1 + G2;
    double2 p2 = p0 - 1 + 2 * G2;

    ij = ij % 256;
    double t0 = 0.5 - dot(p0 * p0, 1);
    double t1 = 0.5 - dot(p1 * p1, 1);
    double t2 = 0.5 - dot(p2 * p2, 1);

    double3 n = 0;
    if (t0 >= 0) {
        t0 *= t0;
        n.x = t0 * t0 * grad(perm[ij.x + perm[ij.y]], p0);
    }
    if (t1 >= 0) {
        t1 *= t1;
        n.y = t1 * t1 * grad(perm[ij.x + ij1.x + perm[ij.y + ij1.y]], p1);
    }
    if (t2 >= 0) {
        t2 *= t2;
        n.z = t2 * t2 * grad(perm[ij.x + 1 + perm[ij.y + 1]], p2);
    }
    return 40.0 * dot(n, 1);
}
inline double simplex(const double3& p) {
    static const double F3 = 1.0 / 3.0;// 0.333333333f;
    static const double G3 = 1.0 / 6.0;// 0.166666667f;
    double s = dot(p, 1) * F3;
    double3 ps = p + s;
    int3 ijk = floor(ps);

    double3 P0 = ijk - dot(ijk, 1) * G3;
    double3 p0 = p - P0;
    int3 ijk1, ijk2;
    if (p0.x >= p0.y) {
        if (p0.y >= p0.z) { ijk1 = int3(1,0,0); ijk2 = int3(1,1,0); } // X Y Z order
        else if (p0.x >= p0.z) { ijk1 = int3(1,0,0); ijk2 = int3(1,0,1); } // X Z Y order
        else { ijk1 = int3(0,0,1); ijk2 = int3(1,0,1); } // Z X Y order
    } else { // x0<y0
        if (p0.y < p0.z) {ijk1 = int3(0,0,1); ijk2 = int3(0,1,1); } // Z Y X order
        else if (p0.x < p0.z) { ijk1 = int3(0,1,0); ijk2 = int3(0,1,1); } // Y Z X order
        else { ijk1 = int3(0,1,0); ijk2 = int3(1,1,0); } // Y X Z order
    }

    double3 p1 = p0 - ijk1 + G3;
    double3 p2 = p0 - ijk2 + 2*G3;
    double3 p3 = p0 - 1 + 3*G3;

    int3 a = ijk % 256;
    for (int i = 0; i < 3; i++) ijk.v[i] = a.v[i] < 0 ? a.v[i] + 256 : a.v[i];

    // Calculate the contribution from the four corners
    double4 n = 0;
    double4 t = 0.6 - dot(p0, p0);
    if (t.x > 0) { 
        t.x *= t.x;
        n.x = t.x * t.x * grad(perm[ijk.x + perm[ijk.y + perm[ijk.z]]], p0);
    }
    if (t.y > 0) {
        t.y *= t.y;
        n.y = t.y * t.y * grad(perm[ijk.x + ijk1.x + perm[ijk.y + ijk1.y + perm[ijk.z + ijk1.z]]], p1);
    }
    if (t.z > 0) {
        t.z *= t.z;
        n.z = t.z * t.z * grad(perm[ijk.x + ijk2.x + perm[ijk.y + ijk2.y + perm[ijk.z + ijk2.z]]], p2);
    }
    if (t.w > 0) {
        t.w *= t.w;
        n.w = t.w * t.w * grad(perm[ijk.x + 1 + perm[ijk.y + 1 + perm[ijk.z + 1]]], p3);
    }
    return 32.0 * dot(n, 1);
}

inline float fbmf(float3 p, float lacunarity, uint32_t octaves) {
    float value = 0;
    float signal = 1;
    for (uint32_t i = 0; i < octaves; i++) {
        value += simplexf(p) * signal;
        p *= lacunarity;
        signal /= lacunarity;
    }
    return value;
}
inline float billowf(float3 p, float lacunarity, uint32_t octaves) {
    float value = 0;
    float l = 1;
    for (uint32_t i = 0; i < octaves; ++i) {
        value += (2 * abs(simplex(p)) - 1) * l;
        p *= lacunarity;
        l /= lacunarity;
    }

    value += 0.5;
    return value;
}
inline float ridgedf(float3 p, float lacunarity, uint32_t octaves) {
    float value = 0;
    float l = 1;
    for (uint32_t i = 0; i < octaves; ++i) {
        float signal = abs(simplex(p));
        value += signal * signal * l;
        p *= lacunarity;
        l /= lacunarity;
    }
    return value;
}
inline float multifractalf(float3 p, float lacunarity, uint32_t octaves) {
    float value = 1;
    float l = 1;
    for (int i = 0; i < octaves; i++) {
        value *= simplex(p) * l + 1;
        p *= lacunarity;
        l /= lacunarity;
    }

    return value;
}

inline double fbm(double3 p, float lacunarity, uint32_t octaves) {
    double value = 0;
    double l = 1;
    for (uint32_t i = 0; i < octaves; i++) {
        value += simplex(p) * l;
        p *= lacunarity;
        l /= lacunarity;
    }
    return value;
}
inline double billow(double3 p, float lacunarity, uint32_t octaves) {
    double value = 0;
    double l = 1;
    for (uint32_t i = 0; i < octaves; ++i) {
        value += (2 * abs(simplex(p)) - 1) * l;
        p *= lacunarity;
        l /= lacunarity;
    }

    value += 0.5;
    return value;
}
inline double ridged(double3 p, double lacunarity, uint32_t octaves) {
    double value = 0;
    double l = 1;
    for (uint32_t i = 0; i < octaves; ++i) {
        double signal = abs(simplex(p));
        value += signal * signal * l;
        p *= lacunarity;
        l /= lacunarity;
    }
    return value;
}
inline double multifractal(double3 p, float lacunarity, uint32_t octaves) {
    double value = 1;
    double l = 1;
    for (int i = 0; i < octaves; i++) {
        value *= simplex(p) * l + 1;
        p *= lacunarity;
        l /= lacunarity;
    }

    return value;
}