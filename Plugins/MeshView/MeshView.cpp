#include <Scene/ClothRenderer.hpp>
#include <Scene/SkinnedMeshRenderer.hpp>
#include <Scene/GUI.hpp>
#include <Util/Profiler.hpp>
#include <Util/Tokenizer.hpp>

#include <Core/EnginePlugin.hpp>
#include <assimp/pbrmaterial.h>

using namespace std;

class MeshView : public EnginePlugin {
private:
	Scene* mScene;
	vector<Object*> mObjects;
	Object* mSelected;

	MouseKeyboardInput* mInput;

public:
	PLUGIN_EXPORT MeshView() : mScene(nullptr), mSelected(nullptr), mInput(nullptr) {
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

		return true;
	}

	PLUGIN_EXPORT void Update(CommandBuffer* commandBuffer) override {

	}

	PLUGIN_EXPORT void PreRenderScene(CommandBuffer* commandBuffer, Camera* camera, PassType pass) override {
		if (pass != PASS_MAIN || camera != mScene->Cameras()[0]) return;
		Font* sem11 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-SemiBold.ttf", 11);
		Font* sem16 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-SemiBold.ttf", 16);
		Font* reg14 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Regular.ttf", 14);
		Font* bld24 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Bold.ttf", 24);
	
		/*
		GUI::BeginScreenLayout(LAYOUT_VERTICAL, fRect2D(10, camera->FramebufferHeight()/2 - 300, 250, 600), float4(.2f, .2f, .2f, 1), 10);

		GUI::LayoutLabel(bld24, "MeshView", 24, 20, 0, 1, 4);
		GUI::LayoutSeparator(1, .5f);

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
		GUI::EndLayout();
		*/
	}
	
	PLUGIN_EXPORT void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
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