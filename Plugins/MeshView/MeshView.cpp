#include <Scene/MeshRenderer.hpp>
#include <Scene/SkinnedMeshRenderer.hpp>
#include <Util/Profiler.hpp>
#include <Util/Tokenizer.hpp>

#include <Core/EnginePlugin.hpp>
#include <assimp/pbrmaterial.h>

#define THROW_INVALID_SKEL { fprintf_color(COLOR_RED, stderr, "Invalid SKEL file\n"); throw; }
#define THROW_INVALID_SKIN { fprintf_color(COLOR_RED, stderr, "Invalid SKIN file\n"); throw; }

using namespace std;

class SkelJoint : public Bone {
public:
	AABB mBox;
	float3 mPose;
	float3 mRotMin;
	float3 mRotMax;

	inline SkelJoint(const std::string& name, uint32_t index) : Bone(name, index), Object(name), mRotMin(-1e10f), mRotMax(1e10f) {};

	PLUGIN_EXPORT virtual void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) override {
		MouseKeyboardInput* input = Scene()->InputManager()->GetFirst<MouseKeyboardInput>();

		quaternion r = WorldRotation();
		if (Gizmos::RotationHandle(mName + to_string(mBoneIndex), input->GetPointer(0), WorldPosition(), r, .1f))
			LocalRotation(Parent() ? inverse(Parent()->WorldRotation()) * r : r);

		float3 center = (ObjectToWorld() * float4(mBox.Center(), 1.f)).xyz;
		Gizmos::DrawWireCube(center, mBox.Extents(), WorldRotation(), float4(1,1,1,.2f));
	}
};

class MeshView : public EnginePlugin {
public:
	PLUGIN_EXPORT MeshView() : mScene(nullptr), mSelected(nullptr), mInput(nullptr), mHeadBlend(0.f) {
		mEnabled = true;
	}
	PLUGIN_EXPORT ~MeshView() {
		for (Object* obj : mObjects)
			mScene->RemoveObject(obj);
	}

	PLUGIN_EXPORT bool Init(Scene* scene) override;

	PLUGIN_EXPORT void Update() override {
		if (mInput->KeyDown(KEY_RIGHT)) mHeadBlend += 2.f * mScene->Instance()->DeltaTime();
		if (mInput->KeyDown(KEY_LEFT))  mHeadBlend -= 2.f * mScene->Instance()->DeltaTime();
		mHeadBlend = clamp(mHeadBlend, 0.f, 1.f);
		mHead->ShapeKey("head1", mHeadBlend);
		mHead->ShapeKey("head2", 1.f - mHeadBlend);
	}

	PLUGIN_EXPORT void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera);

private:
	Scene* mScene;
	vector<Object*> mObjects;
	Object* mSelected;

	SkinnedMeshRenderer* mHead;

	float mHeadBlend;

	SkelJoint* ReadJoint(Tokenizer& t, AnimationRig& destRig, const string &name, float scale) {
		shared_ptr<SkelJoint> j = make_shared<SkelJoint>(name, destRig.size());
		mScene->AddObject(j);
		mObjects.push_back(j.get());

		destRig.push_back(j.get());

		string token;
		if (!t.Next(token) || token != "{") THROW_INVALID_SKEL

		while (t.Next(token)) {
			if (token == "offset") {
				float3 offset;
				if (!t.Next(offset.x)) THROW_INVALID_SKEL
				if (!t.Next(offset.y)) THROW_INVALID_SKEL
				if (!t.Next(offset.z)) THROW_INVALID_SKEL
				offset *=scale;
				offset.z = -offset.z;
				j->LocalPosition(offset);
				continue;
			}
			if (token == "boxmin") {
				if (!t.Next(j->mBox.mMin.x)) THROW_INVALID_SKEL
				if (!t.Next(j->mBox.mMin.y)) THROW_INVALID_SKEL
				if (!t.Next(j->mBox.mMin.z)) THROW_INVALID_SKEL
				j->mBox.mMin *= scale;
				j->mBox.mMin.z = -j->mBox.mMin.z;
				continue;
			}
			if (token == "boxmax") {
				if (!t.Next(j->mBox.mMax.x)) THROW_INVALID_SKEL
				if (!t.Next(j->mBox.mMax.y)) THROW_INVALID_SKEL
				if (!t.Next(j->mBox.mMax.z)) THROW_INVALID_SKEL
				j->mBox.mMax *= scale;
				j->mBox.mMax.z = -j->mBox.mMax.z;
				continue;
			}
			if (token == "rotxlimit") {
				if (!t.Next(j->mRotMin.x)) THROW_INVALID_SKEL
				if (!t.Next(j->mRotMax.x)) THROW_INVALID_SKEL
				j->mPose.x = clamp(j->mPose.x, j->mRotMin.x, j->mRotMax.x);
				continue;
			}
			if (token == "rotylimit") {
				if (!t.Next(j->mRotMin.y)) THROW_INVALID_SKEL
				if (!t.Next(j->mRotMax.y)) THROW_INVALID_SKEL
				j->mPose.y = clamp(j->mPose.y, j->mRotMin.y, j->mRotMax.y);
				continue;
			}
			if (token == "rotzlimit") {
				if (!t.Next(j->mRotMin.z)) THROW_INVALID_SKEL
				if (!t.Next(j->mRotMax.z)) THROW_INVALID_SKEL
				j->mPose.z = clamp(j->mPose.z, j->mRotMin.z, j->mRotMax.z);
				continue;
			}
			if (token == "pose") {
				if (!t.Next(j->mPose.x)) THROW_INVALID_SKEL
				if (!t.Next(j->mPose.y)) THROW_INVALID_SKEL
				if (!t.Next(j->mPose.z)) THROW_INVALID_SKEL
				j->mPose = clamp(j->mPose, j->mRotMin, j->mRotMax);
				quaternion r(j->mPose);
				r.x = -r.x;
				r.y = -r.y;
				j->LocalRotation(r);

				continue;
			}
			if (token == "balljoint") {
				if (!t.Next(token)) THROW_INVALID_SKEL
				j->AddChild(ReadJoint(t, destRig, token, scale));
				continue;
			}

			if (token == "}") break;
		}

		return j.get();
	}

	void LoadSkel(const fs::path& filepath, AnimationRig& destRig, float scale) {
		ifstream file(filepath.string());

		Tokenizer t(file, { ' ', '\n', '\r', '\t' });

		string token;
		if (!t.Next(token) || token != "balljoint") THROW_INVALID_SKEL
			if (!t.Next(token)) THROW_INVALID_SKEL
				ReadJoint(t, destRig, token, scale);

		if (t.Next(token)) {
			if (token == "balljoint") {
				fprintf_color(COLOR_RED, stderr, "Multiple root joints!\n");
				throw;
			}
			THROW_INVALID_SKEL
		}

		printf("Loaded %s\n", filepath.string().c_str());
	}

	shared_ptr<Mesh> LoadSkin(const fs::path& filepath, AnimationRig& destRig, float scale, bool calcNormals) {
		ifstream file(filepath.string());

		Tokenizer t(file, { '{', '}', ' ', '\n', '\r', '\t' });

		vector<StdVertex> vertices;
		vector<VertexWeight> weights;
		vector<uint32_t> indices;

		uint32_t sz;
		uint32_t i;

		string token;
		while (t.Next(token)) {
			if (token == "positions") {
				if (!t.Next(sz)) THROW_INVALID_SKIN
				if (sz > vertices.size()) vertices.resize(sz);
				for (i = 0; i < sz; i++) {
					if (!t.Next(vertices[i].position.x)) THROW_INVALID_SKIN
					if (!t.Next(vertices[i].position.y)) THROW_INVALID_SKIN
					if (!t.Next(vertices[i].position.z)) THROW_INVALID_SKIN
					vertices[i].position *= scale;
					vertices[i].position.z = -vertices[i].position.z;
				}
			} else if (token == "normals") {
				if (!t.Next(sz)) THROW_INVALID_SKIN
				if (sz > vertices.size()) vertices.resize(sz);
				for (i = 0; i < sz; i++) {
					if (!t.Next(vertices[i].normal.x)) THROW_INVALID_SKIN
					if (!t.Next(vertices[i].normal.y)) THROW_INVALID_SKIN
					if (!t.Next(vertices[i].normal.z)) THROW_INVALID_SKIN
					vertices[i].normal.z = -vertices[i].normal.z;
					vertices[i].normal = normalize(vertices[i].normal);
				}
			} else if (token == "texcoords") {
				if (!t.Next(sz)) THROW_INVALID_SKIN
				if (sz > vertices.size()) vertices.resize(sz);
				for (i = 0; i < sz; i++) {
					if (!t.Next(vertices[i].uv.x)) THROW_INVALID_SKIN
					if (!t.Next(vertices[i].uv.y)) THROW_INVALID_SKIN
					vertices[i].uv.y = 1 - vertices[i].uv.y;
					vertices[i].tangent = 1;
				}
			} else if (token == "skinweights") {
				if (!t.Next(sz)) THROW_INVALID_SKIN
				if (sz > weights.size()) weights.resize(sz);
				for (i = 0; i < sz; i++) {
					uint32_t c;
					if (!t.Next(c)) THROW_INVALID_SKIN;
					if (c > 4) { fprintf_color(COLOR_RED, stderr, "More than 4 weights per vertex not supported!\n"); throw; }
					weights[i].Weights = 0;
					for (uint32_t j = 0; j < c; j++) {
						if (!t.Next(weights[i].Indices[j])) THROW_INVALID_SKIN
						if (!t.Next(weights[i].Weights[j])) THROW_INVALID_SKIN
					}
				}
			} else if (token == "triangles") {
				if (!t.Next(sz)) THROW_INVALID_SKIN
				if (sz*3 > indices.size()) indices.resize(sz*3);
				for (i = 0; i < sz; i++) {
					if (!t.Next(indices[3*i+0])) THROW_INVALID_SKIN
					if (!t.Next(indices[3*i+1])) THROW_INVALID_SKIN
					if (!t.Next(indices[3*i+2])) THROW_INVALID_SKIN
				}
			} else if (token == "bindings") {
				if (!t.Next(sz)) THROW_INVALID_SKIN
				if (sz != destRig.size()) THROW_INVALID_SKIN
				for (i = 0; i < sz; i++) {
					if (!t.Next(token)) THROW_INVALID_SKIN;
					if (token != "matrix") THROW_INVALID_SKIN;

					Bone* j = destRig[i];
					j->mInverseBind = float4x4(1);
					if (!t.Next(j->mInverseBind[0][0])) THROW_INVALID_SKIN
					if (!t.Next(j->mInverseBind[0][1])) THROW_INVALID_SKIN
					if (!t.Next(j->mInverseBind[0][2])) THROW_INVALID_SKIN

					if (!t.Next(j->mInverseBind[1][0])) THROW_INVALID_SKIN
					if (!t.Next(j->mInverseBind[1][1])) THROW_INVALID_SKIN
					if (!t.Next(j->mInverseBind[1][2])) THROW_INVALID_SKIN
					
					if (!t.Next(j->mInverseBind[2][0])) THROW_INVALID_SKIN
					if (!t.Next(j->mInverseBind[2][1])) THROW_INVALID_SKIN
					if (!t.Next(j->mInverseBind[2][2])) THROW_INVALID_SKIN
					
					if (!t.Next(j->mInverseBind[3][0])) THROW_INVALID_SKIN
					if (!t.Next(j->mInverseBind[3][1])) THROW_INVALID_SKIN
					if (!t.Next(j->mInverseBind[3][2])) THROW_INVALID_SKIN

					j->mInverseBind[0][2] = -j->mInverseBind[0][2];
					j->mInverseBind[1][2] = -j->mInverseBind[1][2];
					j->mInverseBind[2][2] = -j->mInverseBind[2][2];
					j->mInverseBind[3][2] = -j->mInverseBind[3][2];

					j->mInverseBind[2][0] = -j->mInverseBind[2][0];
					j->mInverseBind[2][1] = -j->mInverseBind[2][1];
					j->mInverseBind[2][2] = -j->mInverseBind[2][2];
					j->mInverseBind[2][3] = -j->mInverseBind[2][3];

					j->mInverseBind = inverse(j->mInverseBind);
				}
			}
		}

		if (calcNormals){
			for (i = 0; i < vertices.size(); i++)
				vertices[i].normal = 0;
			for (i = 0; i < indices.size(); i+=3) {
				float3 n = normalize(cross(
					normalize(vertices[indices[i+2]].position - vertices[indices[i]].position),
					normalize(vertices[indices[i+1]].position - vertices[indices[i]].position) ));
				vertices[indices[i+0]].normal += n;
				vertices[indices[i+1]].normal += n;
				vertices[indices[i+2]].normal += n;
			}
			for (i = 0; i < vertices.size(); i++)
				vertices[i].normal = normalize(vertices[i].normal);
		}
		
		printf("Loaded %s\n", filepath.string().c_str());

		vector<vector<StdVertex>> shapes;
		vector<pair<string, const void*>> shapeKeys;

		for (const auto& p : fs::directory_iterator(filepath.parent_path())) {
			if (p.path().extension().string() == ".morph") {
				ifstream mfile(p.path().string());
				Tokenizer mt(mfile, { '{', '}', ' ', '\n', '\r', '\t' });

				shapes.push_back({});
				vector<StdVertex>& shape = shapes.back();
				shape.resize(vertices.size());
				memcpy(shape.data(), vertices.data(), vertices.size() * sizeof(StdVertex));


				while (mt.Next(token)) {
					if (token == "positions") {
						if (!mt.Next(sz)) THROW_INVALID_SKIN
						for (i = 0; i < sz; i++) {
							uint32_t j;
							if (!mt.Next(j)) THROW_INVALID_SKIN
							if (!mt.Next(shape[j].position.x)) THROW_INVALID_SKIN
							if (!mt.Next(shape[j].position.y)) THROW_INVALID_SKIN
							if (!mt.Next(shape[j].position.z)) THROW_INVALID_SKIN
							shape[j].position *= scale;
							shape[j].position.z = -shape[j].position.z;
						}
					} else if (token == "normals") {
						if (!mt.Next(sz)) THROW_INVALID_SKIN
						for (i = 0; i < sz; i++) {
							uint32_t j;
							if (!mt.Next(j)) THROW_INVALID_SKIN
							if (!mt.Next(shape[j].normal.x)) THROW_INVALID_SKIN
							if (!mt.Next(shape[j].normal.y)) THROW_INVALID_SKIN
							if (!mt.Next(shape[j].normal.z)) THROW_INVALID_SKIN
							shape[j].normal.z = -shape[j].normal.z;
						}
					}
				}

				shapeKeys.push_back(make_pair(p.path().stem().string(), shape.data()));

				printf("Loaded %s\n", p.path().string().c_str());
			}
		}

		return make_shared<Mesh>(filepath.filename().string(), mScene->Instance()->Device(),
			vertices.data(), weights.data(), shapeKeys, indices.data(), (uint32_t)vertices.size(), sizeof(StdVertex),
			(uint32_t)indices.size(), &StdVertex::VertexInput, VK_INDEX_TYPE_UINT32);
	}

	Object* Load(const fs::path& filepath, float scale) {
		string ext = filepath.has_extension() ? filepath.extension().string() : "";
		// load with Assimp
		return nullptr;
	}

	MouseKeyboardInput* mInput;
};

ENGINE_PLUGIN(MeshView)

bool MeshView::Init(Scene* scene) {
	mScene = scene;
	mInput = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

	mScene->Environment()->EnableCelestials(false);
	mScene->Environment()->EnableScattering(false);
	mScene->Environment()->AmbientLight(.3f);

	auto light = make_shared<Light>("Spot");
	light->CastShadows(true);
	light->Type(LIGHT_TYPE_SPOT);
	light->Color(float3(1, .4f, .4f));
	light->Intensity(15.f);
	light->Range(16.f);
	light->LocalPosition(3, 3, -1);
	light->InnerSpotAngle(radians(20.f));
	light->OuterSpotAngle(radians(30.f));
	light->LocalRotation(quaternion(float3(PI * .25f, -PI / 2, 0)));
	mScene->AddObject(light);
	mObjects.push_back(light.get());

	auto light2 = make_shared<Light>("Spot");
	light2->CastShadows(true);
	light2->Type(LIGHT_TYPE_SPOT);
	light2->Color(float3(.4, 1, .4f));
	light2->Intensity(15.f);
	light2->Range(16.f);
	light2->LocalPosition(-3, 3, 1);
	light2->InnerSpotAngle(radians(10.f));
	light2->OuterSpotAngle(radians(25.f));
	light2->LocalRotation(quaternion(float3(PI * .25f, PI / 2, 0)));
	mScene->AddObject(light2);
	mObjects.push_back(light2.get());

	#pragma region plane
	auto planeMat = make_shared<Material>("Plane", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
	planeMat->EnableKeyword("TEXTURED");
	planeMat->SetParameter("MainTextures", 0, mScene->AssetManager()->LoadTexture("Assets/Textures/grid.png"));
	planeMat->SetParameter("NormalTextures", 0, mScene->AssetManager()->LoadTexture("Assets/Textures/bump.png"));
	planeMat->SetParameter("MaskTextures", 0, mScene->AssetManager()->LoadTexture("Assets/Textures/mask.png"));
	planeMat->SetParameter("TextureST", float4(256, 256, 1, 1));
	planeMat->SetParameter("Color", float4(1));
	planeMat->SetParameter("Metallic", 0.f);
	planeMat->SetParameter("Roughness", .5f);
	planeMat->SetParameter("BumpStrength", 1.f);
	planeMat->SetParameter("Emission", float3(0));

	auto plane = make_shared<MeshRenderer>("Plane");
	plane->Mesh(shared_ptr<Mesh>(Mesh::CreatePlane("Plane", mScene->Instance()->Device(), 512.f)));
	plane->Material(planeMat);
	plane->PushConstant("TextureIndex", 0u);
	plane->LocalRotation(quaternion(float3(-PI / 2, 0, 0)));
	mScene->AddObject(plane);
	mObjects.push_back(plane.get());
	#pragma endregion

	#pragma region wasp
	auto waspMat = make_shared<Material>("Wasp", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
	waspMat->SetParameter("Color", float4(1));
	waspMat->SetParameter("Metallic", 0.f);
	waspMat->SetParameter("Roughness", .5f);
	waspMat->SetParameter("Emission", float3(0));

	AnimationRig waspRig;
	LoadSkel("Assets/Models/wasp/wasp.skel", waspRig, 1);

	waspRig[0]->LocalPosition(-1, 1, 0);

	auto wasp = make_shared<SkinnedMeshRenderer>("Wasp");
	wasp->Mesh(LoadSkin("Assets/Models/wasp/wasp.skin", waspRig,  1, true));
	wasp->Rig(waspRig);
	wasp->Material(waspMat);
	wasp->PushConstant("TextureIndex", 0u);
	mScene->AddObject(wasp);
	mObjects.push_back(wasp.get());
	waspRig[0]->AddChild(wasp.get());
	#pragma endregion

	#pragma region head
	auto headMat = make_shared<Material>("Head", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
	headMat->EnableKeyword("TEXTURED");
	headMat->SetParameter("MainTextures", 0, mScene->AssetManager()->LoadTexture("Assets/Models/head/head.png"));
	headMat->SetParameter("NormalTextures", 0, mScene->AssetManager()->LoadTexture("Assets/Textures/bump.png"));
	headMat->SetParameter("MaskTextures", 0, mScene->AssetManager()->LoadTexture("Assets/Textures/mask.png"));
	headMat->SetParameter("TextureST", float4(1, 1, 0, 0));
	headMat->SetParameter("Color", float4(1));
	headMat->SetParameter("Metallic", 0.f);
	headMat->SetParameter("Roughness", .9f);
	headMat->SetParameter("BumpStrength", 1.f);
	headMat->SetParameter("Emission", float3(0));

	AnimationRig headRig;
	LoadSkel("Assets/Models/head/head.skel", headRig, .3f);

	headRig[0]->LocalPosition(1, 1, 0);
	headRig[0]->LocalRotation(quaternion(float3(0, -PI / 2, 0)));

	auto head = make_shared<SkinnedMeshRenderer>("Head");
	head->Mesh(LoadSkin("Assets/Models/head/head_tex.skin", headRig, .3f, false));
	head->Rig(headRig);
	head->Material(headMat);
	head->PushConstant("TextureIndex", 0u);
	mScene->AddObject(head);
	mObjects.push_back(head.get());
	headRig[0]->AddChild(head.get());
	mHead = head.get();
	#pragma endregion

	return true;
}

void MeshView::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
	const Ray& ray = mInput->GetPointer(0)->mWorldRay;

	bool change = mInput->GetPointer(0)->mAxis.at(0) > .5f;

	// manipulate selection
	Light* selectedLight = nullptr;
	if (mSelected) {
		selectedLight = dynamic_cast<Light*>(mSelected);
		if (selectedLight) {
			switch (selectedLight->Type()) {
			case LIGHT_TYPE_SPOT:
				Gizmos::DrawWireSphere(selectedLight->WorldPosition(), selectedLight->Radius(), float4(selectedLight->Color(), .5f));
				Gizmos::DrawWireCircle(selectedLight->WorldPosition() + selectedLight->WorldRotation() * float3(0, 0, selectedLight->Range()),
					selectedLight->Range() * tanf(selectedLight->InnerSpotAngle()), selectedLight->WorldRotation(), float4(selectedLight->Color(), .5f));
				Gizmos::DrawWireCircle(
					selectedLight->WorldPosition() + selectedLight->WorldRotation() * float3(0, 0, selectedLight->Range()),
					selectedLight->Range() * tanf(selectedLight->OuterSpotAngle()), selectedLight->WorldRotation(), float4(selectedLight->Color(), .5f));
				break;

			case LIGHT_TYPE_POINT:
				Gizmos::DrawWireSphere(selectedLight->WorldPosition(), selectedLight->Radius(), float4(selectedLight->Color(), .5f));
				Gizmos::DrawWireSphere(selectedLight->WorldPosition(), selectedLight->Range(), float4(selectedLight->Color(), .2f));
				break;
			}
		}

		float s = camera->Orthographic() ? .05f : .05f * length(mSelected->WorldPosition() - camera->WorldPosition());
		if (mInput->KeyDown(KEY_LSHIFT)) {
			quaternion r = mSelected->WorldRotation();
			if (Gizmos::RotationHandle("Selected Light Rotation", mInput->GetPointer(0), mSelected->WorldPosition(), r, s)) {
				mSelected->LocalRotation(r);
				change = false;
			}
		} else {
			float3 p = mSelected->WorldPosition();
			if (Gizmos::PositionHandle("Selected Light Position", mInput->GetPointer(0), camera->WorldRotation(), p, s)) {
				mSelected->LocalPosition(p);
				change = false;
			}
		}
	}

	// change selection
	if (change) mSelected = nullptr;
	for (Light* light : mScene->ActiveLights()) {
		float2 lt;
		bool hover = ray.Intersect(Sphere(light->WorldPosition(), .09f), lt);
		hover = hover && lt.x > 0;

		float3 col = light->mEnabled ? light->Color() : light->Color() * .2f;
		Gizmos::DrawBillboard(light->WorldPosition(), hover && light != selectedLight ? .09f : .075f, camera->WorldRotation(), float4(col, 1),
			mScene->AssetManager()->LoadTexture("Assets/Textures/icons.png"), float4(.5f, .5f, 0, 0));

		if (hover && change)
			mSelected = light;
	}
}