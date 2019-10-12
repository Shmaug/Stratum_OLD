#ifdef __cplusplus
#pragma once
#define uint uint32_t
#endif

#define PER_CAMERA 0
#define PER_MATERIAL 1
#define PER_OBJECT 2

#define CAMERA_BUFFER_BINDING 0
#define OBJECT_BUFFER_BINDING 1
#define BINDING_START 2

#define LIGHT_SUN 0
#define LIGHT_POINT 1
#define LIGHT_SPOT 2

struct CameraBuffer {
	float4x4 ViewProjection;
	float4 Viewport;
	float3 Up;
	float _pad0;
	float3 Right;
	float _pad1;
	float3 Position;
};
struct ObjectBuffer {
	float4x4 ObjectToWorld;
	float4x4 WorldToObject;
};

struct GPULight {
	float3 WorldPosition;
	float InvSqrRange;
	float3 Direction;
	float SpotAngleScale;
	float3 Color;
	float SpotAngleOffset;
	uint Type;
	uint pad0;
	uint pad1;
	uint pad2;
};

#ifdef __cplusplus
#undef uint
#endif