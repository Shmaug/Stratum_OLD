#include <Scene/Gizmos.hpp>
#include <Scene/Scene.hpp>
#include <Input/MouseKeyboardInput.hpp>

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
	mGizmoShader = scene->AssetManager()->LoadShader("Shaders/gizmo.shader");
	
	const uint32_t CircleResolution = 64;

	float3 sphereVerts[3*CircleResolution];
	uint16_t sphereIndices[CircleResolution*6];
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
		circleVerts[i].x = cosf(2.f * PI * i / (float)CircleResolution - PI * .5f);
		circleVerts[i].y = sinf(2.f * PI * i / (float)CircleResolution - PI * .5f);
		circleVerts[i].z = 0;
		circleIndices[2*i] = i;
		circleIndices[2*i+1] = (i+1) % CircleResolution;

		sphereVerts[i] = circleVerts[i];
		sphereVerts[CircleResolution+i] = float3(circleVerts[i].x, 0, circleVerts[i].y);
		sphereVerts[2*CircleResolution+i] = float3(0, circleVerts[i].x, circleVerts[i].y);
		sphereIndices[2*i] = i;
		sphereIndices[2*i+1] = (i+1) % CircleResolution;
		sphereIndices[2*CircleResolution + 2*i] = CircleResolution + i;
		sphereIndices[2*CircleResolution + 2*i+1] = CircleResolution + (i+1) % CircleResolution;
		sphereIndices[4*CircleResolution + 2*i] = 2*CircleResolution + i;
		sphereIndices[4*CircleResolution + 2*i+1] = 2*CircleResolution + (i+1) % CircleResolution;
	}

	mCubeMesh = new Mesh("Cube", scene->DeviceManager(), cubeVerts, cubeIndices, 8, sizeof(float3), 36, &Float3VertexInput, VK_INDEX_TYPE_UINT16);
	mWireCubeMesh = new Mesh("Wire Cube", scene->DeviceManager(), cubeVerts, wireCubeIndices, 8, sizeof(float3), 24, &Float3VertexInput, VK_INDEX_TYPE_UINT16, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	mWireCircleMesh = new Mesh("Wire Circle", scene->DeviceManager(), circleVerts, circleIndices, CircleResolution, sizeof(float3), 2*CircleResolution, &Float3VertexInput, VK_INDEX_TYPE_UINT16, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	mWireSphereMesh = new Mesh("Wire Sphere", scene->DeviceManager(), sphereVerts, sphereIndices, 3*CircleResolution, sizeof(float3), 6*CircleResolution, &Float3VertexInput, VK_INDEX_TYPE_UINT16, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
}
Gizmos::~Gizmos() {
	for (auto& it : mDescriptorSets)
		safe_delete(it.second);
	safe_delete(mCubeMesh);
	safe_delete(mWireCubeMesh);
	safe_delete(mWireCircleMesh);
	safe_delete(mWireSphereMesh);
}

bool Gizmos::PositionHandle(CommandBuffer* commandBuffer, uint32_t backBufferIndex, const InputPointer* input, const quaternion& plane, float3& position){
	float radius = .15f;
	float t = input->mWorldRay.Intersect(Sphere(position, radius)).x;
	float lt = input->mLastWorldRay.Intersect(Sphere(position, radius)).x;

	DrawWireCircle(commandBuffer, backBufferIndex, position, radius, plane, float4(1,1,1,1));
	
	if (input->mAxis.at(0) < .5f || t < 0 || lt < 0) return false;

	float3 fwd = plane * float3(0,0,1);
	float3 p  = input->mWorldRay.mOrigin + input->mWorldRay.mDirection * input->mWorldRay.Intersect(fwd, position);
	float3 lp = input->mLastWorldRay.mOrigin + input->mLastWorldRay.mDirection * input->mLastWorldRay.Intersect(fwd, position);

	position += p - lp;

	return true;
}
bool Gizmos::RotationHandle(CommandBuffer* commandBuffer, uint32_t backBufferIndex, const InputPointer* input, const float3& center, quaternion& rotation){
	GraphicsShader* shader = mGizmoShader->GetGraphics(commandBuffer->Device(), {});
	VkPipelineLayout layout = commandBuffer->BindShader(shader, backBufferIndex, &Float3VertexInput, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	if (!layout) return false;

	VkPushConstantRange colorRange = shader->mPushConstants.at("Color");
	VkPushConstantRange posRange   = shader->mPushConstants.at("Position");
	VkPushConstantRange rotRange   = shader->mPushConstants.at("Rotation");
	VkPushConstantRange scaleRange = shader->mPushConstants.at("Scale");

	float radius = .125f;

	float4 color_xy(.2f,.2f,1,.5f);
	float4 color_xz(.2f,1,.2f,.5f);
	float4 color_yz(1,.2f,.2f,.5f);

	float t = input->mWorldRay.Intersect(Sphere(center, radius)).x;
	float lt = input->mLastWorldRay.Intersect(Sphere(center, radius)).x;

	float3 scale(radius);
	vkCmdPushConstants(*commandBuffer, layout, posRange.stageFlags, posRange.offset, posRange.size, &center);
	vkCmdPushConstants(*commandBuffer, layout, scaleRange.stageFlags, scaleRange.offset, scaleRange.size, &scale);

	VkDeviceSize vboffset = 0;
	VkBuffer vb = *mWireCircleMesh->VertexBuffer(commandBuffer->Device());
	vkCmdBindVertexBuffers(*commandBuffer, 0, 1, &vb, &vboffset);
	vkCmdBindIndexBuffer(*commandBuffer, *mWireCircleMesh->IndexBuffer(commandBuffer->Device()), 0, mWireCircleMesh->IndexType());

	quaternion r = rotation;
	vkCmdPushConstants(*commandBuffer, layout, colorRange.stageFlags, colorRange.offset, colorRange.size, &color_xy);
	vkCmdPushConstants(*commandBuffer, layout, rotRange.stageFlags, rotRange.offset, rotRange.size, &r);
	vkCmdDrawIndexed(*commandBuffer, mWireCircleMesh->IndexCount(), 1, 0, 0, 0);

	r *= quaternion(float3(0, PI/2, 0));
	vkCmdPushConstants(*commandBuffer, layout, colorRange.stageFlags, colorRange.offset, colorRange.size, &color_yz);
	vkCmdPushConstants(*commandBuffer, layout, rotRange.stageFlags, rotRange.offset, rotRange.size, &r);
	vkCmdDrawIndexed(*commandBuffer, mWireCircleMesh->IndexCount(), 1, 0, 0, 0);
	
	r *= quaternion(float3(PI/2, 0, 0));
	vkCmdPushConstants(*commandBuffer, layout, colorRange.stageFlags, colorRange.offset, colorRange.size, &color_xz);
	vkCmdPushConstants(*commandBuffer, layout, rotRange.stageFlags, rotRange.offset, rotRange.size, &r);
	vkCmdDrawIndexed(*commandBuffer, mWireCircleMesh->IndexCount(), 1, 0, 0, 0);

	if (input->mAxis.at(0) < .5f || t < 0 || lt < 0) return false;

	float3 p = input->mWorldRay.mOrigin - center + input->mWorldRay.mDirection * t;
	float3 lp = input->mLastWorldRay.mOrigin - center + input->mLastWorldRay.mDirection * lt;

	float3 rotAxis = cross(normalize(lp), normalize(p));
	float angle = length(rotAxis);
	if (fabsf(angle) > .0001f)
		rotation = quaternion(asinf(angle), rotAxis / angle) * rotation;

	return true;
}

void Gizmos::DrawLine(CommandBuffer* commandBuffer, uint32_t backBufferIndex, const float3& p0, const float3& p1, const float4& color){
	float3 v = p1 - p0;
	float l = length(v);
	v /= l;

	float3 axis = cross(v, float3(0,0,1));
	float angle = length(axis);

	DrawWireCube(commandBuffer, backBufferIndex, (p0 + p1) * .5f, float3(0, 0, l * .5f), quaternion(asinf(angle), axis / angle), color);
}
void Gizmos::DrawBillboard(CommandBuffer* commandBuffer, uint32_t backBufferIndex, const float3& center, const float3& extents, const float4& color, Texture* texture, const float4& textureST){
	GraphicsShader* shader = mGizmoShader->GetGraphics(commandBuffer->Device(), {"TEXTURED_QUAD"});
	VkPipelineLayout layout = commandBuffer->BindShader(shader, backBufferIndex, nullptr, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	if (!layout) return;

	VkPushConstantRange colorRange = shader->mPushConstants.at("Color");
	VkPushConstantRange posRange   = shader->mPushConstants.at("Position");
	VkPushConstantRange rotRange   = shader->mPushConstants.at("Rotation");
	VkPushConstantRange scaleRange = shader->mPushConstants.at("Scale");
	VkPushConstantRange stRange = shader->mPushConstants.at("MainTexture_ST");

	vkCmdPushConstants(*commandBuffer, layout, colorRange.stageFlags, colorRange.offset, colorRange.size, &color);
	vkCmdPushConstants(*commandBuffer, layout, posRange.stageFlags, posRange.offset, posRange.size, &center);
	vkCmdPushConstants(*commandBuffer, layout, scaleRange.stageFlags, scaleRange.offset, scaleRange.size, &extents);
	vkCmdPushConstants(*commandBuffer, layout, stRange.stageFlags, stRange.offset, stRange.size, &textureST);

	if (mDescriptorSets.count(texture) == 0){
		DescriptorSet* ds = new DescriptorSet(texture->mName + " DescriptorSet", commandBuffer->Device()->DescriptorPool(), shader->mDescriptorSetLayouts[PER_OBJECT]);
		ds->CreateSampledTextureDescriptor(texture, shader->mDescriptorBindings.at("MainTexture").second.binding);
		mDescriptorSets.emplace(texture, ds);
	}
	VkDescriptorSet ds = *mDescriptorSets.at(texture);
	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &ds, 0, nullptr);
	vkCmdDraw(*commandBuffer, 6, 1, 0, 0);
}
void Gizmos::DrawCube(CommandBuffer* commandBuffer, uint32_t backBufferIndex, const float3& center, const float3& extents, const quaternion& rotation, const float4& color) {
	GraphicsShader* shader = mGizmoShader->GetGraphics(commandBuffer->Device(), {});
	VkPipelineLayout layout = commandBuffer->BindShader(shader, backBufferIndex, &Float3VertexInput, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	if (!layout) return;

	VkPushConstantRange colorRange = shader->mPushConstants.at("Color");
	VkPushConstantRange posRange   = shader->mPushConstants.at("Position");
	VkPushConstantRange rotRange   = shader->mPushConstants.at("Rotation");
	VkPushConstantRange scaleRange = shader->mPushConstants.at("Scale");

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
void Gizmos::DrawWireCube  (CommandBuffer* commandBuffer, uint32_t backBufferIndex, const float3& center, const float3& extents, const quaternion& rotation, const float4& color){
	GraphicsShader* shader = mGizmoShader->GetGraphics(commandBuffer->Device(), {});
	VkPipelineLayout layout = commandBuffer->BindShader(shader, backBufferIndex, &Float3VertexInput, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	if (!layout) return;

	VkPushConstantRange colorRange = shader->mPushConstants.at("Color");
	VkPushConstantRange posRange  =  shader->mPushConstants.at("Position");
	VkPushConstantRange rotRange  =  shader->mPushConstants.at("Rotation");
	VkPushConstantRange scaleRange = shader->mPushConstants.at("Scale");

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
void Gizmos::DrawWireCircle(CommandBuffer* commandBuffer, uint32_t backBufferIndex, const float3& center, float radius, const quaternion& rotation, const float4& color){
	GraphicsShader* shader = mGizmoShader->GetGraphics(commandBuffer->Device(), {});
	VkPipelineLayout layout = commandBuffer->BindShader(shader, backBufferIndex, &Float3VertexInput, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	if (!layout) return;

	float3 scale(radius);

	VkPushConstantRange colorRange = shader->mPushConstants.at("Color");
	VkPushConstantRange posRange   = shader->mPushConstants.at("Position");
	VkPushConstantRange rotRange   = shader->mPushConstants.at("Rotation");
	VkPushConstantRange scaleRange = shader->mPushConstants.at("Scale");

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
void Gizmos::DrawWireSphere(CommandBuffer* commandBuffer, uint32_t backBufferIndex, const float3& center, float radius, const float4& color){
	GraphicsShader* shader = mGizmoShader->GetGraphics(commandBuffer->Device(), {});
	VkPipelineLayout layout = commandBuffer->BindShader(shader, backBufferIndex, &Float3VertexInput, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	if (!layout) return;

	float3 scale(radius);
	quaternion rotation(0,0,0,1);

	VkPushConstantRange colorRange = shader->mPushConstants.at("Color");
	VkPushConstantRange posRange   = shader->mPushConstants.at("Position");
	VkPushConstantRange rotRange   = shader->mPushConstants.at("Rotation");
	VkPushConstantRange scaleRange = shader->mPushConstants.at("Scale");

	vkCmdPushConstants(*commandBuffer, layout, colorRange.stageFlags, colorRange.offset, colorRange.size, &color);
	vkCmdPushConstants(*commandBuffer, layout, posRange.stageFlags, posRange.offset, posRange.size, &center);
	vkCmdPushConstants(*commandBuffer, layout, rotRange.stageFlags, rotRange.offset, rotRange.size, &rotation);
	vkCmdPushConstants(*commandBuffer, layout, scaleRange.stageFlags, scaleRange.offset, scaleRange.size, &scale);

	VkDeviceSize vboffset = 0;
	VkBuffer vb = *mWireSphereMesh->VertexBuffer(commandBuffer->Device());
	vkCmdBindVertexBuffers(*commandBuffer, 0, 1, &vb, &vboffset);
	vkCmdBindIndexBuffer(*commandBuffer, *mWireSphereMesh->IndexBuffer(commandBuffer->Device()), 0, mWireSphereMesh->IndexType());
	vkCmdDrawIndexed(*commandBuffer, mWireSphereMesh->IndexCount(), 1, 0, 0, 0);
}