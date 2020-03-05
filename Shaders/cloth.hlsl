#pragma kernel AddForces
#pragma kernel Integrate
#pragma kernel ComputeNormals0
#pragma kernel ComputeNormals1

#pragma multi_compile INDEX_UINT32

#define FORCE_INT_SCALE 16384

[[vk::binding(0, 0)]] ByteAddressBuffer SourceVertices : register(t0);
[[vk::binding(1, 0)]] RWStructuredBuffer<int4> Forces : register(u0);
[[vk::binding(2, 0)]] RWStructuredBuffer<float4> Velocities : register(u1);
[[vk::binding(3, 0)]] RWByteAddressBuffer Vertices : register(u2);
[[vk::binding(4, 0)]] ByteAddressBuffer Triangles : register(t1);
[[vk::binding(5, 0)]] StructuredBuffer<float4> Spheres : register(t2);
[[vk::binding(6, 0)]] cbuffer ObjectBuffer : register(b0){
    float4x4 ObjectToWorld;
    float4x4 WorldToObject;
}

[[vk::binding(7, 0)]] RWStructuredBuffer<uint> Verticesu : register(u3);

[[vk::push_constant]] cbuffer PushConstants : register(b1) {
    uint VertexSize;
    uint NormalLocation;
    uint TangentLocation;
    uint TexcoordLocation;
    float3 Gravity;
    float Friction;
    float Drag;
    float SpringK;
    float SpringD;
    float DeltaTime;
    uint SphereCount;
}

void SpringForce(float3 p0, float3 p1, float3 v0, float3 v1, float l0, inout float3 f0, inout float3 f1) {
    float3 e = p1 - p0;
    float l = length(e);
    e /= l;

    float3 f = -SpringK * (l0 - l) - SpringD * (dot(e, v0) - dot(e, v1));

    f0 += e * f;
    f1 -= e * f;
}

[numthreads(64, 1, 1)]
void AddForces(uint3 index : SV_DispatchThreadID) {
#ifdef INDEX_UINT32
    uint3 tri = Triangles.Load3(index.x * 12);
#else
    uint2 tt = Triangles.Load2(index.x * 6);
    uint3 tri = uint3(
        tt[0] & 0x0000FFFF,
        (tt[0] & 0xFFFF0000) >> 16,
        tt[1] & 0x0000FFFF);
#endif

    float3 sp0 = mul(ObjectToWorld, float4(asfloat(SourceVertices.Load3(tri[0] * VertexSize)), 1)).xyz;
    float3 sp1 = mul(ObjectToWorld, float4(asfloat(SourceVertices.Load3(tri[1] * VertexSize)), 1)).xyz;
    float3 sp2 = mul(ObjectToWorld, float4(asfloat(SourceVertices.Load3(tri[2] * VertexSize)), 1)).xyz;
    float3 p0  = mul(ObjectToWorld, float4(asfloat(Vertices.Load3(tri[0] * VertexSize)), 1)).xyz;
    float3 p1  = mul(ObjectToWorld, float4(asfloat(Vertices.Load3(tri[1] * VertexSize)), 1)).xyz;
    float3 p2  = mul(ObjectToWorld, float4(asfloat(Vertices.Load3(tri[2] * VertexSize)), 1)).xyz;
    float3 v0 = Velocities[tri[0]].xyz;
    float3 v1 = Velocities[tri[1]].xyz;
    float3 v2 = Velocities[tri[2]].xyz;

    float3 f0 = 0;
    float3 f1 = 0;
    float3 f2 = 0;

    // Springs
    SpringForce(p0, p1, v0, v1, length(sp1 - sp0), f0, f1);
    SpringForce(p0, p2, v0, v2, length(sp2 - sp0), f0, f2);
    SpringForce(p1, p2, v1, v2, length(sp2 - sp1), f1, f2);

    // Drag
    float3 n = cross(p1 - p0, p2 - p0);
    float area = length(n);
    n /= area;
    area *= .5;
    float3 v = (v0 + v1 + v2) / 3.0;
    float3 drag = -.5 * dot(v, v) * Drag * area * dot(n, normalize(v)) * n;
    f0 += drag;
    f1 += drag;
    f2 += drag;


    int3 if0 = int3(f0 * FORCE_INT_SCALE);
    int3 if1 = int3(f1 * FORCE_INT_SCALE);
    int3 if2 = int3(f2 * FORCE_INT_SCALE);

    int rt;
    InterlockedAdd(Forces[tri[0]].x, if0.x, rt);
    InterlockedAdd(Forces[tri[0]].y, if0.y, rt);
    InterlockedAdd(Forces[tri[0]].z, if0.z, rt);
    
    InterlockedAdd(Forces[tri[1]].x, if1.x, rt);
    InterlockedAdd(Forces[tri[1]].y, if1.y, rt);
    InterlockedAdd(Forces[tri[1]].z, if1.z, rt);
    
    InterlockedAdd(Forces[tri[2]].x, if2.x, rt);
    InterlockedAdd(Forces[tri[2]].y, if2.y, rt);
    InterlockedAdd(Forces[tri[2]].z, if2.z, rt);
}

[numthreads(64, 1, 1)]
void Integrate(uint3 index : SV_DispatchThreadID) {
    float3 p = mul(ObjectToWorld, float4(asfloat(Vertices.Load3(index.x * VertexSize)), 1)).xyz;
    float3 v = Velocities[index.x].xyz;

    float3 force = Gravity + float3(Forces[index.x].xyz) / FORCE_INT_SCALE;

    v += force * DeltaTime;
    p += v * DeltaTime;

    for (uint i = 0; i < SphereCount; i++) {
        float3 ds = p - Spheres[i].xyz;
        float r = length(ds);
        ds /= r;

        float3 vds = ds * dot(ds, v); // velocity in direction of dS
        float dr = Spheres[i].w - r;
        if (dr > 0) {
            p += ds * dr;
            v += -(v - vds) * DeltaTime * Friction;
            v -= vds; // velocity rejection
        }
    }
    if (p.y < .01) {
        p.y = .01;

        v.y = 0; // velocity rejection
        v.xz += -v.xz * DeltaTime * Friction;
    }

    Velocities[index.x] = float4(v, 0);
    Vertices.Store3(index.x * VertexSize, asuint(mul(WorldToObject, float4(p, 1)).xyz));
    Vertices.Store3(index.x * VertexSize + NormalLocation, uint3(0,0,0));
    Vertices.Store4(index.x * VertexSize + TangentLocation, uint4(0,0,0,0));
}

[numthreads(64, 1, 1)]
void ComputeNormals0(uint3 index : SV_DispatchThreadID) {
    #ifdef INDEX_UINT3
    uint3 tri = Triangles.Load3(index.x * 12);
    #else
    uint2 t = Triangles.Load2(index.x * 6);
    uint3 tri = uint3(
        t[0] & 0x0000FFFF,
        (t[0] & 0xFFFF0000) >> 16,
        t[1] & 0x0000FFFF);
    #endif

    float3 v0 = asfloat(Verticesu.Load3(tri[0] * VertexSize/4));
    float3 v1 = asfloat(Verticesu.Load3(tri[1] * VertexSize/4));
    float3 v2 = asfloat(Verticesu.Load3(tri[2] * VertexSize/4));

    float2 uv0 = asfloat(Verticesu.Load3(tri[0] * VertexSize/4 + TexcoordLocation/4));
    float2 uv1 = asfloat(Verticesu.Load3(tri[1] * VertexSize/4 + TexcoordLocation/4));
    float2 uv2 = asfloat(Verticesu.Load3(tri[2] * VertexSize/4 + TexcoordLocation/4));

    v0 = mul(ObjectToWorld, float4(v0, 1)).xyz;
    v1 = mul(ObjectToWorld, float4(v1, 1)).xyz;
    v2 = mul(ObjectToWorld, float4(v2, 1)).xyz;

    float3 dv0 = v1 - v0;
    float3 dv1 = v2 - v0;

    float2 duv0 = uv1 - uv0;
    float2 duv1 = uv2 - uv0;

    float3 n = cross(dv0, dv1);

    float r = 1 / (duv0.x * duv1.y - duv0.y * duv1.x);
    float3 tan = (dv0 * duv1.y - dv1 * duv0.y) * r;

    uint3 ni = asuint(int3(n * FORCE_INT_SCALE));
    uint3 ti = asuint(int3(tan * FORCE_INT_SCALE));

    uint rt;
    InterlockedAdd(Verticesu[tri[0] * VertexSize/4 + NormalLocation/4 + 0], ni.x, rt);
    InterlockedAdd(Verticesu[tri[1] * VertexSize/4 + NormalLocation/4 + 0], ni.x, rt);
    InterlockedAdd(Verticesu[tri[2] * VertexSize/4 + NormalLocation/4 + 0], ni.x, rt);
    InterlockedAdd(Verticesu[tri[0] * VertexSize/4 + NormalLocation/4 + 2], ni.y, rt);
    InterlockedAdd(Verticesu[tri[1] * VertexSize/4 + NormalLocation/4 + 2], ni.y, rt);
    InterlockedAdd(Verticesu[tri[2] * VertexSize/4 + NormalLocation/4 + 2], ni.y, rt);
    InterlockedAdd(Verticesu[tri[0] * VertexSize/4 + NormalLocation/4 + 1], ni.z, rt);
    InterlockedAdd(Verticesu[tri[1] * VertexSize/4 + NormalLocation/4 + 1], ni.z, rt);
    InterlockedAdd(Verticesu[tri[2] * VertexSize/4 + NormalLocation/4 + 1], ni.z, rt);

    InterlockedAdd(Verticesu[tri[0] * VertexSize/4 + TangentLocation/4 + 0], ti.x, rt);
    InterlockedAdd(Verticesu[tri[1] * VertexSize/4 + TangentLocation/4 + 0], ti.x, rt);
    InterlockedAdd(Verticesu[tri[2] * VertexSize/4 + TangentLocation/4 + 0], ti.x, rt);
    InterlockedAdd(Verticesu[tri[0] * VertexSize/4 + TangentLocation/4 + 1], ti.y, rt);
    InterlockedAdd(Verticesu[tri[1] * VertexSize/4 + TangentLocation/4 + 1], ti.y, rt);
    InterlockedAdd(Verticesu[tri[2] * VertexSize/4 + TangentLocation/4 + 1], ti.y, rt);
    InterlockedAdd(Verticesu[tri[0] * VertexSize/4 + TangentLocation/4 + 2], ti.z, rt);
    InterlockedAdd(Verticesu[tri[1] * VertexSize/4 + TangentLocation/4 + 2], ti.z, rt);
    InterlockedAdd(Verticesu[tri[2] * VertexSize/4 + TangentLocation/4 + 2], ti.z, rt);
    InterlockedAdd(Verticesu[tri[0] * VertexSize/4 + TangentLocation/4 + 3], 1u, rt);
    InterlockedAdd(Verticesu[tri[1] * VertexSize/4 + TangentLocation/4 + 3], 1u, rt);
    InterlockedAdd(Verticesu[tri[2] * VertexSize/4 + TangentLocation/4 + 3], 1u, rt);
}

[numthreads(64, 1, 1)]
void ComputeNormals1(uint3 index : SV_DispatchThreadID) {
    int3 n = asint(Vertices.Load3(index.x * VertexSize + NormalLocation));
    int4 t = asint(Vertices.Load4(index.x * VertexSize + TangentLocation));

    float3 fn = normalize(float3(n)/FORCE_INT_SCALE);
    float3 ft = (float3(t.xyz) / FORCE_INT_SCALE) / t.w;

    fn = mul(float4(fn, 1), ObjectToWorld).xyz;
    ft = mul(float4(ft, 1), ObjectToWorld).xyz;

    Vertices.Store3(index.x * VertexSize + NormalLocation, asuint(fn));
    Vertices.Store4(index.x * VertexSize + TangentLocation, asuint(float4(ft, 1)));
}