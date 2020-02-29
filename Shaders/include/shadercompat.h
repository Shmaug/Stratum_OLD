#ifdef __cplusplus
#pragma once
#define uint uint32_t
#endif

#define PER_CAMERA 0
#define PER_MATERIAL 1
#define PER_OBJECT 2

#define CAMERA_BUFFER_BINDING 0
#define INSTANCE_BUFFER_BINDING 1
#define LIGHT_BUFFER_BINDING 2
#define SHADOW_ATLAS_BINDING 3
#define SHADOW_BUFFER_BINDING 4
#define BINDING_START 5

#define LIGHT_SUN 0
#define LIGHT_POINT 1
#define LIGHT_SPOT 2

#define STRATUM_PUSH_CONSTANTS \
uint StereoEye; \
float4 StereoClipTransform; \
float3 AmbientLight; \
uint LightCount; \
float2 ShadowTexelSize;

#define STRATUM_MATRIX_V Camera.View[StereoEye]
#define STRATUM_MATRIX_P Camera.Projection[StereoEye]
#define STRATUM_MATRIX_VP Camera.ViewProjection[StereoEye]
#define StratumOffsetClipPosStereo(clipPos) clipPos.xy = clipPos.xy * StereoClipTransform.xy + StereoClipTransform.zw

struct InstanceBuffer {
	float4x4 ObjectToWorld;
	float4x4 WorldToObject;
};

struct CameraBuffer {
	float4x4 View[2];
	float4x4 Projection[2];
	float4x4 ViewProjection[2];
	float4x4 InvProjection[2];
	float4 Viewport;
	float4 ProjParams;
	float3 Up;
	float _pad0;
	float3 Right;
	float _pad1;
	float3 Position;
};

struct ScatteringParameters {
	float4 MoonRotation;
	float MoonSize;

	float3 IncomingLight;

	float3 SunDir;

	float PlanetRadius;
	float AtmosphereHeight;

	float SunIntensity;
	float MieG;

	float3 ScatteringR;
	float3 ScatteringM;

	float4 StarRotation;
	float StarFade;
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

struct VertexWeight {
	float4 Weights;
	uint4 Indices;
};

#ifdef __cplusplus
#undef uint
#endif