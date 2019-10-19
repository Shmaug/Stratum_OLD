#include <Scene/Gizmos.hpp>
#include <Scene/Scene.hpp>

using namespace std;

const ::VertexInput Float3VertexInput{
	{
		0, // binding
		sizeof(float3), // stride
		VK_VERTEX_INPUT_RATE_VERTEX // inputRate
	},
	{
		{
			0, // location
			0, // binding
			VK_FORMAT_R32G32B32_SFLOAT, // format
			0 // offset
		}
	}
};

Gizmos::Gizmos(Scene* scene) {
	mGizmoMaterial = new Material("Gizmo", scene->AssetManager()->LoadShader("Shaders/gizmo.shader"));
	
	const uint32_t CircleResolution = 36;

	float3 circleVerts[CircleResolution];
	uint16_t circleIndices[CircleResolution*2];
	float3 cubeVerts[8] {
		float3(-1, -1, -1),
		float3( 1, -1, -1),
		float3(-1, -1,  1),
		float3( 1, -1,  1),
		
		float3(-1,  1, -1),
		float3( 1,  1, -1),
		float3(-1,  1,  1),
		float3( 1,  1,  1),
	};
	uint16_t wireCubeIndices[24] {
		0,1, 1,3, 3,2, 2,0,
		4,5, 5,7, 7,6, 6,4,
		0,4, 1,5, 3,7, 2,6
	};
	uint16_t cubeIndices[36] {
		2,7,6,2,3,7,
		0,1,2,2,1,3,
		1,5,7,7,3,1,
		4,5,1,4,1,0,
		6,4,2,4,0,2,
		4,7,5,4,6,7
	};

	for (uint32_t i = 0; i < CircleResolution; i++){
		circleVerts[i].x = cosf(2.f * PI * i / (float)CircleResolution);
		circleVerts[i].y = sinf(2.f * PI * i / (float)CircleResolution);
		circleVerts[i].z = 0;
		circleIndices[2*i] = i;
		circleIndices[2*i+1] = (i+1) % CircleResolution;
	}

	mCubeMesh = new Mesh("Cube", scene->DeviceManager(), cubeVerts, cubeIndices, 8, sizeof(float3), 36, &Float3VertexInput, VK_INDEX_TYPE_UINT16);
	mWireCubeMesh = new Mesh("Wire Cube", scene->DeviceManager(), cubeVerts, wireCubeIndices, 8, sizeof(float3), 24, &Float3VertexInput, VK_INDEX_TYPE_UINT16, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	mWireCircleMesh = new Mesh("Wire Circle", scene->DeviceManager(), circleVerts, circleIndices, CircleResolution, sizeof(float3), 2*CircleResolution, &Float3VertexInput, VK_INDEX_TYPE_UINT16, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);

}
Gizmos::~Gizmos() {
	safe_delete(mGizmoMaterial);
	safe_delete(mCubeMesh);
	safe_delete(mWireCubeMesh);
	safe_delete(mWireCircleMesh);
}

void Gizmos::DrawCube(CommandBuffer* commandBuffer, uint32_t backBufferIndex, const float3& center, const float3& extents, const quaternion& rotation, const float4& color) {
	VkPipelineLayout layout = commandBuffer->BindMaterial(mGizmoMaterial, backBufferIndex, &Float3VertexInput, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	if (!layout) return;

	VkPushConstantRange colorRange = mGizmoMaterial->GetShader(commandBuffer->Device())->mPushConstants.at("Color");
	VkPushConstantRange posRange = mGizmoMaterial->GetShader(commandBuffer->Device())->mPushConstants.at("Position");
	VkPushConstantRange rotRange = mGizmoMaterial->GetShader(commandBuffer->Device())->mPushConstants.at("Rotation");
	VkPushConstantRange scaleRange = mGizmoMaterial->GetShader(commandBuffer->Device())->mPushConstants.at("Scale");

	vkCmdPushConstants(*commandBuffer, layout, colorRange.stageFlags, colorRange.offset, colorRange.size, &color);
	vkCmdPushConstants(*commandBuffer, layout, posRange.stageFlags, posRange.offset, posRange.size, &center);
	vkCmdPushConstants(*commandBuffer, layout, rotRange.stageFlags, rotRange.offset, rotRange.size, &rotation);
	vkCmdPushConstants(*commandBuffer, layout, scaleRange.stageFlags, scaleRange.offset, scaleRange.size, &extents);

	VkDeviceSize vboffset = 0;
	VkBuffer vb = *mCubeMesh->VertexBuffer(commandBuffer->Device());
	vkCmdBindVertexBuffers(*commandBuffer, 0, 1, &vb, &vboffset);
	vkCmdBindIndexBuffer(*commandBuffer, *mCubeMesh->IndexBuffer(commandBuffer->Device()), 0, mCubeMesh->IndexType());
	vkCmdDrawIndexed(*commandBuffer, mCubeMesh->IndexCount(), 1, 0, 0, 0);
}
void Gizmos::DrawWireCircle(CommandBuffer* commandBuffer, uint32_t backBufferIndex, const float3& center, float radius, const quaternion& rotation, const float4& color){
	VkPipelineLayout layout = commandBuffer->BindMaterial(mGizmoMaterial, backBufferIndex, &Float3VertexInput, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	if (!layout) return;

	float3 scale(radius);

	VkPushConstantRange colorRange = mGizmoMaterial->GetShader(commandBuffer->Device())->mPushConstants.at("Color");
	VkPushConstantRange posRange = mGizmoMaterial->GetShader(commandBuffer->Device())->mPushConstants.at("Position");
	VkPushConstantRange rotRange = mGizmoMaterial->GetShader(commandBuffer->Device())->mPushConstants.at("Rotation");
	VkPushConstantRange scaleRange = mGizmoMaterial->GetShader(commandBuffer->Device())->mPushConstants.at("Scale");

	vkCmdPushConstants(*commandBuffer, layout, colorRange.stageFlags, colorRange.offset, colorRange.size, &color);
	vkCmdPushConstants(*commandBuffer, layout, posRange.stageFlags, posRange.offset, posRange.size, &center);
	vkCmdPushConstants(*commandBuffer, layout, rotRange.stageFlags, rotRange.offset, rotRange.size, &rotation);
	vkCmdPushConstants(*commandBuffer, layout, scaleRange.stageFlags, scaleRange.offset, scaleRange.size, &scale);

	VkDeviceSize vboffset = 0;
	VkBuffer vb = *mWireCircleMesh->VertexBuffer(commandBuffer->Device());
	vkCmdBindVertexBuffers(*commandBuffer, 0, 1, &vb, &vboffset);
	vkCmdBindIndexBuffer(*commandBuffer, *mWireCircleMesh->IndexBuffer(commandBuffer->Device()), 0, mWireCircleMesh->IndexType());
	vkCmdDrawIndexed(*commandBuffer, mWireCircleMesh->IndexCount(), 1, 0, 0, 0);
}
void Gizmos::DrawWireCube  (CommandBuffer* commandBuffer, uint32_t backBufferIndex, const float3& center, const float3& extents, const quaternion& rotation, const float4& color){
	VkPipelineLayout layout = commandBuffer->BindMaterial(mGizmoMaterial, backBufferIndex, &Float3VertexInput, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	if (!layout) return;

	VkPushConstantRange colorRange = mGizmoMaterial->GetShader(commandBuffer->Device())->mPushConstants.at("Color");
	VkPushConstantRange posRange = mGizmoMaterial->GetShader(commandBuffer->Device())->mPushConstants.at("Position");
	VkPushConstantRange rotRange = mGizmoMaterial->GetShader(commandBuffer->Device())->mPushConstants.at("Rotation");
	VkPushConstantRange scaleRange = mGizmoMaterial->GetShader(commandBuffer->Device())->mPushConstants.at("Scale");

	vkCmdPushConstants(*commandBuffer, layout, colorRange.stageFlags, colorRange.offset, colorRange.size, &color);
	vkCmdPushConstants(*commandBuffer, layout, posRange.stageFlags, posRange.offset, posRange.size, &center);
	vkCmdPushConstants(*commandBuffer, layout, rotRange.stageFlags, rotRange.offset, rotRange.size, &rotation);
	vkCmdPushConstants(*commandBuffer, layout, scaleRange.stageFlags, scaleRange.offset, scaleRange.size, &extents);

	VkDeviceSize vboffset = 0;
	VkBuffer vb = *mWireCubeMesh->VertexBuffer(commandBuffer->Device());
	vkCmdBindVertexBuffers(*commandBuffer, 0, 1, &vb, &vboffset);
	vkCmdBindIndexBuffer(*commandBuffer, *mWireCubeMesh->IndexBuffer(commandBuffer->Device()), 0, mWireCubeMesh->IndexType());
	vkCmdDrawIndexed(*commandBuffer, mWireCubeMesh->IndexCount(), 1, 0, 0, 0);
}