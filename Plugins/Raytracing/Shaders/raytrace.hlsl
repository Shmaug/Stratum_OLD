#pragma kernel Raytrace

[[vk::binding(0, 0)]] RWTexture2D<float4> OutputTexture : register(u0);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4x4 InvViewProj;
	float3 CameraPosition;
	float2 ScreenResolution;
}

float3 Unproject(float2 uv) {
	float4 unprojected = mul(InvViewProj, float4(uv * 2 - 1, 0, 1));
	return normalize(unprojected.xyz / unprojected.w);
}

[numthreads(8, 8, 1)]
void Raytrace(uint3 index : SV_DispatchThreadID) {
	float3 rd = Unproject(index.xy / ScreenResolution).xyz;

	float3 color = OutputTexture[index.xy].rgb;

	color = rd * .5 + .5;

	OutputTexture[index.xy] = float4(color, 1);
}
