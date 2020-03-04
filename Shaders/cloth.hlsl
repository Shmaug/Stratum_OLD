#pragma kernel AddForces
#pragma kernel Integrate

#pragma multi_compile INDEX_UINT32

#define MAX_FORCE 65535
#define MAX_FORCE_INT 0xFFFFFF

[[vk::binding(0, 0)]] ByteAddressBuffer SourceVertices : register(t0);
[[vk::binding(1, 0)]] RWStructuredBuffer<int4> Forces : register(u0);
[[vk::binding(2, 0)]] RWByteAddressBuffer Vertices : register(u1);
[[vk::binding(3, 0)]] RWByteAddressBuffer LastVertices : register(u2);
[[vk::binding(4, 0)]] ByteAddressBuffer Triangles : register(t1);

[[vk::binding(5, 0)]] cbuffer ObjectBuffer : register(b0){
    float4x4 ObjectToWorld;
    float4x4 WorldToObject;
}

[[vk::push_constant]] cbuffer PushConstants : register(b1) {
    uint VertexSize;
    float Drag;
    float Stiffness;
    float DeltaTime;
}

float3 SpringForce(float3 delta, float3 len) {
    return delta * (len / length(delta) - 1);
}

[numthreads(64, 1, 1)]
void AddForces(uint3 index : SV_DispatchThreadID) {
#ifdef INDEX_UINT3
    uint3 tri = Triangles.Load3(index.x * 12);
#else
    uint2 t = Triangles.Load2(index.x * 6);
    uint3 tri = uint3(
        t[0] & 0x0000FFFF,
        (t[0] & 0xFFFF0000) << 16,
        t[1] & 0x0000FFFF);
#endif

    float3 sv0 = asfloat(SourceVertices.Load3(tri[0] * VertexSize));
    float3 sv1 = asfloat(SourceVertices.Load3(tri[1] * VertexSize));
    float3 sv2 = asfloat(SourceVertices.Load3(tri[2] * VertexSize));
    float3 v0  = asfloat(Vertices.Load3(tri[0] * VertexSize));
    float3 v1 = asfloat(Vertices.Load3(tri[1] * VertexSize));
    float3 v2 = asfloat(Vertices.Load3(tri[2] * VertexSize));
    float3 lv0 = asfloat(LastVertices.Load3(tri[0] * VertexSize));
    float3 lv1 = asfloat(LastVertices.Load3(tri[1] * VertexSize));
    float3 lv2 = asfloat(LastVertices.Load3(tri[2] * VertexSize));

    sv0 = mul(ObjectToWorld, float4(sv0, 1)).xyz;
    sv1 = mul(ObjectToWorld, float4(sv1, 1)).xyz;
    sv2 = mul(ObjectToWorld, float4(sv2, 1)).xyz;
    v0 = mul(ObjectToWorld, float4(v0, 1)).xyz;
    v1 = mul(ObjectToWorld, float4(v1, 1)).xyz;
    v2 = mul(ObjectToWorld, float4(v2, 1)).xyz;
    lv0 = mul(ObjectToWorld, float4(lv0, 1)).xyz;
    lv1 = mul(ObjectToWorld, float4(lv1, 1)).xyz;
    lv2 = mul(ObjectToWorld, float4(lv2, 1)).xyz;
    
    float3 f0 = 0;
    float3 f1 = 0;
    float3 f2 = 0;

    // Spring constraints
    float3 sd0 = sv1 - sv0;
    float3 sd1 = sv2 - sv0;
    float3 sd2 = sv2 - sv1;
    float3 d0 = v1 - v0;
    float3 d1 = v2 - v0;
    float3 d2 = v2 - v1;
    float3 s0 = Stiffness * SpringForce(d0, length(sd0));
    float3 s1 = Stiffness * SpringForce(d1, length(sd1));
    float3 s2 = Stiffness * SpringForce(d2, length(sd2));

    f0 += s0 + s1;
    f1 += -s0 + s1;
    f2 += -s1 - s2;

    // Drag
    float3 n = cross(d0, d1);
    float3 v = ((v0 + v1 + v2) - (lv0 + lv1 + lv2)) / 3.0; // velocity of centroid
    float3 nv = dot(n, v);
    float3 drag = Drag * n * nv * nv;
    f0 += drag;
    f1 += drag;
    f2 += drag;

    int3 if0 = int3(clamp(f0 / MAX_FORCE, -1, 1) * MAX_FORCE_INT);
    int3 if1 = int3(clamp(f1 / MAX_FORCE, -1, 1) * MAX_FORCE_INT);
    int3 if2 = int3(clamp(f2 / MAX_FORCE, -1, 1) * MAX_FORCE_INT);

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
    float3 v = asfloat(Vertices.Load3(index.x * VertexSize));
    float3 lv = asfloat(LastVertices.Load3(index.x * VertexSize));
    v = mul(ObjectToWorld, float4(v, 1)).xyz;
    lv = mul(ObjectToWorld, float4(lv, 1)).xyz;

    float3 force = MAX_FORCE * ((float3(Forces[index.x].xyz)) / MAX_FORCE_INT);

    v = 2 * v - lv + force * DeltaTime * DeltaTime;

    v = mul(WorldToObject, float4(v, 1)).xyz;
    Vertices.Store3(index.x * VertexSize, asuint(v));
}