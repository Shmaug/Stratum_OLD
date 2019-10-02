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
	vec3 position;
	vec3 normal;
	vec4 tangent;
	vec2 uv;

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

struct VertexWeight {
	uint16_t indices[4];
	float weights[4];
};

Mesh::Mesh(const string& name) : mName(name), mVertexInput(nullptr), mIndexCount(0), mVertexCount(0), mIndexType(VK_INDEX_TYPE_UINT16) {}
Mesh::Mesh(const string& name, ::DeviceManager* devices, const string& filename, float scale)
	: mName(name), mVertexInput(nullptr) {
	const aiScene* scene = aiImportFile(filename.c_str(), aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_FlipUVs);
	if (!scene) {
		printf("Failed to load %s\n", filename.c_str());
		assert(scene);
	}

	unsigned int vertexCount = 0;
	for (unsigned int m = 0; m < scene->mNumMeshes; m++)
		vertexCount += scene->mMeshes[m]->mNumVertices;
	bool use32bit = vertexCount > 0xFFFF;

	vector<Vertex> vertices;
	vector<uint16_t> indices16;
	vector<uint32_t> indices32;

	vec3 mn, mx;

	// append vertices, keep track of bounding box
	for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
		const aiMesh* mesh = scene->mMeshes[i];
		uint32_t baseIndex = (uint32_t)vertices.size();

		for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
			Vertex vertex = {};
			memset(&vertex, 0, sizeof(Vertex));

			vertex.position = { (float)mesh->mVertices[i].x, (float)mesh->mVertices[i].y, (float)mesh->mVertices[i].z };
			if (mesh->HasNormals()) vertex.normal = { (float)mesh->mNormals[i].x, (float)mesh->mNormals[i].y, (float)mesh->mNormals[i].z };
			if (mesh->HasTangentsAndBitangents()) {
				vertex.tangent = { (float)mesh->mTangents[i].x, (float)mesh->mTangents[i].y, (float)mesh->mTangents[i].z, 1.f };
				vec3 bt = vec3((float)mesh->mBitangents[i].x, (float)mesh->mBitangents[i].y, (float)mesh->mBitangents[i].z);
				vertex.tangent.w = dot(cross((vec3)vertex.tangent, vertex.normal), bt) > 0.f ? 1.f : -1.f;
			}
			if (mesh->HasTextureCoords(0)) vertex.uv = { (float)mesh->mTextureCoords[0][i].x, (float)mesh->mTextureCoords[0][i].y };
			vertex.position *= scale;

			if (i == 0) {
				mn = vertex.position;
				mx = vertex.position;
			} else {
				mn.x = fminf(vertex.position.x, mn.x);
				mn.y = fminf(vertex.position.y, mn.y);
				mn.z = fminf(vertex.position.z, mn.z);
				mx.x = fmaxf(vertex.position.x, mx.x);
				mx.y = fmaxf(vertex.position.y, mx.y);
				mx.z = fmaxf(vertex.position.z, mx.z);
			}

			vertices.push_back(vertex);
		}

		if (use32bit)
			for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
				const aiFace& f = mesh->mFaces[i];
				assert(f.mNumIndices == 3);
				indices32.push_back(baseIndex + f.mIndices[0]);
				indices32.push_back(baseIndex + f.mIndices[1]);
				indices32.push_back(baseIndex + f.mIndices[2]);
			}
		else
			for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
				const aiFace& f = mesh->mFaces[i];
				assert(f.mNumIndices == 3);
				indices16.push_back(baseIndex + f.mIndices[0]);
				indices16.push_back(baseIndex + f.mIndices[1]);
				indices16.push_back(baseIndex + f.mIndices[2]);
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
	mBounds = AABB((mn + mx) * .5f, (mx - mn) * .5f);
	mVertexInput = &Vertex::VertexInput;

	for (uint32_t i = 0; i < devices->DeviceCount(); i++) {
		Device* device = devices->GetDevice(i);
		DeviceData& d = mDeviceData[device];
		d.mVertexBuffer = make_shared<Buffer>(name + " Vertex Buffer", device, vertices.data(), sizeof(Vertex) * vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		if (use32bit)
			d.mIndexBuffer = make_shared<Buffer>(name + " Index Buffer", device, indices32.data(), sizeof(uint32_t) * indices32.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
		else
			d.mIndexBuffer  = make_shared<Buffer>(name + " Index Buffer", device, indices16.data(), sizeof(uint16_t) * indices16.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	}

	aiReleaseImport(scene);

	printf("Loaded %s: %d verts %d tris / %.2fx%.2fx%.2f\n", filename.c_str(), (int)vertices.size(), (int)(use32bit ? indices32.size() : indices16.size()) / 3, mx.x - mn.x, mx.y - mn.y, mx.z - mn.z);
}
Mesh::Mesh(const string& name, ::DeviceManager* devices, const void* vertices, const void* indices, uint32_t vertexCount, uint32_t vertexSize, uint32_t indexCount, const ::VertexInput* vertexInput, VkIndexType indexType)
	: mName(name), mVertexInput(vertexInput), mIndexCount(indexCount), mIndexType(indexType), mVertexCount(vertexCount) {
	
	vec3 mn, mx;
	for (uint32_t i = 0; i < indexCount; i++) {
		uint32_t index;
		if (mIndexType == VK_INDEX_TYPE_UINT32)
			index = ((uint32_t*)indices)[i];
		else
			index = ((uint16_t*)indices)[i];

		const vec3& pos = *(vec3*)((uint8_t*)vertices + vertexSize * index);
		if (i == 0)
			mn = mx = pos;
		else {
			mn = min(pos, mn);
			mx = max(pos, mn);
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
Mesh::Mesh(const string& name, ::Device* device, const void* vertices, const void* indices, uint32_t vertexCount, uint32_t vertexSize, uint32_t indexCount, const ::VertexInput* vertexInput, VkIndexType indexType)
	: mName(name), mVertexInput(vertexInput), mIndexCount(indexCount), mIndexType(indexType), mVertexCount(vertexCount) {

	vec3 mn, mx;
	for (uint32_t i = 0; i < indexCount; i++) {
		uint32_t index;
		if (mIndexType == VK_INDEX_TYPE_UINT32)
			index = ((uint32_t*)indices)[i];
		else
			index = ((uint16_t*)indices)[i];

		const vec3& pos = *(vec3*)((uint8_t*)vertices + vertexSize * index);
		if (i == 0)
			mn = mx = pos;
		else {
			mn = min(pos, mn);
			mx = max(pos, mn);
		}
	}

	uint32_t indexSize = mIndexType == VK_INDEX_TYPE_UINT32 ? sizeof(uint32_t) : sizeof(uint16_t);

	mBounds = AABB((mn + mx) * .5f, (mx - mn) * .5f);
	DeviceData& d = mDeviceData[device];
	d.mVertexBuffer = make_shared<Buffer>(name + " Vertex Buffer", device, vertices, vertexSize * vertexCount, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	d.mIndexBuffer = make_shared<Buffer>(name + " Index Buffer", device, indices, indexSize * indexCount, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}
Mesh::Mesh(const string& name, ::DeviceManager* devices, const aiMesh* mesh, float scale)
	: mName(name), mVertexInput(nullptr) {
	bool use32bit = mesh->mNumVertices > 0xFFFF;

	vector<Vertex> vertices(mesh->mNumVertices);
	memset(vertices.data(), 0, sizeof(Vertex) * mesh->mNumVertices);
	vector<uint16_t> indices16;
	vector<uint32_t> indices32;

	vec3 mn, mx;

	// append vertices, keep track of bounding box
	for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
		Vertex& vertex = vertices[i];

		vertex.position = { (float)mesh->mVertices[i].x, (float)mesh->mVertices[i].y, (float)mesh->mVertices[i].z };
		if (mesh->HasNormals()) vertex.normal = { (float)mesh->mNormals[i].x, (float)mesh->mNormals[i].y, (float)mesh->mNormals[i].z };
		if (mesh->HasTangentsAndBitangents()) {
			vertex.tangent = { (float)mesh->mTangents[i].x, (float)mesh->mTangents[i].y, (float)mesh->mTangents[i].z, 1.f };
			vec3 bt = vec3((float)mesh->mBitangents[i].x, (float)mesh->mBitangents[i].y, (float)mesh->mBitangents[i].z);
			vertex.tangent.w = dot(cross((vec3)vertex.tangent, vertex.normal), bt) > 0.f ? 1.f : -1.f;
		}
		if (mesh->HasTextureCoords(0)) vertex.uv = { (float)mesh->mTextureCoords[0][i].x, (float)mesh->mTextureCoords[0][i].y };
		vertex.position *= scale;

		if (i == 0) {
			mn = vertex.position;
			mx = vertex.position;
		} else {
			mn.x = fminf(vertex.position.x, mn.x);
			mn.y = fminf(vertex.position.y, mn.y);
			mn.z = fminf(vertex.position.z, mn.z);
			mx.x = fmaxf(vertex.position.x, mx.x);
			mx.y = fmaxf(vertex.position.y, mx.y);
			mx.z = fmaxf(vertex.position.z, mx.z);
		}
	}

	if (use32bit)
		for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
			const aiFace& f = mesh->mFaces[i];
			assert(f.mNumIndices == 3);
			indices32.push_back(f.mIndices[0]);
			indices32.push_back(f.mIndices[1]);
			indices32.push_back(f.mIndices[2]);
		} else
			for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
				const aiFace& f = mesh->mFaces[i];
				assert(f.mNumIndices == 3);
				indices16.push_back(f.mIndices[0]);
				indices16.push_back(f.mIndices[1]);
				indices16.push_back(f.mIndices[2]);
			}

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

Mesh::~Mesh() {}