#include <Scene/ClothRenderer.hpp>
#include <Scene/SkinnedMeshRenderer.hpp>
#include <Scene/GUI.hpp>
#include <Util/Profiler.hpp>
#include <Util/Tokenizer.hpp>

#include <Core/EnginePlugin.hpp>

using namespace std;

class BRDFView : public EnginePlugin {
private:
	Scene* mScene;
	vector<Object*> mObjects;

	float mMetallic;
	float mSpecular;
	float mAnisotropy;
	float mRoughness;
	float mSpecularTint;
	float mSheenTint;
	float mSheen;
	float mClearcoatGloss;
	float mClearcoat;
	float mSubsurface;
	float mTransmission;

	float mTheta;
	float mPhi;

	Material* mMaterial;

	MouseKeyboardInput* mInput;

public:
	PLUGIN_EXPORT BRDFView() : mScene(nullptr), mInput(nullptr),
		mMetallic(0), mSpecular(.5f), mAnisotropy(0), mRoughness(0.2f),
		mSpecularTint(0), mSheenTint(0), mSheen(0), mClearcoatGloss(0), mClearcoat(0),
		mSubsurface(0), mTransmission(0), mTheta(0), mPhi(PI/4) {
		mEnabled = true;
	}
	PLUGIN_EXPORT ~BRDFView() {
		for (Object* obj : mObjects)
			mScene->RemoveObject(obj);
	}

	PLUGIN_EXPORT bool Init(Scene* scene) override {
		mScene = scene;
		mInput = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

		mScene->Environment()->EnableCelestials(false);
		mScene->Environment()->EnableScattering(false);
		mScene->Environment()->AmbientLight(.1f);

		auto mat = make_shared<Material>("BRDFVis", mScene->AssetManager()->LoadShader("Shaders/brdf_graph.stm"));
		mMaterial = mat.get();

		auto sphere = make_shared<MeshRenderer>("Sphere");
		sphere->Mesh(mScene->AssetManager()->LoadMesh("Assets/Models/sphere.fbx", .01f));
		sphere->Material(mat);
		mScene->AddObject(sphere);
		mObjects.push_back(sphere.get());


		return true;
	}

	PLUGIN_EXPORT void PreRenderScene(CommandBuffer* commandBuffer, Camera* camera, PassType pass) override {
		if (pass != PASS_MAIN || camera != mScene->Cameras()[0]) return;
		Font* sem11 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-SemiBold.ttf", 11);
		Font* sem16 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-SemiBold.ttf", 16);
		Font* reg14 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Regular.ttf", 14);
		Font* bld24 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Bold.ttf", 24);
	
		GUI::BeginScreenLayout(LAYOUT_VERTICAL, fRect2D(10, camera->FramebufferHeight()/2 - 300, 250, 600), float4(.2f, .2f, .2f, 1), 10);

		GUI::LayoutLabel(bld24, "BRDFView", 24, 20, 0, 1, 4);

		GUI::LayoutLabel(sem16, "Theta:", 16, 18, 0, 1);
		GUI::LayoutSlider(mTheta, -PI / 2, PI / 2, 16, float4(.3f, .3f, .3f, 1));
		GUI::LayoutLabel(sem16, "Phi:", 16, 18, 0, 1);
		GUI::LayoutSlider(mPhi, 0, PI / 2, 16, float4(.3f, .3f, .3f, 1));

		GUI::LayoutSeparator(.5f, 1);

		GUI::LayoutLabel(sem16, "Metallic:", 16, 18, 0, 1, 2, TEXT_ANCHOR_MIN);
		GUI::LayoutSlider(mMetallic, 0, 1, 16, float4(.3f, .3f, .3f, 1));

		GUI::LayoutLabel(sem16, "Specular:", 16, 18, 0, 1, 2, TEXT_ANCHOR_MIN);
		GUI::LayoutSlider(mSpecular, 0, 1, 16, float4(.3f, .3f, .3f, 1));

		GUI::LayoutLabel(sem16, "Anisotropy:", 16, 18, 0, 1, 2, TEXT_ANCHOR_MIN);
		GUI::LayoutSlider(mAnisotropy, 0, 1, 16, float4(.3f, .3f, .3f, 1));

		GUI::LayoutLabel(sem16, "Roughness:", 16, 18, 0, 1, 2, TEXT_ANCHOR_MIN);
		GUI::LayoutSlider(mRoughness, 0, 1, 16, float4(.3f, .3f, .3f, 1));

		GUI::LayoutLabel(sem16, "SpecularTint:", 16, 18, 0, 1, 2, TEXT_ANCHOR_MIN);
		GUI::LayoutSlider(mSpecularTint, 0, 1, 16, float4(.3f, .3f, .3f, 1));

		GUI::LayoutLabel(sem16, "SheenTint:", 16, 18, 0, 1, 2, TEXT_ANCHOR_MIN);
		GUI::LayoutSlider(mSheenTint, 0, 1, 16, float4(.3f, .3f, .3f, 1));

		GUI::LayoutLabel(sem16, "Sheen:", 16, 18, 0, 1, 2, TEXT_ANCHOR_MIN);
		GUI::LayoutSlider(mSheen, 0, 1, 16, float4(.3f, .3f, .3f, 1));

		GUI::LayoutLabel(sem16, "ClearcoatGloss:", 16, 18, 0, 1, 2, TEXT_ANCHOR_MIN);
		GUI::LayoutSlider(mClearcoatGloss, 0, 1, 16, float4(.3f, .3f, .3f, 1));

		GUI::LayoutLabel(sem16, "Clearcoat:", 16, 18, 0, 1, 2, TEXT_ANCHOR_MIN);
		GUI::LayoutSlider(mClearcoat, 0, 1, 16, float4(.3f, .3f, .3f, 1));

		GUI::LayoutLabel(sem16, "Subsurface:", 16, 18, 0, 1, 2, TEXT_ANCHOR_MIN);
		GUI::LayoutSlider(mSubsurface, 0, 1, 16, float4(.3f, .3f, .3f, 1));

		GUI::LayoutLabel(sem16, "Transmission:", 16, 18, 0, 1, 2, TEXT_ANCHOR_MIN);
		GUI::LayoutSlider(mTransmission, 0, 1, 16, float4(.3f, .3f, .3f, 1));

		GUI::EndLayout();

		mMaterial->SetParameter("LocalLightDirection", normalize(float3(sinf(mPhi) * cosf(mTheta), cosf(mPhi), sinf(mPhi) * sinf(mTheta))));

		mMaterial->SetParameter("Metallic", mMetallic);
		mMaterial->SetParameter("Specular", mSpecular);
		mMaterial->SetParameter("Anisotropy", mAnisotropy);
		mMaterial->SetParameter("Roughness", mRoughness);
		mMaterial->SetParameter("SpecularTint", mSpecularTint);
		mMaterial->SetParameter("SheenTint", mSheenTint);
		mMaterial->SetParameter("Sheen", mSheen);
		mMaterial->SetParameter("ClearcoatGloss", mClearcoatGloss);
		mMaterial->SetParameter("Clearcoat", mClearcoat);
		mMaterial->SetParameter("Subsurface", mSubsurface);
		mMaterial->SetParameter("Transmission", mTransmission);
	}

	PLUGIN_EXPORT void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) override {
		float3 d = normalize(float3(sinf(mPhi) * cosf(mTheta), cosf(mPhi), -sinf(mPhi) * sinf(mTheta)));
		Gizmos::DrawLine(0, d * .75f, float4(1, 1, 1, 1));
	}
};

ENGINE_PLUGIN(BRDFView)