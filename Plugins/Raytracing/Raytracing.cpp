#include <Core/EnginePlugin.hpp>

#include <Scene/Camera.hpp>
#include <Scene/MeshRenderer.hpp>
#include <Scene/Scene.hpp>
#include <Util/Profiler.hpp>

#include <assimp/pbrmaterial.h>

using namespace std;

class Raytracing : public EnginePlugin {
public:
	PLUGIN_EXPORT Raytracing();
	PLUGIN_EXPORT ~Raytracing();

	PLUGIN_EXPORT bool Init(Scene* scene) override;
	PLUGIN_EXPORT void Update() override;

private:
	vector<Object*> mObjects;
	Scene* mScene;
};

ENGINE_PLUGIN(Raytracing)

Raytracing::Raytracing()
	: mScene(nullptr) {
	mEnabled = true;
}
Raytracing::~Raytracing() {
	for (Object* obj : mObjects)
		mScene->RemoveObject(obj);
}

bool Raytracing::Init(Scene* scene) {
	mScene = scene;

	mScene->Environment()->EnableCelestials(false);
	mScene->Environment()->EnableScattering(false);
	mScene->Environment()->AmbientLight(.3f);
	mScene->Environment()->EnvironmentTexture(mScene->AssetManager()->LoadTexture("Assets/Textures/old_outdoor_theater_4k.hdr"));

	#pragma region load glTF
	string folder = "Assets/Models/room/";
	string file = "CrohnsProtoRoom.gltf";

	shared_ptr<Material> opaque = make_shared<Material>("PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
	opaque->EnableKeyword("TEXTURED");
	opaque->SetParameter("TextureST", float4(1, 1, 0, 0));

	shared_ptr<Material> alphaClip = make_shared<Material>("Cutout PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
	alphaClip->RenderQueue(5000);
	alphaClip->BlendMode(BLEND_MODE_ALPHA);
	alphaClip->CullMode(VK_CULL_MODE_NONE);
	alphaClip->EnableKeyword("TEXTURED");
	alphaClip->EnableKeyword("ALPHA_CLIP");
	alphaClip->EnableKeyword("TWO_SIDED");
	alphaClip->SetParameter("TextureST", float4(1, 1, 0, 0));

	shared_ptr<Material> alphaBlend = make_shared<Material>("Transparent PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
	alphaBlend->RenderQueue(5000);
	alphaBlend->BlendMode(BLEND_MODE_ALPHA);
	alphaBlend->CullMode(VK_CULL_MODE_NONE);
	alphaBlend->EnableKeyword("TEXTURED");
	alphaBlend->EnableKeyword("TWO_SIDED");
	alphaBlend->SetParameter("TextureST", float4(1, 1, 0, 0));

	shared_ptr<Material> curOpaque = nullptr;
	shared_ptr<Material> curClip = nullptr;
	shared_ptr<Material> curBlend = nullptr;

	uint32_t arraySize =
		mScene->AssetManager()->LoadShader("Shaders/pbr.stm")->GetGraphics(PASS_MAIN, { "TEXTURED" })->mDescriptorBindings.at("MainTextures").second.descriptorCount;

	uint32_t opaque_i = 0;
	uint32_t clip_i = 0;
	uint32_t blend_i = 0;

	auto matfunc = [&](Scene* scene, aiMaterial* aimaterial) {
		aiString alphaMode;
		if (aimaterial->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS) {
			if (alphaMode == aiString("MASK")) return alphaClip;
			if (alphaMode == aiString("BLEND")) return alphaBlend;
		}
		return opaque;
	};
	auto objfunc = [&](Scene* scene, Object* object, aiMaterial* aimaterial) {
		MeshRenderer* renderer = dynamic_cast<MeshRenderer*>(object);
		if (!renderer) return;

		Material* mat = renderer->Material();
		uint32_t i;

		if (mat == opaque.get()) {
			i = opaque_i;
			opaque_i++;
			if (opaque_i >= arraySize) curOpaque.reset();
			if (!curOpaque) {
				opaque_i = opaque_i % arraySize;
				curOpaque = make_shared<Material>("PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
				curOpaque->EnableKeyword("TEXTURED");
				curOpaque->SetParameter("TextureST", float4(1, 1, 0, 0));
			}
			renderer->Material(curOpaque);
			mat = curOpaque.get();

		} else if (mat == alphaClip.get()) {
			i = clip_i;
			clip_i++;
			if (clip_i >= arraySize) curClip.reset();
			if (!curClip) {
				clip_i = clip_i % arraySize;
				curClip = make_shared<Material>("Cutout PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
				curClip->RenderQueue(5000);
				curClip->BlendMode(BLEND_MODE_ALPHA);
				curClip->CullMode(VK_CULL_MODE_NONE);
				curClip->EnableKeyword("TEXTURED");
				curClip->EnableKeyword("ALPHA_CLIP");
				curClip->EnableKeyword("TWO_SIDED");
				curClip->SetParameter("TextureST", float4(1, 1, 0, 0));
			}
			renderer->Material(curClip);
			mat = curClip.get();

		} else if (mat == alphaBlend.get()) {
			i = blend_i;
			blend_i++;
			if (blend_i >= 64) curBlend.reset();
			if (!curBlend) {
				blend_i = blend_i % arraySize;
				curBlend = make_shared<Material>("Transparent PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
				curBlend->RenderQueue(5000);
				curBlend->BlendMode(BLEND_MODE_ALPHA);
				curBlend->CullMode(VK_CULL_MODE_NONE);
				curBlend->EnableKeyword("TEXTURED");
				curBlend->EnableKeyword("TWO_SIDED");
				curBlend->SetParameter("TextureST", float4(1, 1, 0, 0));
			}
			renderer->Material(curBlend);
			mat = curBlend.get();

		} else return;

		aiColor3D emissiveColor(0);
		aiColor4D baseColor(1);
		float metallic = 1.f;
		float roughness = 1.f;
		aiString baseColorTexture, metalRoughTexture, normalTexture, emissiveTexture;

		if (aimaterial->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_TEXTURE, &baseColorTexture) == AI_SUCCESS && baseColorTexture.length) {
			mat->SetParameter("MainTextures", i, scene->AssetManager()->LoadTexture(folder + baseColorTexture.C_Str()));
			baseColor = aiColor4D(1);
		} else
			mat->SetParameter("MainTextures", i, scene->AssetManager()->LoadTexture("Assets/Textures/white.png"));

		if (aimaterial->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &metalRoughTexture) == AI_SUCCESS && metalRoughTexture.length)
			mat->SetParameter("MaskTextures", i, scene->AssetManager()->LoadTexture(folder + metalRoughTexture.C_Str(), false));
		else
			mat->SetParameter("MaskTextures", i, scene->AssetManager()->LoadTexture("Assets/Textures/mask.png", false));

		if (aimaterial->GetTexture(aiTextureType_NORMALS, 0, &normalTexture) == AI_SUCCESS && normalTexture.length)
			mat->SetParameter("NormalTextures", i, scene->AssetManager()->LoadTexture(folder + normalTexture.C_Str(), false));
		else
			mat->SetParameter("NormalTextures", i, scene->AssetManager()->LoadTexture("Assets/Textures/bump.png", false));

		aimaterial->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_FACTOR, baseColor);
		aimaterial->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, metallic);
		aimaterial->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, roughness);
		aimaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor);

		renderer->PushConstant("TextureIndex", i);
		renderer->PushConstant("Color", float4(baseColor.r, baseColor.g, baseColor.b, baseColor.a));
		renderer->PushConstant("Roughness", roughness);
		renderer->PushConstant("Metallic", metallic);
		renderer->PushConstant("Emission", float3(emissiveColor.r, emissiveColor.g, emissiveColor.b));
	};

	queue<Object*> nodes;
	nodes.push(mScene->LoadModelScene(folder + file, matfunc, objfunc, .6f, 1.f, .05f, .0015f));
	while (nodes.size()) {
		Object* o = nodes.front();
		nodes.pop();
		for (uint32_t i = 0; i < o->ChildCount(); i++)
			nodes.push(o->Child(i));

		mObjects.push_back(o);
		if (Light* l = dynamic_cast<Light*>(o)){
			if (l->Type() == LIGHT_TYPE_SUN){
				l->CascadeCount(1);
				l->ShadowDistance(30);
			}
		}
	}
	#pragma endregion

	return true;
}

void Raytracing::Update() {
	MouseKeyboardInput* input = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

}
