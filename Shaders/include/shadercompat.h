#ifdef __cplusplus
#pragma once
#define uint uint32_t
#endif

#define PER_CAMERA 0
#define PER_MATERIAL 1
#define PER_OBJECT 2

#define CAMERA_BUFFER_BINDING 0
#define OBJECT_BUFFER_BINDING 1
#define LIGHT_BUFFER_BINDING 2
#define SHADOW_ATLAS_BINDING 3
#define SHADOW_BUFFER_BINDING 4
#define BINDING_START 4

#define LIGHT_SUN 0
#define LIGHT_POINT 1
#define LIGHT_SPOT 2

struct ObjectBuffer{
	float4x4 ObjectToWorld;
	float4x4 WorldToObject;
};
struct CameraBuffer {
	float4x4 View;
	float4x4 Projection;
	float4x4 ViewProjection;
	float4x4 InvProjection;
	float4 Viewport;
	float4 ProjParams;
	float3 Up;
	float _pad0;
	float3 Right;
	float _pad1;
	float3 Position;
};

struct GPULight {
	float4 CascadeSplits;
	float3 WorldPosition;
	float InvSqrRange;
	float3 Direction;
	float SpotAngleScale;
	float3 Color;
	float SpotAngleOffset;
	uint Type;
	int ShadowIndex;
	int2 pad;
};

struct ShadowData {
	float4x4 WorldToShadow; // ViewProjection matrix for the shadow render
	float4 ShadowST;
	float3 CameraPosition;
	float InvProj22;
};

#ifdef __cplusplus
#undef uint
#endif