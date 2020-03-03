#include <Core/EnginePlugin.hpp>

#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Scene/TextRenderer.hpp>
#include <Scene/MeshRenderer.hpp>
#include <Util/Profiler.hpp>

#include "TerrainSystem.hpp"

using namespace std;

ENGINE_PLUGIN(TerrainSystem)

TerrainSystem::TerrainSystem() : mScene(nullptr), mTerrain(nullptr), mSelected(nullptr) {
	mEnabled = true;
}
TerrainSystem::~TerrainSystem() {
	for (Object* obj : mObjects)
		mScene->RemoveObject(obj);
}

bool TerrainSystem::Init(Scene* scene) {
	mScene = scene;
	mInput = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

	#pragma region Terrain object
	shared_ptr<Material> terrainMat = make_shared<Material>("Terrain", mScene->AssetManager()->LoadShader("Shaders/terrain.stm"));

	terrainMat->SetParameter("MainTextures", 0, mScene->AssetManager()->LoadTexture("Assets/Textures/rock13/rock13_col.jpg"));
	terrainMat->SetParameter("NormalTextures", 0, mScene->AssetManager()->LoadTexture("Assets/Textures/rock13/rock13_nrm.jpg", false));
	terrainMat->SetParameter("MaskTextures", 0, mScene->AssetManager()->LoadTexture("Assets/Textures/rock13/rock13_msk.png", false));

	//terrainMat->SetParameter("MainTextures", 1, mScene->AssetManager()->LoadTexture("Assets/Textures/grass/grass1_col.png"));
	//terrainMat->SetParameter("NormalTextures", 1, mScene->AssetManager()->LoadTexture("Assets/Textures/grass/grass1_nrm.png", false));
	//terrainMat->SetParameter("MaskTextures", 1, mScene->AssetManager()->LoadTexture("Assets/Textures/grass/grass1_msk.png", false));
	
	//terrainMat->SetParameter("MainTextures", 2, mScene->AssetManager()->LoadTexture("Assets/Textures/dirt/ground3_col.jpg"));
	//terrainMat->SetParameter("NormalTextures", 2, mScene->AssetManager()->LoadTexture("Assets/Textures/dirt/ground3_nrm.jpg", false));
	//terrainMat->SetParameter("MaskTextures", 2, mScene->AssetManager()->LoadTexture("Assets/Textures/dirt/ground3_msk.jpg", false));

	terrainMat->SetParameter("MainTextures", 1, mScene->AssetManager()->LoadTexture("Assets/Textures/snow06/Snow06_col.jpg", false));
	terrainMat->SetParameter("NormalTextures", 1, mScene->AssetManager()->LoadTexture("Assets/Textures/snow06/Snow06_nrm.jpg", false));
	terrainMat->SetParameter("MaskTextures", 1, mScene->AssetManager()->LoadTexture("Assets/Textures/snow06/Snow06_msk.png", false));

	shared_ptr<TerrainRenderer> terrain = make_shared<TerrainRenderer>("Terrain", 2048.f, 100.f);
	mScene->AddObject(terrain);
	terrain->Material(terrainMat);
	mTerrain = terrain.get();
	mObjects.push_back(mTerrain);
	mTerrain->Initialize();
	#pragma endregion
	
	#pragma region Detail meshes
	/*
	shared_ptr<Material> rockMat = make_shared<Material>("Rock", mScene->AssetManager()->LoadShader("Shaders/pbr.shader"));
	shared_ptr<Material> treeMat = make_shared<Material>("Tree", mScene->AssetManager()->LoadShader("Shaders/pbr.shader"));
	shared_ptr<Material> bushMat = make_shared<Material>("Bush", mScene->AssetManager()->LoadShader("Shaders/leaves.shader"));
	shared_ptr<Material> grassMat = make_shared<Material>("Grass", mScene->AssetManager()->LoadShader("Shaders/leaves.shader"));

	treeMat->SetParameter("MainTexture", mScene->AssetManager()->LoadTexture("Assets/Textures/bark/bark_col.png"));
	treeMat->SetParameter("NormalTexture", mScene->AssetManager()->LoadTexture("Assets/Textures/bark/bark_nrm.png"));
	treeMat->SetParameter("MaskTexture", mScene->AssetManager()->LoadTexture("Assets/Textures/bark/bark_msk.png"));
	treeMat->SetParameter("Color", float4(1));
	treeMat->SetParameter("Roughness", 1.f);
	treeMat->SetParameter("Metallic", 1.f);
	treeMat->SetParameter("BumpStrength", 1.f);
	treeMat->EnableKeyword("COLOR_MAP");
	treeMat->EnableKeyword("NORMAL_MAP");
	treeMat->EnableKeyword("MASK_MAP");
	treeMat->PassMask((PassType)(Main | Depth));

	bushMat->SetParameter("MainTexture", mScene->AssetManager()->LoadTexture("Assets/Textures/leaves/leaves_col.png"));
	bushMat->SetParameter("NormalTexture", mScene->AssetManager()->LoadTexture("Assets/Textures/leaves/leaves_nrm.png"));
	bushMat->SetParameter("MaskTexture", mScene->AssetManager()->LoadTexture("Assets/Textures/leaves/leaves_msk.png"));
	bushMat->SetParameter("BumpStrength", 1.f);
	bushMat->PassMask((PassType)(Main | Depth));

	grassMat->SetParameter("MainTexture", mScene->AssetManager()->LoadTexture("Assets/Textures/white.png"));
	grassMat->SetParameter("NormalTexture", mScene->AssetManager()->LoadTexture("Assets/Textures/bump.png"));
	grassMat->SetParameter("MaskTexture", mScene->AssetManager()->LoadTexture("Assets/Textures/white.png"));
	grassMat->SetParameter("BumpStrength", 1.f);
	grassMat->PassMask((PassType)(Main | Depth));

	rockMat->SetParameter("MainTexture", mScene->AssetManager()->LoadTexture("Assets/Models/rock/rock_col.png"));
	rockMat->SetParameter("NormalTexture", mScene->AssetManager()->LoadTexture("Assets/Models/rock/rock_nrm.png"));
	rockMat->SetParameter("MaskTexture", mScene->AssetManager()->LoadTexture("Assets/Models/rock/rock_msk.png"));
	rockMat->SetParameter("Color", float4(1));
	rockMat->SetParameter("Roughness", 1.f);
	rockMat->SetParameter("Metallic", 1.f);
	rockMat->SetParameter("BumpStrength", 1.f);
	rockMat->EnableKeyword("COLOR_MAP");
	rockMat->EnableKeyword("NORMAL_MAP");
	rockMat->EnableKeyword("MASK_MAP");
	rockMat->PassMask((PassType)(Main | Depth));
	mTerrain->AddDetail(
		mScene->AssetManager()->LoadMesh("Assets/Models/oaktree.fbx", .05f),
		treeMat, 50, false, .005f, 0.005f, -.1f );

	mTerrain->AddDetail(
		mScene->AssetManager()->LoadMesh("Assets/Models/rock/rock.fbx", .05f),
		rockMat, 40, true, .005f, .005f, -.4f );

	mTerrain->AddDetail(
		mScene->AssetManager()->LoadMesh("Assets/Models/bush_01.fbx", .05f),
		bushMat, 20, true, .05f, .05f, 0 );
	*/
	//mTerrain->AddDetail(
	//	mScene->AssetManager()->LoadMesh("Assets/Models/grass_clump_01.fbx", .05f),
	//	grassMat, true, 5.f, .1f, 0 );
	#pragma endregion

	return true;
}

void TerrainSystem::Update(CommandBuffer* commandBuffer) {
	float tod = mScene->Environment()->TimeOfDay();
	tod += mScene->Instance()->DeltaTime() * .000555555555f; // 30min days
	if (mInput->KeyDown(KEY_T)) {
		tod += mScene->Instance()->DeltaTime() * .075f; // zoooom
		if (tod > 1) tod -= 1;
	}
	mScene->Environment()->TimeOfDay(tod);

	mTerrain->UpdateLOD(mScene->Cameras()[0]);
}

void TerrainSystem::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
	const Ray& ray = mInput->GetPointer(0)->mWorldRay;
	float hitT;
	Collider* hit = mScene->Raycast(ray, hitT);

	bool change = mInput->KeyDownFirst(MOUSE_LEFT);

	// manipulate selection
	Light* selectedLight = nullptr;
	if (mSelected) {
		selectedLight = dynamic_cast<Light*>(mSelected);
		if (selectedLight) {
			switch (selectedLight->Type()) {
			case LightType::Spot:
				Gizmos::DrawWireSphere(selectedLight->WorldPosition(), selectedLight->Radius(), float4(selectedLight->Color(), .5f));
				Gizmos::DrawWireCircle(selectedLight->WorldPosition() + selectedLight->WorldRotation() * float3(0, 0, selectedLight->Range()),
					selectedLight->Range() * tanf(selectedLight->InnerSpotAngle()), selectedLight->WorldRotation(), float4(selectedLight->Color(), .5f));
				Gizmos::DrawWireCircle(
					selectedLight->WorldPosition() + selectedLight->WorldRotation() * float3(0, 0, selectedLight->Range()),
					selectedLight->Range() * tanf(selectedLight->OuterSpotAngle()), selectedLight->WorldRotation(), float4(selectedLight->Color(), .5f));
				break;

			case LightType::Point:
				Gizmos::DrawWireSphere(selectedLight->WorldPosition(), selectedLight->Radius(), float4(selectedLight->Color(), .5f));
				Gizmos::DrawWireSphere(selectedLight->WorldPosition(), selectedLight->Range(), float4(selectedLight->Color(), .2f));
				break;
			}
		}

		float s = camera->Orthographic() ? .05f : .05f * length(mSelected->WorldPosition() - camera->WorldPosition());
		if (mInput->KeyDown(KEY_SHIFT)) {
			quaternion r = mSelected->WorldRotation();
			if (Gizmos::RotationHandle(mInput->GetPointer(0), mSelected->WorldPosition(), r, s)) {
				mSelected->LocalRotation(r);
				change = false;
			}
		} else {
			float3 p = mSelected->WorldPosition();
			if (Gizmos::PositionHandle(mInput->GetPointer(0), camera->WorldRotation(), p, s)) {
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
		Gizmos::DrawBillboard(light->WorldPosition(), hover && light != selectedLight ? .09f : .075f, camera->WorldRotation(), float4(col, 1),
			mScene->AssetManager()->LoadTexture("Assets/Textures/icons.png"), float4(.5f, .5f, 0, 0));

		if (hover) {
			hitT = lt;
			if (change) mSelected = light;
		}
	}
}