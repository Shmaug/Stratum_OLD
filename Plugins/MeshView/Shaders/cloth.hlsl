#pragma kernel Simulate

struct ClothVertex {
    float4 Position;
    float4 Acceleration;
}

[[vk::binding(0, 0)]] RWStructuredBuffer<ClothVertex> Vertices : register(u0);

[[vk::push_constant]] cbuffer PushConstants : register(b0){
    float DeltaTime;
}

[numthreads(64, 1, 1)]
void Simulate(uint3 index : SV_DispatchThreadID) {
    ClothVertex v = Vertices[index.x];

    float3 accel = float3(0, -9.8, 0);
    
    float3 vel = (v.Position - pos) / DeltaTime + .5 * (accel + v.Acceleration) * DeltaTime;
    
    v.Position += vel * DeltaTime + .5 * accel * DeltaTime * DeltaTime;
    v.Acceleration = accel;

    Vertices[index.x].Position = v;
}