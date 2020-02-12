#include <Scene/MeshRenderer.hpp>
#include <Scene/SkinnedMeshRenderer.hpp>
#include <Scene/GUI.hpp>
#include <Util/Profiler.hpp>
#include <Util/Tokenizer.hpp>

#include <Core/EnginePlugin.hpp>
#include <assimp/pbrmaterial.h>

#define THROW_INVALID_SKEL { fprintf_color(COLOR_RED, stderr, "Invalid SKEL file\n"); throw; }
#define THROW_INVALID_SKIN { fprintf_color(COLOR_RED, stderr, "Invalid SKIN file\n"); throw; }
#define THROW_INVALID_ANIM { fprintf_color(COLOR_RED, stderr, "Invalid ANIM file\n"); throw; }

using namespace std;

class SkelJoint : public Bone {
public:
	AABB mBox;
	float3 mRotMin;
	float3 mRotMax;
	float3 mPose;

	inline SkelJoint(const std::string& name, uint32_t index) : Bone(name, index), Object(name), mPose(0), mRotMin(-1e10f), mRotMax(1e10f) {};
};

class MeshView : public EnginePlugin {
private:
	Scene* mScene;
	vector<Object*> mObjects;
	Object* mSelected;

	uint32_t mCurrentBone;
	shared_ptr<Animation> mWaspWalk;
	float mAnimTime;
	bool mPlayAnimation;
	bool mLoopAnimation;

	SkinnedMeshRenderer* mWasp;
	SkinnedMeshRenderer* mHead;

	SkelJoint* ReadJoint(Tokenizer& t, AnimationRig& destRig, const string& name, float scale) {
		shared_ptr<SkelJoint> j = make_shared<SkelJoint>(name, destRig.size());
		mScene->AddObject(j);
		mObjects.push_back(j.get());

		destRig.push_back(j.get());

		string token;
		if (!t.Next(token) || token != "{") THROW_INVALID_SKEL;

			while (t.Next(token)) {
				if (token == "offset") {
					float3 offset;
					if (!t.Next(offset.x)) THROW_INVALID_SKEL;
					if (!t.Next(offset.y)) THROW_INVALID_SKEL;
					if (!t.Next(offset.z)) THROW_INVALID_SKEL;
					offset *= scale;
					offset.z = -offset.z;
					j->LocalPosition(offset);
					continue;
				}
				if (token == "boxmin") {
					if (!t.Next(j->mBox.mMin.x)) THROW_INVALID_SKEL;
					if (!t.Next(j->mBox.mMin.y)) THROW_INVALID_SKEL;
					if (!t.Next(j->mBox.mMin.z)) THROW_INVALID_SKEL;
					j->mBox.mMin *= scale;
					j->mBox.mMin.z = -j->mBox.mMin.z;
					continue;
				}
				if (token == "boxmax") {
					if (!t.Next(j->mBox.mMax.x)) THROW_INVALID_SKEL;
					if (!t.Next(j->mBox.mMax.y)) THROW_INVALID_SKEL;
					if (!t.Next(j->mBox.mMax.z)) THROW_INVALID_SKEL;
					j->mBox.mMax *= scale;
					j->mBox.mMax.z = -j->mBox.mMax.z;
					continue;
				}
				if (token == "rotxlimit") {
					if (!t.Next(j->mRotMin.x)) THROW_INVALID_SKEL;
					if (!t.Next(j->mRotMax.x)) THROW_INVALID_SKEL;
					j->mPose.x = clamp(j->mPose.x, j->mRotMin.x, j->mRotMax.x);
					continue;
				}
				if (token == "rotylimit") {
					if (!t.Next(j->mRotMin.y)) THROW_INVALID_SKEL;
					if (!t.Next(j->mRotMax.y)) THROW_INVALID_SKEL;
					j->mPose.y = clamp(j->mPose.y, j->mRotMin.y, j->mRotMax.y);
					continue;
				}
				if (token == "rotzlimit") {
					if (!t.Next(j->mRotMin.z)) THROW_INVALID_SKEL;
					if (!t.Next(j->mRotMax.z)) THROW_INVALID_SKEL;
					j->mPose.z = clamp(j->mPose.z, j->mRotMin.z, j->mRotMax.z);
					continue;
				}
				if (token == "pose") {
					if (!t.Next(j->mPose.x)) THROW_INVALID_SKEL;
					if (!t.Next(j->mPose.y)) THROW_INVALID_SKEL;
					if (!t.Next(j->mPose.z)) THROW_INVALID_SKEL;
					j->mPose = clamp(j->mPose, j->mRotMin, j->mRotMax);
					quaternion r(j->mPose);
					r.x = -r.x;
					r.y = -r.y;
					j->LocalRotation(r);
					continue;
				}
				if (token == "balljoint") {
					if (!t.Next(token)) THROW_INVALID_SKEL;
					j->AddChild(ReadJoint(t, destRig, token, scale));
					continue;
				}

				if (token == "}") break;
			}

		return j.get();
	}

	shared_ptr<Animation> LoadAnim(const fs::path& filepath) {
		ifstream file(filepath.string());

		Tokenizer t(file, { ' ', '\n', '\r', '\t' });

		float timeStart = 0;
		float timeEnd = 0;

		unordered_map<uint32_t, AnimationChannel> channels;

		AnimationExtrapolate exIn = EXTRAPOLATE_CONSTANT;
		AnimationExtrapolate exOut = EXTRAPOLATE_CONSTANT;

		string token;
		if (!t.Next(token) || token != "animation") THROW_INVALID_ANIM;
		if (!t.Next(token)) THROW_INVALID_ANIM;
		while (t.Next(token)) {
			if (token == "range") {
				if (!t.Next(timeStart)) THROW_INVALID_ANIM;
				if (!t.Next(timeEnd)) THROW_INVALID_ANIM;
			} else if (token == "numchannels") {
				uint32_t cc;
				if (!t.Next(cc)) THROW_INVALID_ANIM;
			} else if (token == "channel") {
				if (!t.Next(token)) THROW_INVALID_ANIM;
			} else if (token == "extrapolate") {
				if (!t.Next(token)) THROW_INVALID_ANIM;
				if (token == "constant") exIn = EXTRAPOLATE_CONSTANT;
				else if (token == "linear") exIn = EXTRAPOLATE_LINEAR;
				else if (token == "cycle") exIn = EXTRAPOLATE_CYCLE;
				else if (token == "cycle_offset") exIn = EXTRAPOLATE_CYCLE_OFFSET;
				else if (token == "bounce") exIn = EXTRAPOLATE_BOUNCE;
				else THROW_INVALID_ANIM;

				if (!t.Next(token)) THROW_INVALID_ANIM;
				if (token == "constant") exOut = EXTRAPOLATE_CONSTANT;
				else if (token == "linear") exOut = EXTRAPOLATE_LINEAR;
				else if (token == "cycle") exOut = EXTRAPOLATE_CYCLE;
				else if (token == "cycle_offset") exOut = EXTRAPOLATE_CYCLE_OFFSET;
				else if (token == "bounce") exOut = EXTRAPOLATE_BOUNCE;
				else THROW_INVALID_ANIM;
			} else if (token == "keys") {
				vector<AnimationKeyframe> keys;
				uint32_t kc;
				if (!t.Next(kc)) THROW_INVALID_ANIM;
				keys.resize(kc);
				if (!t.Next(token)) THROW_INVALID_ANIM;
				for (uint32_t i = 0; i < kc; i++) {
					if (!t.Next(keys[i].mTime)) THROW_INVALID_ANIM;
					if (!t.Next(keys[i].mValue)) THROW_INVALID_ANIM;

					if (!t.Next(token)) THROW_INVALID_ANIM;
					if (token == "flat") keys[i].mTangentModeIn = ANIMATION_TANGENT_FLAT;
					else if (token == "linear") keys[i].mTangentModeIn = ANIMATION_TANGENT_LINEAR;
					else if (token == "smooth") keys[i].mTangentModeIn = ANIMATION_TANGENT_SMOOTH;
					else {
						keys[i].mTangentIn = atof(token.c_str());
						keys[i].mTangentModeIn = ANIMATION_TANGENT_MANUAL;
					}

					if (!t.Next(token)) THROW_INVALID_ANIM;
					if (token == "flat") keys[i].mTangentModeOut = ANIMATION_TANGENT_FLAT;
					else if (token == "linear") keys[i].mTangentModeOut = ANIMATION_TANGENT_LINEAR;
					else if (token == "smooth") keys[i].mTangentModeOut = ANIMATION_TANGENT_SMOOTH;
					else {
						keys[i].mTangentOut = atof(token.c_str());
						keys[i].mTangentModeOut = ANIMATION_TANGENT_MANUAL;
					}
				}
				channels.emplace(channels.size(), AnimationChannel(keys, exIn, exOut));
			}
		}

		return make_shared<Animation>(channels, timeStart, timeEnd);
	}
	
	void LoadSkel(const fs::path& filepath, AnimationRig& destRig, float scale) {
		ifstream file(filepath.string());

		Tokenizer t(file, { ' ', '\n', '\r', '\t' });

		string token;
		if (!t.Next(token) || token != "balljoint") THROW_INVALID_SKEL;
		if (!t.Next(token)) THROW_INVALID_SKEL;
		ReadJoint(t, destRig, token, scale);

		if (t.Next(token)) {
			if (token == "balljoint") {
				fprintf_color(COLOR_RED, stderr, "Multiple root joints!\n");
				throw;
			}
			THROW_INVALID_SKEL;
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
				if (!t.Next(sz)) THROW_INVALID_SKIN;
					if (sz > vertices.size()) vertices.resize(sz);
				for (i = 0; i < sz; i++) {
					if (!t.Next(vertices[i].position.x)) THROW_INVALID_SKIN;
					if (!t.Next(vertices[i].position.y)) THROW_INVALID_SKIN;
					if (!t.Next(vertices[i].position.z)) THROW_INVALID_SKIN;
					vertices[i].position *= scale;
					vertices[i].position.z = -vertices[i].position.z;
				}
			} else if (token == "normals") {
				if (!t.Next(sz)) THROW_INVALID_SKIN;
				if (sz > vertices.size()) vertices.resize(sz);
				for (i = 0; i < sz; i++) {
					if (!t.Next(vertices[i].normal.x)) THROW_INVALID_SKIN;
					if (!t.Next(vertices[i].normal.y)) THROW_INVALID_SKIN;
					if (!t.Next(vertices[i].normal.z)) THROW_INVALID_SKIN;
					vertices[i].normal.z = -vertices[i].normal.z;
					vertices[i].normal = normalize(vertices[i].normal);
				}
			} else if (token == "texcoords") {
				if (!t.Next(sz)) THROW_INVALID_SKIN;
				if (sz > vertices.size()) vertices.resize(sz);
				for (i = 0; i < sz; i++) {
					if (!t.Next(vertices[i].uv.x)) THROW_INVALID_SKIN;
					if (!t.Next(vertices[i].uv.y)) THROW_INVALID_SKIN;
					vertices[i].uv.y = 1 - vertices[i].uv.y;
					vertices[i].tangent = 1;
				}
			} else if (token == "skinweights") {
				if (!t.Next(sz)) THROW_INVALID_SKIN;
				if (sz > weights.size()) weights.resize(sz);
				for (i = 0; i < sz; i++) {
					uint32_t c;
					if (!t.Next(c)) THROW_INVALID_SKIN;
					if (c > 4) { fprintf_color(COLOR_RED, stderr, "More than 4 weights per vertex not supported!\n"); throw; }
					weights[i].Weights = 0;
					for (uint32_t j = 0; j < c; j++) {
						if (!t.Next(weights[i].Indices[j])) THROW_INVALID_SKIN;
						if (!t.Next(weights[i].Weights[j])) THROW_INVALID_SKIN;
					}
				}
			} else if (token == "triangles") {
				if (!t.Next(sz)) THROW_INVALID_SKIN;
				if (sz * 3 > indices.size()) indices.resize(sz * 3);
				for (i = 0; i < sz; i++) {
					if (!t.Next(indices[3 * i + 0])) THROW_INVALID_SKIN;
					if (!t.Next(indices[3 * i + 1])) THROW_INVALID_SKIN;
					if (!t.Next(indices[3 * i + 2])) THROW_INVALID_SKIN;
				}
			} else if (token == "bindings") {
				if (!t.Next(sz)) THROW_INVALID_SKIN;
				if (sz != destRig.size()) THROW_INVALID_SKIN;
				for (i = 0; i < sz; i++) {
					if (!t.Next(token)) THROW_INVALID_SKIN;
					if (token != "matrix") THROW_INVALID_SKIN;

					Bone* j = destRig[i];
					j->mInverseBind = float4x4(1);
					if (!t.Next(j->mInverseBind[0][0])) THROW_INVALID_SKIN;
					if (!t.Next(j->mInverseBind[0][1])) THROW_INVALID_SKIN;
					if (!t.Next(j->mInverseBind[0][2])) THROW_INVALID_SKIN;

					if (!t.Next(j->mInverseBind[1][0])) THROW_INVALID_SKIN;
					if (!t.Next(j->mInverseBind[1][1])) THROW_INVALID_SKIN;
					if (!t.Next(j->mInverseBind[1][2])) THROW_INVALID_SKIN;

					if (!t.Next(j->mInverseBind[2][0])) THROW_INVALID_SKIN;
					if (!t.Next(j->mInverseBind[2][1])) THROW_INVALID_SKIN;
					if (!t.Next(j->mInverseBind[2][2])) THROW_INVALID_SKIN;

					if (!t.Next(j->mInverseBind[3][0])) THROW_INVALID_SKIN;
					if (!t.Next(j->mInverseBind[3][1])) THROW_INVALID_SKIN;
					if (!t.Next(j->mInverseBind[3][2])) THROW_INVALID_SKIN;

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

		if (calcNormals) {
			for (i = 0; i < vertices.size(); i++)
				vertices[i].normal = 0;
			for (i = 0; i < indices.size(); i += 3) {
				float3 n = normalize(cross(
					normalize(vertices[indices[i + 2]].position - vertices[indices[i]].position),
					normalize(vertices[indices[i + 1]].position - vertices[indices[i]].position)));
				vertices[indices[i + 0]].normal += n;
				vertices[indices[i + 1]].normal += n;
				vertices[indices[i + 2]].normal += n;
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
						if (!mt.Next(sz)) THROW_INVALID_SKIN;
						for (i = 0; i < sz; i++) {
							uint32_t j;
							if (!mt.Next(j)) THROW_INVALID_SKIN;
							if (!mt.Next(shape[j].position.x)) THROW_INVALID_SKIN;
							if (!mt.Next(shape[j].position.y)) THROW_INVALID_SKIN;
							if (!mt.Next(shape[j].position.z)) THROW_INVALID_SKIN;
							shape[j].position *= scale;
							shape[j].position.z = -shape[j].position.z;
						}
					} else if (token == "normals") {
						if (!mt.Next(sz)) THROW_INVALID_SKIN;
							for (i = 0; i < sz; i++) {
								uint32_t j;
								if (!mt.Next(j)) THROW_INVALID_SKIN;
								if (!mt.Next(shape[j].normal.x)) THROW_INVALID_SKIN;
								if (!mt.Next(shape[j].normal.y)) THROW_INVALID_SKIN;
								if (!mt.Next(shape[j].normal.z)) THROW_INVALID_SKIN;
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

	MouseKeyboardInput* mInput;

public:
	PLUGIN_EXPORT MeshView() 
		: mScene(nullptr), mSelected(nullptr), mInput(nullptr), mAnimTime(0.f), mHead(nullptr), mWasp(nullptr),
		mCurrentBone(0xFFFFFFFF), mPlayAnimation(false), mLoopAnimation(true) {
		mEnabled = true;
	}
	PLUGIN_EXPORT ~MeshView() {
		for (Object* obj : mObjects)
			mScene->RemoveObject(obj);
	}

	PLUGIN_EXPORT bool Init(Scene* scene) override {
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

		return true;
	}

	PLUGIN_EXPORT void Update() override {
		if (mHead) {
			mAnimTime = clamp(mAnimTime, 0.f, 1.f);
			mHead->ShapeKey("head1", mAnimTime);
			mHead->ShapeKey("head2", 1.f - mAnimTime);
		}
		if (mWasp) {
			if (mPlayAnimation) mAnimTime += mScene->Instance()->DeltaTime();
			if (mLoopAnimation) if (mAnimTime > mWaspWalk->TimeEnd()) mAnimTime -= (mWaspWalk->TimeStart() + mWaspWalk->TimeEnd());
			
			mWaspWalk->Sample(mAnimTime, mWasp->Rig());
		}
	}

	PLUGIN_EXPORT void PreRenderScene(CommandBuffer* commandBuffer, Camera* camera, PassType pass) override {
		if (pass != PASS_MAIN || camera != mScene->Cameras()[0]) return;
		Font* sem11 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-SemiBold.ttf", 11);
		Font* sem16 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-SemiBold.ttf", 16);
		Font* reg14 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Regular.ttf", 14);
		Font* bld24 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Bold.ttf", 24);
	
		GUI::BeginScreenLayout(LAYOUT_VERTICAL, fRect2D(10, camera->FramebufferHeight()/2 - 200, 250, 400), float4(.2f, .2f, .2f, 1), 10);

		GUI::LayoutLabel(bld24, "MeshView", 24, 20, 0, 1, 4);
		GUI::LayoutSeparator(1, .5f);

		if (GUI::LayoutButton(sem16, "Wasp", 16, 20, .5f, 1)) {
			if (mHead) mHead->mVisible = false;
			if (mWasp) { mWasp->mVisible = true; return; }

			auto waspMat = make_shared<Material>("Wasp", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
			waspMat->SetParameter("Color", float4(1));
			waspMat->SetParameter("Metallic", 0.f);
			waspMat->SetParameter("Roughness", .5f);
			waspMat->SetParameter("Emission", float3(0));

			AnimationRig waspRig;
			LoadSkel("Assets/Models/wasp/wasp.skel", waspRig, 1);

			waspRig[0]->LocalPosition(0, 1, 0);

			auto wasp = make_shared<SkinnedMeshRenderer>("Wasp");
			wasp->Mesh(LoadSkin("Assets/Models/wasp/wasp.skin", waspRig, 1, true));
			wasp->Rig(waspRig);
			wasp->Material(waspMat);
			wasp->PushConstant("TextureIndex", 0u);
			mScene->AddObject(wasp);
			mObjects.push_back(wasp.get());
			waspRig[0]->AddChild(wasp.get());
			mWasp = wasp.get();

			mWaspWalk = LoadAnim("Assets/Models/wasp/wasp_walk.anim");
		}
		if (GUI::LayoutButton(sem16, "Head", 16, 20, .5f, 1)) {
			if (mWasp) mWasp->mVisible = false;
			if (mHead) { mHead->mVisible = true; return; }

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

			headRig[0]->LocalPosition(0, 1, 0);
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
		}

		GUI::EndLayout();

		if (mWaspWalk) {
			fRect2D r = GUI::BeginScreenLayout(LAYOUT_HORIZONTAL, fRect2D(0, 0, camera->FramebufferWidth(), 200), float4(.2f, .2f, .2f, 1), 2);
			
			GUI::LayoutSpace(10);

			GUI::BeginSubLayout(LAYOUT_VERTICAL, 150, 0, 0, 0);
			GUI::BeginScrollSubLayout(196, mWasp->Rig().size() * 24, float4(.2f, .2f, .2f, 1), 0, 2);
			
			for (Bone* b : mWasp->Rig())
				if (GUI::LayoutButton(sem16, b->mName, 16, 20, mCurrentBone == b->mBoneIndex ? float4(.5f, .5f, .5f, 1) : float4(.25f, .25f, .25f, 1), 1, 2))
					mCurrentBone = b->mBoneIndex;
			GUI::EndLayout();
			GUI::EndLayout();

			GUI::LayoutSpace(2);

			r = GUI::BeginSubLayout(LAYOUT_VERTICAL, r.mExtent.x - 162, float4(.2f, .2f, .2f, 1), 0);
			GUI::BeginSubLayout(LAYOUT_HORIZONTAL, 24, float4(.2f, .2f, .2f, 1), 0);
			if (GUI::LayoutButton(sem16, mPlayAnimation ? "Stop" : "Play", 16, 80, float4(.25f, .25f, .25f, 1), 1, 2))
				mPlayAnimation = !mPlayAnimation;
			if (GUI::LayoutButton(sem16, "Loop", 16, 80, mLoopAnimation ?  float4(.4f, .4f, .4f, 1) : float4(.25f, .25f, .25f, 1), 1, 2))
				mLoopAnimation = !mLoopAnimation;

			static const float4 channelColors[3]{
				float4(1, .5f, .5f, 1),
				float4(.5f, 1, .5f, 1),
				float4(.5f, .5f, 1, 1),
			};
			static const float4 channelColorsDark[3]{
				float4(1, .25f, .25f, 1),
				float4(.25f, 1, .25f, 1),
				float4(.25f, .25f, 1, 1),
			};

			// extrapolation modes
			for (uint32_t j = 0; j < 3; j++) {
				if (mWaspWalk->Channels().count(mCurrentBone*3)) {
					const AnimationChannel& c = mWaspWalk->Channels().at(mCurrentBone*3);
					string e = "";
					switch (c.ExtrapolateIn()){
						case EXTRAPOLATE_CONSTANT: 		e = "Constant"; break;
						case EXTRAPOLATE_LINEAR: 		e = "Linear"; break;
						case EXTRAPOLATE_CYCLE: 		e = "Cycle"; break;
						case EXTRAPOLATE_CYCLE_OFFSET: 	e = "Cycle Offset"; break;
						case EXTRAPOLATE_BOUNCE: 		e = "Bounce"; break;
					}
					switch (c.ExtrapolateOut()){
						case EXTRAPOLATE_CONSTANT: 		e += "/Constant"; break;
						case EXTRAPOLATE_LINEAR: 		e += "/Linear"; break;
						case EXTRAPOLATE_CYCLE: 		e += "/Cycle"; break;
						case EXTRAPOLATE_CYCLE_OFFSET: 	e += "/Cycle Offset"; break;
						case EXTRAPOLATE_BOUNCE: 		e += "/Bounce"; break;
					}
					GUI::LayoutLabel(sem16, e, 16, 120, 0, channelColors[j], 2);
				}
			}
			
			GUI::EndLayout();
			GUI::EndLayout();

			// curve window background
			r.mExtent.y -= 24;
			GUI::Rect(r, float4(.1f, .1f, .1f, 1));

			float tmx = mWaspWalk->TimeEnd();
			float tmn = mWaspWalk->TimeStart();
			float vmx = 0;
			float vmn = 0;

			// compute curve range
			for (uint32_t j = 0; j < 3; j++) {
				if (mWaspWalk->Channels().count(mCurrentBone*3 + j)) {
					const AnimationChannel& c = mWaspWalk->Channels().at(mCurrentBone*3 + j);
					for (uint32_t i = 0; i < c.KeyframeCount(); i++){
						tmx = fmaxf(tmx, c.Keyframe(i).mTime);
						tmn = fminf(tmn, c.Keyframe(i).mTime);
						vmx = fmaxf(vmx, c.Keyframe(i).mValue);
						vmn = fminf(vmn, c.Keyframe(i).mValue);
					}
					tmx += (tmx - tmn) * .1f;
					tmn -= (tmx - tmn) * .1f;
					vmx += (vmx - vmn) * .1f;
					vmn -= (vmx - vmn) * .1f;
				}
			}

			// scrub animation
			if (!mPlayAnimation) {
				float2 c = mInput->CursorPos();
				c.y = mInput->WindowHeight() - c.y;
				if (r.Contains(c) && mInput->KeyDown(MOUSE_LEFT))
					mAnimTime = tmn + (tmx - tmn) * (c.x - r.mOffset.x) / r.mExtent.x;
			}

			// animation window background
			fRect2D bgrect(r.mOffset + float2(r.mExtent.x * (mWaspWalk->TimeStart() - tmn) / (tmx - tmn), 0), float2(r.mExtent.x * (mWaspWalk->TimeEnd()) / (tmx - tmn), r.mExtent.y));
			GUI::Rect(bgrect, float4(.15f, .15f, .15f, 1));
			GUI::Rect(fRect2D(bgrect.mOffset, float2(1, r.mExtent.y)), float4(1, 1, 1, 1));
			GUI::Rect(fRect2D(bgrect.mOffset + float2(bgrect.mExtent.x, 0), float2(1, r.mExtent.y)), float4(1, 1, 1, 1));
			// center line
			GUI::Rect(fRect2D(r.mOffset + r.mExtent * float2(0, (0 - vmn) / (vmx - vmn)), float2(r.mExtent.x, 1)), float4(1, 1, 1, .3f));

			for (uint32_t j = 0; j < 3; j++) {
				if (mWaspWalk->Channels().count(mCurrentBone*3 + j)) {
					const AnimationChannel& c = mWaspWalk->Channels().at(mCurrentBone*3 + j);
					// boxes and tangents
					for (uint32_t i = 0; i < c.KeyframeCount(); i++){
						float2 p(c.Keyframe(i).mTime, c.Keyframe(i).mValue);
						p.x = (p.x - tmn) / (tmx - tmn);
						p.y = (p.y - vmn) / (vmx - vmn);
						
						float2 tangents[3];
						tangents[0] = -normalize(float2(1, c.Keyframe(i).mTangentIn)) * .25f;
						tangents[1] = 0;
						tangents[2] =  normalize(float2(1, c.Keyframe(i).mTangentOut)) * .25f;
						GUI::DrawScreenLine(tangents, 3, 2.5f, r.mOffset + p*r.mExtent, r.mExtent / float2(tmx - tmn, vmx - vmn), channelColors[j]*2);
					}
					
					vector<float2> points;
					for (float t = tmn; t < tmx; t += (tmx - tmn) / 2048.f){
						float2 tp(t, c.Sample(t));
						tp.x = (tp.x - tmn) / (tmx - tmn);
						tp.y = (tp.y - vmn) / (vmx - vmn);
						points.push_back(tp);
					}
					if (points.size()) GUI::DrawScreenLine(points.data(), points.size(), 2, r.mOffset, r.mExtent, channelColorsDark[j]);

					float2 curEval = r.mOffset + float2(2 + r.mExtent.x * (mAnimTime - tmn) / (tmx - tmn), r.mExtent.y*(c.Sample(mAnimTime) - vmn) / (vmx - vmn));
					GUI::DrawString(sem11, to_string(c.Sample(mAnimTime)), channelColors[j], curEval, 11.f, TEXT_ANCHOR_MIN, TEXT_ANCHOR_MID);
				}
			}
			
			// scrub bar
			GUI::Rect(fRect2D(r.mOffset.x + r.mExtent.x * (mAnimTime - tmn) / (tmx - tmn), r.mOffset.y, 1, r.mExtent.y), float4(1, 1, 1, .2f));

			GUI::EndLayout();
		}

	}
	
	PLUGIN_EXPORT void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
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


		if (mCurrentBone < mWasp->Rig().size()) {
			Bone* b = mWasp->Rig()[mCurrentBone];
			if (mCurrentBone == 0) {
				float3 r = b->WorldPosition();
				if (Gizmos::PositionHandle("Channel Position", mInput->GetPointer(0), b->WorldPosition(), r, .1f))
					b->LocalPosition(b->Parent() ? (b->Parent()->WorldToObject() * float4(r, 1)).xyz : r);
			} else {
				quaternion r = b->WorldRotation();
				if (Gizmos::RotationHandle("Channel Rotation", mInput->GetPointer(0), b->WorldPosition(), r, .1f))
					b->LocalRotation(b->Parent() ? inverse(b->Parent()->WorldRotation()) * r : r);
			}
		}
	}
};

ENGINE_PLUGIN(MeshView)