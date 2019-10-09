#include <Scene/MeshRenderer.hpp>
#include <Scene/TextRenderer.hpp>
#include <Util/Profiler.hpp>

#include "DicomVis.hpp"

#include <thread>

#define THREAD_COUNT 1

using namespace std;

ENGINE_PLUGIN(DicomVis)

DicomVis::DicomVis() : mScene(nullptr) {}
DicomVis::~DicomVis() {}

bool DicomVis::Init(Scene* scene) {
	mScene = scene;

	Shader* fontshader = mScene->AssetManager()->LoadShader("Shaders/font.shader");
	Font* font = mScene->AssetManager()->LoadFont("Assets/segoeui.ttf", 24);

	shared_ptr<Material> fontMat = make_shared<Material>("Segoe UI", fontshader);
	fontMat->SetParameter("MainTexture", font->Texture());


	return true;
}

void DicomVis::Update(const FrameTime& frameTime) {

}

shared_ptr<Texture> LoadDicomVolume(const vector<string>& files, vec3& size, DeviceManager* devices) {
	struct Slice {
		float mLocation;
		vec3 mSize;
	};
	vector<Slice> slices;
	
	uint32_t w = 0;
	uint32_t h = 0;
	uint32_t d = 0;

	size = vec3(0, 0, 0);

	for (uint32_t i = 0; i < (int)files.size(); i++) {
		
	}

	std::sort(slices.begin(), slices.end(), [](const Slice& a, const Slice& b) {
		return a.mLocation < b.mLocation;
	});

	printf("%.02fm x %.02fm x %.02fm\n", size.x, size.y, size.z);

	#pragma pack(push)
	#pragma pack(1)
	struct VolumePixel {
		uint16_t color;
		uint16_t mask;
	};
	#pragma pack(pop)

	VkDeviceSize imageSize = (VkDeviceSize)w * (VkDeviceSize)h * (VkDeviceSize)d;
	VolumePixel* pixels = new VolumePixel[imageSize];
	imageSize *= sizeof(VolumePixel);
	memset(pixels, 0xFFFF, imageSize);

	auto readImages = [&](uint32_t begin, uint32_t end) {
		for (uint32_t i = begin; i < end; i++) {
			VolumePixel* slice = pixels + (VkDeviceSize)w * (VkDeviceSize)h * (VkDeviceSize)i;
			slices[i].mImage->setMinMaxWindow();
			const uint16_t* pixelData = (const uint16_t*)slices[i].mImage->getOutputData(16);
			uint32_t j = 0;
			for (uint32_t x = 0; x < w; x++)
				for (uint32_t y = 0; y < h; y++) {
					j = x + y * w;
					slice[j].color = pixelData[j];
				}
		}
	};

	if (THREAD_COUNT > 1) {
		vector<thread> threads;
		uint32_t s = ((uint32_t)slices.size() + THREAD_COUNT - 1) / THREAD_COUNT;
		for (uint32_t i = 0; i < (uint32_t)slices.size(); i += s)
			threads.push_back(thread(readImages, i, gmin(i + s, (uint32_t)slices.size())));
		for (uint32_t i = 0; i < (uint32_t)threads.size(); i++)
			threads[i].join();
	} else
		readImages(0, (uint32_t)slices.size());

	shared_ptr<Texture> tex = shared_ptr<Texture>(new Texture("Dicom Volume", devices,
		pixels, imageSize, w, h, d, VK_FORMAT_R16G16_UNORM, 0,
		VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT) );

	delete[] pixels;

	return tex;
}