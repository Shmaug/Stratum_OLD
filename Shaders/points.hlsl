#pragma vertex vsmain
#pragma fragment fsmain

#pragma render_queue 5000
#pragma cull false
#pragma zwrite false
#pragma blend_fac src_alpha inv_src_alpha

#pragma static_sampler Sampler

#include <shadercompat.h>

// per-object
[[vk::binding(OBJECT_BUFFER_BINDING, PER_OBJECT)]] ConstantBuffer<ObjectBuffer> Object : register(b0);
[[vk::binding(BINDING_START + 2, PER_OBJECT)]] StructuredBuffer<float3> Vertices : register(t1);
// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);

struct v2f {
	float4 position : SV_Position;
	float3 worldPos : TEXCOORD1;
};
struct fs_out {
	float4 color : SV_Target0;
	float4 depthNormal : SV_Target1;
};

v2f vsmain(uint id : SV_VertexId) {
	v2f o;

	static const float2 s[6] = {
		float2(-1,-1),
		float2( 1,-1),
		float2(-1, 1),
		float2( 1,-1),
		float2( 1, 1),
		float2(-1, 1)
	};

	float2 p = Vertices[id / 6] + PointSize * s[id % 6];
	float4 wp = mul(Object.ObjectToWorld, float4(p, 0, 1.0));

	o.position = mul(Camera.ViewProjection, wp);
	o.worldPos = wp.xyz;

	return o;
}

fs_out fsmain(v2f i) {
	float4 color = Texture.SampleLevel(Sampler, i.texcoord, 0);

	fs_out o;
	o.color = color;
	o.depthNormal = float4(i.normal * .5 + .5, length(Camera.Position - i.worldPos.xyz) / Camera.Viewport.w);
	return o;
}