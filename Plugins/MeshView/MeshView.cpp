#include <Scene/ClothRenderer.hpp>
#include <Scene/SkinnedMeshRenderer.hpp>
#include <Scene/GUI.hpp>
#include <Util/Profiler.hpp>
#include <Util/Tokenizer.hpp>

#include <Core/EnginePlugin.hpp>
#include <assimp/pbrmaterial.h>

using namespace std;

struct IKJoint {
	Object* mObject;
	uint32_t mTwistAxis;
	float3 mCurrent;
	float3 mMin;
	float3 mMax;
	float mTwistPenalty;
};

float WrapAngle(float a) {
	return fmodf(a + PI, 2 * PI) - PI;
}

float3 ForwardKinematics(vector<IKJoint>& chain) {
	return chain[chain.size() - 1].mObject->WorldPosition();
}
float3 PartialGradient(vector<IKJoint>& chain, const float3& goal, const float3& cur, uint32_t joint, float increment) {
	const IKJoint& j = chain[joint];
	float3 curLocal = (j.mObject->WorldToObject() * float4(cur, 1)).xyz;

	float3 r[3];
	r[0] = clamp(j.mCurrent + float3(increment, 0, 0), j.mMin, j.mMax);
	r[1] = clamp(j.mCurrent + float3(0, increment, 0), j.mMin, j.mMax);
	r[2] = clamp(j.mCurrent + float3(0, 0, increment), j.mMin, j.mMax);

	for (uint32_t i = 0; i < 3; i++) r[i].x = WrapAngle(r[i].x);
	for (uint32_t i = 0; i < 3; i++) r[i].y = WrapAngle(r[i].y);
	for (uint32_t i = 0; i < 3; i++) r[i].z = WrapAngle(r[i].z);

	float4x4 m[3];
	if (j.mObject->Parent()) {
		m[0] = j.mObject->Parent()->ObjectToWorld();
		m[1] = j.mObject->Parent()->ObjectToWorld();
		m[2] = j.mObject->Parent()->ObjectToWorld();
	} else {
		m[0] = float4x4(1);
		m[1] = float4x4(1);
		m[2] = float4x4(1);
	}
	m[0] = m[0] * float4x4::TRS(j.mObject->LocalPosition(), quaternion(r[0]), j.mObject->LocalScale());
	m[1] = m[1] * float4x4::TRS(j.mObject->LocalPosition(), quaternion(r[1]), j.mObject->LocalScale());
	m[2] = m[2] * float4x4::TRS(j.mObject->LocalPosition(), quaternion(r[2]), j.mObject->LocalScale());

	float3 p[3];
	p[0] = (m[0] * float4(curLocal, 1)).xyz;
	p[1] = (m[1] * float4(curLocal, 1)).xyz;
	p[2] = (m[2] * float4(curLocal, 1)).xyz;

	float3 inc(
		length(p[0] - goal) + fabs(r[0][j.mTwistAxis]) * j.mTwistPenalty,
		length(p[1] - goal) + fabs(r[1][j.mTwistAxis]) * j.mTwistPenalty,
		length(p[2] - goal) + fabs(r[2][j.mTwistAxis]) * j.mTwistPenalty );

	float c = length(cur - goal) + fabs(j.mCurrent[j.mTwistAxis]) * j.mTwistPenalty;
	return (inc - c) / increment;
}
void EvaluateChain(vector<IKJoint>& chain, const float3& goal, float increment = .00001f, float rate = .0005f, float thresh = .00001f) {
	float twist = 0;
	for (uint32_t i = 0; i < chain.size(); i++) {
		twist += fabs(chain[i].mCurrent[chain[i].mTwistAxis]) * chain[i].mTwistPenalty;
		float3 cur = ForwardKinematics(chain);
		if (length(cur - goal) + twist < thresh) break;

		float3 gradient = PartialGradient(chain, goal, cur, i, increment);

		chain[i].mCurrent = clamp(chain[i].mCurrent - gradient * rate, chain[i].mMin, chain[i].mMax);
		for (uint32_t j = 0; j < 3; j++)
			chain[i].mCurrent[j] = WrapAngle(chain[i].mCurrent[j]);
		chain[i].mObject->LocalRotation(quaternion(chain[i].mCurrent));
	}
}

class MeshView : public EnginePlugin {
private:
	Scene* mScene;
	vector<Object*> mObjects;
	Object* mSelected;

	Object* mArmBase;
	vector<IKJoint> mIKChain;
	float3 mIKTarget;

	Ray ray;

	MouseKeyboardInput* mInput;

public:
	PLUGIN_EXPORT MeshView() : mScene(nullptr), mSelected(nullptr), mInput(nullptr), mArmBase(nullptr) {
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
		mScene->Environment()->AmbientLight(.6f);
		mScene->Environment()->EnvironmentTexture(mScene->AssetManager()->LoadTexture("Assets/Textures/photo_studio_01_2k.hdr"));

		auto gridMat = make_shared<Material>("Plane", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
		gridMat->EnableKeyword("TEXTURED");
		gridMat->SetParameter("MainTextures", 0, mScene->AssetManager()->LoadTexture("Assets/Textures/grid.png"));
		gridMat->SetParameter("NormalTextures", 0, mScene->AssetManager()->LoadTexture("Assets/Textures/bump.png"));
		gridMat->SetParameter("MaskTextures", 0, mScene->AssetManager()->LoadTexture("Assets/Textures/mask.png"));
		gridMat->SetParameter("Color", float4(1));
		gridMat->SetParameter("Metallic", 0.f);
		gridMat->SetParameter("Roughness", .5f);
		gridMat->SetParameter("BumpStrength", 1.f);
		gridMat->SetParameter("Emission", float3(0));

		auto plane = make_shared<MeshRenderer>("Plane");
		plane->Mesh(shared_ptr<Mesh>(Mesh::CreatePlane("Plane", mScene->Instance()->Device(), 128.f)));
		plane->Material(gridMat);
		plane->PushConstant("TextureST", float4(256, 256, 1, 1));
		plane->PushConstant("TextureIndex", 0u);
		plane->LocalRotation(quaternion(float3(-PI / 2, 0, 0)));
		mScene->AddObject(plane);
		mObjects.push_back(plane.get());
		dynamic_cast<Object*>(plane.get())->LayerMask(plane->LayerMask() | 0x1);

		auto boxmesh = shared_ptr<Mesh>(Mesh::CreateCube("Box", mScene->Instance()->Device(), .5f));

		auto box = make_shared<MeshRenderer>("Box");
		box->Mesh(boxmesh);
		box->Material(gridMat);
		box->PushConstant("TextureST", float4(1, 1, 1, 1));
		box->PushConstant("TextureIndex", 0u);
		box->LocalPosition(-1, 1, -1);
		box->LocalScale(1, 2, 1);
		box->LocalRotation(quaternion(float3(0, PI / 4, 0)));
		mScene->AddObject(box);
		mObjects.push_back(box.get());
		dynamic_cast<Object*>(box.get())->LayerMask(0x1);

		auto box2 = make_shared<MeshRenderer>("Box 2");
		box2->Mesh(boxmesh);
		box2->Material(gridMat);
		box2->PushConstant("TextureST", float4(1, 1, 1, 1));
		box2->PushConstant("TextureIndex", 0u);
		box2->LocalPosition(1.15f, .25f, .5f);
		box2->LocalScale(1, .5f, 1);
		box2->LocalRotation(quaternion(float3(0, PI / 4, PI / 4)));
		mScene->AddObject(box2);
		mObjects.push_back(box2.get());
		dynamic_cast<Object*>(box2.get())->LayerMask(0x1);

		auto suzanne = make_shared<MeshRenderer>("Suzanne");
		suzanne->Mesh(mScene->AssetManager()->LoadMesh("Assets/Models/suzanne.fbx", .005f));
		suzanne->Material(gridMat);
		suzanne->PushConstant("TextureST", float4(1, 1, 1, 1));
		suzanne->PushConstant("TextureIndex", 0u);
		suzanne->LocalPosition(0, .3f, 1.f);
		mScene->AddObject(suzanne);
		mObjects.push_back(suzanne.get());
		dynamic_cast<Object*>(suzanne.get())->LayerMask(0x1);
		return true;
	}

	PLUGIN_EXPORT void Update(CommandBuffer* commandBuffer) override {
		if (mArmBase && mArmBase->mEnabled) {
			Camera* camera = mScene->Cameras()[0];
			if (camera && mInput->KeyDown(MOUSE_LEFT))
				ray = camera->ScreenToWorldRay(mInput->CursorPos() / float2(camera->FramebufferWidth(), camera->FramebufferHeight()));
			PROFILER_BEGIN("Raycast");
			float t;
			if (mScene->Raycast(ray, &t, false, 0x1))
				mIKTarget = ray.mOrigin + ray.mDirection * t;
			PROFILER_END;
			
			PROFILER_BEGIN("IK");
			for (uint32_t i = 0; i < 512; i++)
				EvaluateChain(mIKChain, mIKTarget);
			PROFILER_END;
		}
		
	}

	PLUGIN_EXPORT void PreRenderScene(CommandBuffer* commandBuffer, Camera* camera, PassType pass) override {
		if (pass != PASS_MAIN || camera != mScene->Cameras()[0]) return;
		Font* sem11 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-SemiBold.ttf", 11);
		Font* sem16 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-SemiBold.ttf", 16);
		Font* reg14 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Regular.ttf", 14);
		Font* bld24 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Bold.ttf", 24);
	
		GUI::BeginScreenLayout(LAYOUT_VERTICAL, fRect2D(10, camera->FramebufferHeight()/2 - 300, 250, 600), float4(.2f, .2f, .2f, 1), 10);

		GUI::LayoutLabel(bld24, "MeshView", 24, 20, 0, 1, 4);
		GUI::LayoutSeparator(1, .5f);

		/*
		vector<Object*> all = mScene->Objects();
		
		GUI::BeginScrollSubLayout(100, all.size() * 20, float4(.15f, .15f, .15f, 1));
		stack<pair<string, Object*>> objstack;
		for (uint32_t i = 0; i < all.size(); i++) if (all[i]->Parent() == nullptr) objstack.push(make_pair("", all[i]));
		while (objstack.size()) {
			auto o = objstack.top();
			objstack.pop();
			
			if (GUI::LayoutButton(reg14, o.first + o.second->mName, 14, 16, mSelected == o.second ? float4(.25f, .25f, .25f, 1) : float4(.15f, .15f, .15f, 1), 1, 2.f, TEXT_ANCHOR_MIN))
				if (mSelected == o.second)
					mSelected = nullptr;
				else
					 mSelected = o.second;

			for (uint32_t i = 0; i < o.second->ChildCount(); i++)
				objstack.push(make_pair(o.first + "   ", o.second->Child(i)));
		}
		GUI::EndLayout();
		*/
		if (GUI::LayoutButton(sem16, "Load Arm", 16, 20, .5f, 1)) {
			if (mArmBase) { mArmBase->mEnabled = !mArmBase->mEnabled; return; }
			
			auto mat = make_shared<Material>("Arm", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
			mat->SetParameter("Color", float4(1));
			mat->SetParameter("Metallic", 1.f);
			mat->SetParameter("Roughness", 1.f);
			mat->SetParameter("Emission", float3(0));

			float twistPenalty = .001f;

			unordered_map<string, IKJoint> jointMap {
				{ "Yaw",   { nullptr, 1, float3(0), float3(0, -1e10f, 0), float3(0, 1e10f, 0), 0 } },
				{ "Arm0",  { nullptr, 1, float3(0), float3(-PI * .4f, 0, 0), float3(PI * .4f, 0, 0), 0 } },
				{ "Arm1",  { nullptr, 1, float3(0), float3(0, -1e10f, 0), float3(0, 1e10f, 0), twistPenalty } },
				{ "Arm2",  { nullptr, 1, float3(0), float3(-PI * .4f, 0, 0), float3(PI * .4f, 0, 0), 0 } },
				{ "Arm3",  { nullptr, 1, float3(0), float3(-PI * .4f, 0, 0), float3(PI * .4f, 0, 0), 0 } },
				{ "Tip",   { nullptr, 1, float3(0), float3(0, -1e10f, 0), float3(0, 1e10f, 0), twistPenalty } },
			};

			auto matFunc = [&](Scene* scene, aiMaterial* aimat) { return mat; };
			auto objFunc = [&](Scene* scene, Object* object, aiMaterial* aimaterial) {
				if (jointMap.count(object->mName)) {
					IKJoint j = jointMap.at(object->mName);
					j.mObject = object;
					mIKChain.push_back(j);
				}

				MeshRenderer* renderer = dynamic_cast<MeshRenderer*>(object);
				if (!renderer) return;

				aiColor4D baseColor(1);
				float metallic = 1.f;
				float roughness = 1.f;

				aimaterial->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_FACTOR, baseColor);
				aimaterial->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, metallic);
				aimaterial->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, roughness);

				renderer->PushConstant("Color", float4(baseColor.r, baseColor.g, baseColor.b, baseColor.a));
				renderer->PushConstant("Roughness", roughness);
				renderer->PushConstant("Metallic", metallic);
			};

			mArmBase = mScene->LoadModelScene("Assets/Models/arm.gltf", matFunc, objFunc, 1.f, 0, 0, 0);
			auto tip = make_shared<Object>("IKTip");
			tip->LocalPosition(0, .252f, 0);
			mScene->AddObject(tip);
			mObjects.push_back(tip.get());

			mIKChain[mIKChain.size() - 1].mObject->AddChild(tip.get());
			mIKChain.push_back({ tip.get(), 0, 0, 0});

			queue<Object*> nodes;
			nodes.push(mArmBase);
			while (nodes.size()) {
				Object* o = nodes.front();
				nodes.pop();
				for (uint32_t i = 0; i < o->ChildCount(); i++)
					nodes.push(o->Child(i));

				mObjects.push_back(o);
			}
		}

		if (mArmBase && mArmBase->mEnabled) {
			char buf[1024];

			for (uint32_t i = 0; i < mIKChain.size(); i++) {
				sprintf(buf, "%s: %.2f  %.2f  %.2f %.2f", mIKChain[i].mObject->mName.c_str(), mIKChain[i].mCurrent.x, mIKChain[i].mCurrent.y, mIKChain[i].mCurrent.z, fabs(mIKChain[i].mCurrent[mIKChain[i].mTwistAxis]) * mIKChain[i].mTwistPenalty);
				GUI::LayoutLabel(sem16, buf, 16, 18, 0, 1, 2.f, TEXT_ANCHOR_MIN);
			}
		}
		GUI::EndLayout();
	}
	
	PLUGIN_EXPORT void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
		Gizmos::DrawWireSphere(mIKTarget, .02f, float4(.3f, 1, .3f, 1));

		// manipulate selection
		if (mSelected) {
			Light* selectedLight = dynamic_cast<Light*>(mSelected);
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
				if (Gizmos::RotationHandle("Selected Rotation", mInput->GetPointer(0), mSelected->WorldPosition(), r, s)) {
					mSelected->LocalRotation(mSelected->Parent() ? inverse(mSelected->Parent()->WorldRotation()) * r : r);
				}
			} else {
				float3 p = mSelected->WorldPosition();
				if (Gizmos::PositionHandle("Selected Position", mInput->GetPointer(0), camera->WorldRotation(), p, s)) {
					mSelected->LocalPosition(mSelected->Parent() ? (mSelected->Parent()->WorldToObject() * float4(p, 1)).xyz : p);
				}
			}
		}
	}
};

ENGINE_PLUGIN(MeshView)