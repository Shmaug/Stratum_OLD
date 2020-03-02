#pragma kernel AddForces
#pragma kernel Integrate

struct ClothVertex {
    float4 Position;     // object-space
    float4 Velocity; // object-space
};

[[vk::binding(0, 0)]] RWStructuredBuffer<ClothVertex> Vertices : register(u0);

[[vk::push_constant]] cbuffer PushConstants : register(b0) {
    float4x4 ObjectToWorld;
    float4x4 WorldToObject;
    float3 ExternalForce;
    float ClothSpringDistance;
    float Drag;
    float Stiffness;
    float DeltaTime;
    uint ClothResolution;
    uint2 PinIndex;
    float3 PinPos;
}

float3 ResolveSpring(float3 p0, float3 p1) {
    float3 dp = p1 - p0;
    float d = length(dp);
    float3 error = dp - (dp / d) * ClothSpringDistance;
    return Stiffness*error;
}

[numthreads(8, 8, 1)]
void AddForces(uint3 index : SV_DispatchThreadID) {
    ClothVertex v = Vertices[index.y * ClothResolution + index.x];
    float3 pos = mul(ObjectToWorld, float4(v.Position.xyz, 1)).xyz;
    float3 vel = mul((float3x3)ObjectToWorld, v.Velocity.xyz);

    float3 force = ExternalForce;
    force += -vel * length(vel) * Drag;

    if (index.x > 0)                    force += ResolveSpring(pos, mul(ObjectToWorld, float4(Vertices[index.y * ClothResolution + (index.x - 1)].Position.xyz, 1)).xyz);
    if (index.x < ClothResolution - 1)  force += ResolveSpring(pos, mul(ObjectToWorld, float4(Vertices[index.y * ClothResolution + (index.x + 1)].Position.xyz, 1)).xyz);
    if (index.y > 0)                    force += ResolveSpring(pos, mul(ObjectToWorld, float4(Vertices[(index.y - 1) * ClothResolution + index.x].Position.xyz, 1)).xyz);
    if (index.y < ClothResolution - 1)  force += ResolveSpring(pos, mul(ObjectToWorld, float4(Vertices[(index.y + 1) * ClothResolution + index.x].Position.xyz, 1)).xyz);

    vel += force * DeltaTime;

    v.Velocity.xyz = mul((float3x3)WorldToObject, float4(vel, 1)).xyz;
    Vertices[index.y * ClothResolution + index.x] = v;
}

[numthreads(8, 8, 1)]
void Integrate(uint3 index : SV_DispatchThreadID) {
    ClothVertex v = Vertices[index.y * ClothResolution + index.x];

    float3 pos = mul(ObjectToWorld, float4(v.Position.xyz, 1)).xyz;
    float3 vel = mul((float3x3)ObjectToWorld, v.Velocity.xyz);

    pos += vel * DeltaTime;
    pos = (index.x == PinIndex.x && index.y == PinIndex.y) ? PinPos : pos; // pin

    if (pos.y < 0.01) {
        pos.y = 0.01;
        vel.y = 0;
    }

    v.Position.xyz = mul(WorldToObject, float4(pos, 1)).xyz;
    v.Velocity.xyz = mul((float3x3)WorldToObject, vel).xyz;
    Vertices[index.y * ClothResolution + index.x] = v;
}