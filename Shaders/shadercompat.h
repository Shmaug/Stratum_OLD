#ifdef __cplusplus
#include <glm/glm.hpp>
#define int2 ivec2
#define int3 ivec3
#define int4 ivec4
#define uint uint32_t
#define uint2 uvec2
#define uint3 uvec3
#define uint4 uvec4
#define float2 vec2
#define float3 vec3
#define float4 vec4
#define float2x2 mat2
#define float3x3 mat3
#define float4x4 mat4
#pragma pack(push)
#pragma pack(1)
#endif

#define PER_CAMERA 0
#define PER_MATERIAL 1
#define PER_OBJECT 2

#define CAMERA_BUFFER_BINDING 0
#define OBJECT_BUFFER_BINDING 1
#define BINDING_START 2

struct CameraBuffer {
	float4x4 ViewProjection;
	float4 Viewport;
	float3 Position;
	float C0;
	float C1;
};
struct ObjectBuffer {
	float4x4 ObjectToWorld;
	float4x4 WorldToObject;
};

#ifdef __cplusplus
#pragma pack(pop)
#endif