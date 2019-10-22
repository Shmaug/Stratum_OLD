#include <Content/Mesh.hpp>

#include <regex>
#include <thread>
#include <shared_mutex>

#include <Core/Device.hpp>
#include <Util/Util.hpp>

#include <assimp/scene.h>
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>

#pragma warning(disable:26451)

using namespace std;

struct Vertex {
	float3 position;
	float3 normal;
	float4 tangent;
	float2 uv;

	static const ::VertexInput VertexInput;
};
const ::VertexInput Vertex::VertexInput {
	{
		0, // binding
		sizeof(Vertex), // stride
		VK_VERTEX_INPUT_RATE_VERTEX // inputRate
	},
	{
		{
			0, // location
			0, // binding
			VK_FORMAT_R32G32B32_SFLOAT, // format
			offsetof(Vertex, position) // offset
		},
		{
			1, // location
			0, // binding
			VK_FORMAT_R32G32B32_SFLOAT, // format
			offsetof(Vertex, normal) // offset
		},
		{
			2, // location
			0, // binding
			VK_FORMAT_R32G32B32A32_SFLOAT, // format
			offsetof(Vertex, tangent) // offset
		},
		{
			3, // location
			0, // binding
			VK_FORMAT_R32G32_SFLOAT, // format
			offsetof(Vertex, uv) // offset
		}
	}
};

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

struct VertexWeight {
	uint16_t indices[4];
	float weights[4];
};

Mesh::Mesh(const string& name) : mName(name), mVertexInput(nullptr), mIndexCount(0), mVertexCount(0), mIndexType(VK_INDEX_TYPE_UINT16) {}
Mesh::Mesh(const string& name, ::DeviceManager* devices, const string& filename, float scale)
	: mName(name), mVertexInput(nullptr), mTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) {

	bool use32bit = true;
	vector<Vertex> vertices;
	vector<uint16_t> indices16;
	vector<uint32_t> indices32;
	float3 mn, mx;

	const aiScene* scene = aiImportFile(filename.c_str(), aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_FlipUVs | aiProcess_MakeLeftHanded);
	if (!scene) throw runtime_error("Failed to load " + filename);

	uint32_t vertexCount = 0;
	for (uint32_t m = 0; m < scene->mNumMeshes; m++)
		vertexCount += scene->mMeshes[m]->mNumVertices;
	use32bit = vertexCount > 0xFFFF;

	// append vertices, keep track of bounding box
	for (uint32_t i = 0; i < scene->mNumMeshes; i++) {
		const aiMesh* mesh = scene->mMeshes[i];
		uint32_t baseIndex = (uint32_t)vertices.size();

		for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
			Vertex vertex = {};
			memset(&vertex, 0, sizeof(Vertex));

			vertex.position = { (float)mesh->mVertices[i].x, (float)mesh->mVertices[i].y, (float)mesh->mVertices[i].z };
			if (mesh->HasNormals()) vertex.normal = { (float)mesh->mNormals[i].x, (float)mesh->mNormals[i].y, (float)mesh->mNormals[i].z };
			if (mesh->HasTangentsAndBitangents()) {
				vertex.tangent = { (float)mesh->mTangents[i].x, (float)mesh->mTangents[i].y, (float)mesh->mTangents[i].z, 1.f };
				float3 bt = float3((float)mesh->mBitangents[i].x, (float)mesh->mBitangents[i].y, (float)mesh->mBitangents[i].z);
				vertex.tangent.w = dot(cross(vertex.tangent.xyz, vertex.normal), bt) > 0.f ? 1.f : -1.f;
			}
			if (mesh->HasTextureCoords(0)) vertex.uv = { (float)mesh->mTextureCoords[0][i].x, (float)mesh->mTextureCoords[0][i].y };
			vertex.position *= scale;

			if (i == 0) {
				mn = vertex.position;
				mx = vertex.position;
			} else {
				mn = vmin(vertex.position, mn);
				mx = vmax(vertex.position, mx);
			}

			vertices.push_back(vertex);
		}

		if (mesh->mPrimitiveTypes & aiPrimitiveType::aiPrimitiveType_POINT)
			mTopology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		else if (mesh->mPrimitiveTypes & aiPrimitiveType::aiPrimitiveType_LINE)
			mTopology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

		if (use32bit)
			for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
				const aiFace& f = mesh->mFaces[i];
				if (f.mNumIndices == 0) continue;
				indices32.push_back(f.mIndices[0]);
				if (f.mNumIndices == 2) indices32.push_back(f.mIndices[1]);
				for (uint32_t j = 2; j < f.mNumIndices; j++) {
					indices32.push_back(f.mIndices[j - 1]);
					indices32.push_back(f.mIndices[j]);
				}
			} else {
				for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
					const aiFace& f = mesh->mFaces[i];
					if (f.mNumIndices == 0) continue;
					indices16.push_back(f.mIndices[0]);
					if (f.mNumIndices == 2) indices16.push_back(f.mIndices[1]);
					for (uint32_t j = 2; j < f.mNumIndices; j++) {
						indices16.push_back(f.mIndices[j - 1]);
						indices16.push_back(f.mIndices[j]);
					}
				}
			}


		aiReleaseImport(scene);

		if (use32bit) {
			mIndexCount = (uint32_t)indices32.size();
			mIndexType = VK_INDEX_TYPE_UINT32;
		} else {
			mIndexCount = (uint32_t)indices16.size();
			mIndexType = VK_INDEX_TYPE_UINT16;
		}

		mVertexCount = (uint32_t)vertices.size();
		mBounds = AABB((mn + mx) * .5f, (mx - mn) * .5f);
		mVertexInput = &Vertex::VertexInput;

		for (uint32_t i = 0; i < devices->DeviceCount(); i++) {
			Device* device = devices->GetDevice(i);
			DeviceData& d = mDeviceData[device];
			d.mVertexBuffer = make_shared<Buffer>(name + " Vertex Buffer", device, vertices.data(), sizeof(Vertex) * vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
			if (use32bit)
				d.mIndexBuffer = make_shared<Buffer>(name + " Index Buffer", device, indices32.data(), sizeof(uint32_t) * indices32.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
			else
				d.mIndexBuffer = make_shared<Buffer>(name + " Index Buffer", device, indices16.data(), sizeof(uint16_t) * indices16.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
		}
	}

	printf("Loaded %s: %d verts %d tris / %.2fx%.2fx%.2f\n", filename.c_str(), (int)vertices.size(), (int)(use32bit ? indices32.size() : indices16.size()) / 3, mx.x - mn.x, mx.y - mn.y, mx.z - mn.z);
}
Mesh::Mesh(const string& name, ::DeviceManager* devices, const void* vertices, const void* indices, uint32_t vertexCount, uint32_t vertexSize, uint32_t indexCount, const ::VertexInput* vertexInput, VkIndexType indexType, VkPrimitiveTopology topology)
	: mName(name), mVertexInput(vertexInput), mIndexCount(indexCount), mIndexType(indexType), mVertexCount(vertexCount), mTopology(topology) {
	
	float3 mn, mx;
	for (uint32_t i = 0; i < indexCount; i++) {
		uint32_t index;
		if (mIndexType == VK_INDEX_TYPE_UINT32)
			index = ((uint32_t*)indices)[i];
		else
			index = ((uint16_t*)indices)[i];

		const float3& pos = *(float3*)((uint8_t*)vertices + vertexSize * index);
		if (i == 0)
			mn = mx = pos;
		else {
			mn = vmin(pos, mn);
			mx = vmax(pos, mx);
		}
	}

	uint32_t indexSize = mIndexType == VK_INDEX_TYPE_UINT32 ? sizeof(uint32_t) : sizeof(uint16_t);
	
	mBounds = AABB((mn + mx) * .5f, (mx - mn) * .5f);
	for (uint32_t i = 0; i < devices->DeviceCount(); i++) {
		Device* device = devices->GetDevice(i);
		DeviceData& d = mDeviceData[device];
		d.mVertexBuffer = make_shared<Buffer>(name + " Vertex Buffer", device, vertices, vertexSize * vertexCount, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		d.mIndexBuffer  = make_shared<Buffer>(name + " Index Buffer", device, indices, indexSize * indexCount, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	}
}
Mesh::Mesh(const string& name, ::Device* device, const void* vertices, const void* indices, uint32_t vertexCount, uint32_t vertexSize, uint32_t indexCount, const ::VertexInput* vertexInput, VkIndexType indexType, VkPrimitiveTopology topology)
	: mName(name), mVertexInput(vertexInput), mIndexCount(indexCount), mIndexType(indexType), mVertexCount(vertexCount), mTopology(topology) {

	float3 mn, mx;
	for (uint32_t i = 0; i < indexCount; i++) {
		uint32_t index;
		if (mIndexType == VK_INDEX_TYPE_UINT32)
			index = ((uint32_t*)indices)[i];
		else
			index = ((uint16_t*)indices)[i];

		const float3& pos = *(float3*)((uint8_t*)vertices + vertexSize * index);
		if (i == 0)
			mn = mx = pos;
		else {
			mn = vmin(pos, mn);
			mx = vmax(pos, mx);
		}
	}

	uint32_t indexSize = mIndexType == VK_INDEX_TYPE_UINT32 ? sizeof(uint32_t) : sizeof(uint16_t);

	mBounds = AABB((mn + mx) * .5f, (mx - mn) * .5f);
	DeviceData& d = mDeviceData[device];
	d.mVertexBuffer = make_shared<Buffer>(name + " Vertex Buffer", device, vertices, vertexSize * vertexCount, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	d.mIndexBuffer = make_shared<Buffer>(name + " Index Buffer", device, indices, indexSize * indexCount, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}
Mesh::Mesh(const string& name, ::DeviceManager* devices, const aiMesh* mesh, float scale)
	: mName(name), mVertexInput(nullptr), mTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) {

	vector<Vertex> vertices(mesh->mNumVertices);
	memset(vertices.data(), 0, sizeof(Vertex) * mesh->mNumVertices);

	float3 mn, mx;

	// append vertices, keep track of bounding box
	for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
		Vertex& vertex = vertices[i];

		vertex.position = float3((float)mesh->mVertices[i].x, (float)mesh->mVertices[i].y, (float)mesh->mVertices[i].z);
		if (mesh->HasNormals()) vertex.normal = float3((float)mesh->mNormals[i].x, (float)mesh->mNormals[i].y, (float)mesh->mNormals[i].z);
		if (mesh->HasTangentsAndBitangents()) {
			vertex.tangent = float4((float)mesh->mTangents[i].x, (float)mesh->mTangents[i].y, (float)mesh->mTangents[i].z, 1.f);
			float3 bt = float3((float)mesh->mBitangents[i].x, (float)mesh->mBitangents[i].y, (float)mesh->mBitangents[i].z);
			vertex.tangent.w = dot(cross(vertex.tangent.xyz, vertex.normal), bt) > 0.f ? 1.f : -1.f;
		}
		if (mesh->HasTextureCoords(0)) vertex.uv = float2((float)mesh->mTextureCoords[0][i].x, (float)mesh->mTextureCoords[0][i].y);
		vertex.position *= scale;

		if (i == 0) {
			mn = vertex.position;
			mx = vertex.position;
		} else {
			mn = vmin(mn, vertex.position);
			mx = vmax(mx, vertex.position);
		}
	}

	if (mesh->mPrimitiveTypes & aiPrimitiveType::aiPrimitiveType_POINT)
		mTopology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	else if (mesh->mPrimitiveTypes & aiPrimitiveType::aiPrimitiveType_LINE)
		mTopology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

	bool use32bit = mesh->mNumVertices > 0xFFFF;
	vector<uint16_t> indices16;
	vector<uint32_t> indices32;

	if (use32bit)
		for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
			const aiFace& f = mesh->mFaces[i];
			if (f.mNumIndices == 0) continue;
			indices32.push_back(f.mIndices[0]);
			if (f.mNumIndices == 2) indices32.push_back(f.mIndices[1]);
			for (uint32_t j = 2; j < f.mNumIndices; j++) {
				indices32.push_back(f.mIndices[j - 1]);
				indices32.push_back(f.mIndices[j]);
			}
		} else {
			for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
				const aiFace& f = mesh->mFaces[i];
				if (f.mNumIndices == 0) continue;
				indices16.push_back(f.mIndices[0]);
				if (f.mNumIndices == 2) indices16.push_back(f.mIndices[1]);
				for (uint32_t j = 2; j < f.mNumIndices; j++) {
					indices16.push_back(f.mIndices[j - 1]);
					indices16.push_back(f.mIndices[j]);
				}
			}
		}

	if (use32bit) {
		mIndexCount = (uint32_t)indices32.size();
		mIndexType = VK_INDEX_TYPE_UINT32;
	} else {
		mIndexCount = (uint32_t)indices16.size();
		mIndexType = VK_INDEX_TYPE_UINT16;
	}

	mVertexCount = (uint32_t)vertices.size();
	mVertexInput = &Vertex::VertexInput;
	mBounds = AABB((mn + mx) * .5f, (mx - mn) * .5f);

	for (uint32_t i = 0; i < devices->DeviceCount(); i++) {
		Device* device = devices->GetDevice(i);
		DeviceData& d = mDeviceData[device];
		d.mVertexBuffer = make_shared<Buffer>(name + " Vertex Buffer", device, vertices.data(), sizeof(Vertex) * vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		if (use32bit)
			d.mIndexBuffer = make_shared<Buffer>(name + " Index Buffer", device, indices32.data(), sizeof(uint32_t) * indices32.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
		else
			d.mIndexBuffer = make_shared<Buffer>(name + " Index Buffer", device, indices16.data(), sizeof(uint16_t) * indices16.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	}
}

Mesh* Mesh::CreatePlane(const string& name, DeviceManager* devices, float s) {
	const Vertex verts[4]{
		{ float3(-s, -s, 0), float3(0,0,1), float4(1,0,0,1), float2(0,0) },
		{ float3( s, -s, 0), float3(0,0,1), float4(1,0,0,1), float2(1,0) },
		{ float3(-s,  s, 0), float3(0,0,1), float4(1,0,0,1), float2(0,1) },
		{ float3( s,  s, 0), float3(0,0,1), float4(1,0,0,1), float2(1,1) }
	};
	const uint16_t indices[6]{
		0,2,1,2,3,1,
	};
	return new Mesh(name, devices, verts, indices, 8, sizeof(Vertex), 6, &Vertex::VertexInput, VK_INDEX_TYPE_UINT16);
}
Mesh* Mesh::CreateCube(const string& name, DeviceManager* devices, float r) {
	float3 verts[8]{
		float3(-r, -r, -r),
		float3( r, -r, -r),
		float3(-r, -r,  r),
		float3( r, -r,  r),
		float3(-r,  r, -r),
		float3( r,  r, -r),
		float3(-r,  r,  r),
		float3( r,  r,  r),
	};
	uint16_t indices[36]{
		2,7,6,2,3,7,
		0,1,2,2,1,3,
		1,5,7,7,3,1,
		4,5,1,4,1,0,
		6,4,2,4,0,2,
		4,7,5,4,6,7
	};
	return new Mesh(name, devices, verts, indices, 8, sizeof(float3), 36, &Float3VertexInput, VK_INDEX_TYPE_UINT16);
}

Mesh::~Mesh() {}