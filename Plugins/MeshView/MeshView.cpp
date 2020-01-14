#include <Scene/MeshRenderer.hpp>
#include <Scene/TextRenderer.hpp>
#include <Util/Profiler.hpp>

#include <Core/EnginePlugin.hpp>
#include <assimp/pbrmaterial.h>

#define STB_IMAGE_IMPLEMENTATION
#include <ThirdParty/stb_image.h>

using namespace std;

class MeshView : public EnginePlugin {
public:
	PLUGIN_EXPORT MeshView();
	PLUGIN_EXPORT ~MeshView();

	PLUGIN_EXPORT bool Init(Scene* scene) override;
	PLUGIN_EXPORT void Update() override;
	PLUGIN_EXPORT void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera);

private:
	Scene* mScene;
	vector<Object*> mObjects;
	Object* mSelected;

	MouseKeyboardInput* mInput;
};

ENGINE_PLUGIN(MeshView)

MeshView::MeshView() : mScene(nullptr), mSelected(nullptr), mInput(nullptr) {
	mEnabled = true;
}
MeshView::~MeshView() {
	for (Object* obj : mObjects)
		mScene->RemoveObject(obj);
}

bool MeshView::Init(Scene* scene) {
	mScene = scene;
	mInput = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

	mScene->Environment()->EnableCelestials(true);
	mScene->Environment()->EnableScattering(true);

	shared_ptr<Material> pbrMat = make_shared<Material>("Floor", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
	pbrMat->EnableKeyword("TEXTURED");
	pbrMat->SetParameter("MainTextures", 0, mScene->AssetManager()->LoadTexture("Assets/Textures/grid.png"));
	pbrMat->SetParameter("NormalTextures", 0, mScene->AssetManager()->LoadTexture("Assets/Textures/bump.png"));
	pbrMat->SetParameter("MaskTextures", 0, mScene->AssetManager()->LoadTexture("Assets/Textures/mask.png"));
	pbrMat->SetParameter("TextureST", float4(2048, 2048, 0, 0));
	pbrMat->SetParameter("TextureIndex", 0);
	pbrMat->SetParameter("Color", float4(1, 1, 1, 1));
	pbrMat->SetParameter("Metallic", 0.f);
	pbrMat->SetParameter("Roughness", .5f);

	shared_ptr<MeshRenderer> floor = make_shared<MeshRenderer>("Floor");
	floor->Material(pbrMat);
	floor->Mesh(shared_ptr<Mesh>(Mesh::CreatePlane("Floor", mScene->Instance(), 2048)));
	floor->LocalRotation(quaternion(float3(-PI*.5f, 0, 0)));
	mScene->AddObject(floor);
	mObjects.push_back(floor.get());

	return true;
}

void MeshView::Update() {
	float tod = mScene->Environment()->TimeOfDay();
	tod += mScene->Instance()->DeltaTime() * .000555555555f; // 30min days
	if (mInput->KeyDown(KEY_T)) {
		tod += mScene->Instance()->DeltaTime() * .075f; // zoooom
		if (tod > 1) tod -= 1;
	}
	mScene->Environment()->TimeOfDay(tod);

}
void MeshView::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
	const Ray& ray = mInput->GetPointer(0)->mWorldRay;
	float hitT;
	Collider* hit = mScene->Raycast(ray, hitT);

	Gizmos* gizmos = mScene->Gizmos();

	bool change = mInput->KeyDownFirst(MOUSE_LEFT);

	// manipulate selection
	Light* selectedLight = nullptr;
	if (mSelected) {
		selectedLight = dynamic_cast<Light*>(mSelected);
		if (selectedLight) {
			switch (selectedLight->Type()) {
			case LightType::Spot:
				gizmos->DrawWireSphere(selectedLight->WorldPosition(), selectedLight->Radius(), float4(selectedLight->Color(), .5f));
				gizmos->DrawWireCircle(selectedLight->WorldPosition() + selectedLight->WorldRotation() * float3(0, 0, selectedLight->Range()),
					selectedLight->Range() * tanf(selectedLight->InnerSpotAngle()), selectedLight->WorldRotation(), float4(selectedLight->Color(), .5f));
				gizmos->DrawWireCircle(
					selectedLight->WorldPosition() + selectedLight->WorldRotation() * float3(0, 0, selectedLight->Range()),
					selectedLight->Range() * tanf(selectedLight->OuterSpotAngle()), selectedLight->WorldRotation(), float4(selectedLight->Color(), .5f));
				break;

			case LightType::Point:
				gizmos->DrawWireSphere(selectedLight->WorldPosition(), selectedLight->Radius(), float4(selectedLight->Color(), .5f));
				gizmos->DrawWireSphere(selectedLight->WorldPosition(), selectedLight->Range(), float4(selectedLight->Color(), .2f));
				break;
			}
		}

		float s = camera->Orthographic() ? .05f : .05f * length(mSelected->WorldPosition() - camera->WorldPosition());
		if (mInput->KeyDown(KEY_LSHIFT)) {
			quaternion r = mSelected->WorldRotation();
			if (mScene->Gizmos()->RotationHandle(mInput->GetPointer(0), mSelected->WorldPosition(), r, s)) {
				mSelected->LocalRotation(r);
				change = false;
			}
		} else {
			float3 p = mSelected->WorldPosition();
			if (mScene->Gizmos()->PositionHandle(mInput->GetPointer(0), camera->WorldRotation(), p, s)) {
				mSelected->LocalPosition(p);
				change = false;
			}
		}
	}

	// change selection
	if (change) mSelected = nullptr;
	for (Light* light : mScene->ActiveLights()) {
		float lt = ray.Intersect(Sphere(light->WorldPosition(), .09f)).x;
		bool hover = lt > 0 && (hitT < 0 || lt < hitT);

		float3 col = light->mEnabled ? light->Color() : light->Color() * .2f;
		gizmos->DrawBillboard(light->WorldPosition(), hover && light != selectedLight ? .09f : .075f, camera->WorldRotation(), float4(col, 1),
			mScene->AssetManager()->LoadTexture("Assets/Textures/icons.png"), float4(.5f, .5f, 0, 0));

		if (hover) {
			hitT = lt;
			if (change) mSelected = light;
		}
	}
}