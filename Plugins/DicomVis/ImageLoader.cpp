#include "ImageLoader.hpp"

#include <thread>

#define STB_IMAGE_IMPLEMENTATION
#include <ThirdParty/stb_image.h>

#include <dcmtk/dcmimgle/dcmimage.h>
#include <dcmtk/dcmdata/dctk.h>

using namespace std;

unordered_multimap<string, ImageStackType> ExtensionMap {
	{ ".dcm", IMAGE_STACK_DICOM },
	{ ".raw", IMAGE_STACK_RAW },
	{ ".png", IMAGE_STACK_STANDARD },
	{ ".jpg", IMAGE_STACK_STANDARD }
};

ImageStackType ImageLoader::FolderStackType(const fs::path& folder) {
	if (!fs::exists(folder)) return IMAGE_STACK_NONE;

	ImageStackType type = IMAGE_STACK_NONE;
	for (const auto& p : fs::directory_iterator(folder))
		if (ExtensionMap.count(p.path().extension().string())) {
			ImageStackType t = ExtensionMap.find(p.path().extension().string())->second;
			if (type != IMAGE_STACK_NONE && type != t) return IMAGE_STACK_NONE; // inconsistent image type
			type = t;
		}

	if (type == IMAGE_STACK_NONE) return type;

	if (type == IMAGE_STACK_STANDARD) {
		vector<fs::path> images;
		for (const auto& p : fs::directory_iterator(folder))
			if (ExtensionMap.count(p.path().extension().string()))
				images.push_back(p.path());
		std::sort(images.begin(), images.end(), [](const fs::path& a, const fs::path& b) {
			return a.stem().string() < b.stem().string();
		});

		int x, y, c;
		if (stbi_info(images[0].string().c_str(), &x, &y, &c) == 0) return IMAGE_STACK_NONE;

		uint32_t width = x;
		uint32_t height = y;
		uint32_t channels = c;
		uint32_t depth = images.size();

		for (uint32_t i = 1; i < images.size(); i++) {
			stbi_info(images[i].string().c_str(), &x, &y, &c);
			if (x != width) { return IMAGE_STACK_NONE; }
			if (y != height) { return IMAGE_STACK_NONE; }
			if (c != channels) { return IMAGE_STACK_NONE; }
		}
	}
	return type;
}

Texture* ImageLoader::LoadStandardStack(const fs::path& folder, Device* device, float3* scale) {
	if (!fs::exists(folder)) return nullptr;

	vector<fs::path> images;
	for (const auto& p : fs::directory_iterator(folder))
		if (ExtensionMap.count(p.path().extension().string()) && ExtensionMap.find(p.path().extension().string())->second == IMAGE_STACK_STANDARD)
			images.push_back(p.path());
	if (images.empty()) return nullptr;
	std::sort(images.begin(), images.end(), [](const fs::path& a, const fs::path& b) {
		return a.stem().string() < b.stem().string();
	});

	int x, y, c;
	stbi_info(images[0].string().c_str(), &x, &y, &c);
	
	uint32_t depth = images.size();
	uint32_t width = x;
	uint32_t height = y;
	uint32_t channels = c == 3 ? 4 : c;
	uint32_t bpp = 1;

	VkFormat format;
	switch (channels) {
	case 4:
		format = VK_FORMAT_R8G8B8A8_UNORM;
		break;
	case 2:
		format = VK_FORMAT_R8G8_UNORM;
		break;
	case 1:
		format = VK_FORMAT_R8_UNORM;
		break;
	default:
		return nullptr; // ??
	}

	size_t sliceSize = width * height * channels;
	uint8_t* pixels = new uint8_t[sliceSize * depth];

	uint32_t done = 0;
	vector<thread> threads;
	uint32_t threadCount = thread::hardware_concurrency();
	for (uint32_t j = 0; j < threadCount; j++) {
		threads.push_back(thread([=,&done](){
			int xt, yt, ct;
			for (uint32_t i = j; i < images.size(); i += threadCount) {
				stbi_uc* img = stbi_load(images[i].string().c_str(), &xt, &yt, &ct, channels);
				memcpy(pixels + sliceSize * i, img, sliceSize);
				stbi_image_free(img);
				done++;
			}
		}));
	}
	printf("Loading stack");
	while (done < images.size()) {
		printf("\rLoading stack: %u/%u    ", done, (uint32_t)images.size());
		this_thread::sleep_for(10ms);
	}
	for (thread& t : threads) t.join();
	printf("\rLoading stack: Done           \n");

	Texture* volume = new Texture(folder.string(), device, pixels, sliceSize*depth, width, height, depth, format, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	delete[] pixels;

	if (scale) *scale = float3(.05f, .05f, .05f);

	return volume;
}

struct DcmSlice {
	DicomImage* image;
	double3 spacing;
	double location;
};
DcmSlice ReadDicomSlice(const string& file) {
	DcmFileFormat fileFormat;
	fileFormat.loadFile(file.c_str());
	DcmDataset* dataset = fileFormat.getDataset();

	double3 s = 0;
	dataset->findAndGetFloat64(DCM_PixelSpacing, s.x, 0);
	dataset->findAndGetFloat64(DCM_PixelSpacing, s.y, 1);
	dataset->findAndGetFloat64(DCM_SliceThickness, s.z, 0);

	double x = 0;
	dataset->findAndGetFloat64(DCM_SliceLocation, x, 0);

	return { new DicomImage(file.c_str()), s, x };
}
Texture* ImageLoader::LoadDicomStack(const fs::path& folder, Device* device, float3* size) {
	if (!fs::exists(folder)) return nullptr;

	double3 maxSpacing = 0;
	vector<DcmSlice> images = {};
	for (const auto& p : fs::directory_iterator(folder))
		if (ExtensionMap.count(p.path().extension().string()) && ExtensionMap.find(p.path().extension().string())->second == IMAGE_STACK_DICOM) {
			images.push_back(ReadDicomSlice(p.path().string()));
			maxSpacing = max(maxSpacing, images[images.size() - 1].spacing);
		}

	if (images.empty()) return nullptr;

	std::sort(images.begin(), images.end(), [](const DcmSlice& a, const DcmSlice& b) {
		return a.location < b.location;
	});

	uint32_t w = images[0].image->getWidth();
	uint32_t h = images[0].image->getHeight();
	uint32_t d = (uint32_t)images.size();

	if (w == 0 || h == 0) return nullptr;

	// volume size in meters
	if (size) {
		float2 b = images[0].location;
		for (auto i : images) {
			b.x = (float)fmin(i.location - i.spacing.z * .5, b.x);
			b.y = (float)fmax(i.location + i.spacing.z * .5, b.y);
		}

		*size = float3(.001 * double3(maxSpacing.xy * double2(w, h), b.y - b.x));
		printf("%fm x %fm x %fm\n", size->x, size->y, size->z);
	}

	uint16_t* data = new uint16_t[w * h * d];
	memset(data, 0, w * h * d * sizeof(uint16_t));
	for (uint32_t i = 0; i < images.size(); i++) {
		images[i].image->setMinMaxWindow();
		uint16_t* pixels = (uint16_t*)images[i].image->getOutputData(16);
		memcpy(data + i * w * h, pixels, w * h * sizeof(uint16_t));
	}

	Texture* tex = new Texture(folder.string(), device, data, w*h*d*sizeof(uint16_t), w, h, d, VK_FORMAT_R16_UNORM, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	delete[] data;
	for (auto& i : images) delete i.image;
	return tex;
}

Texture* ImageLoader::LoadRawStack(const fs::path& folder, Device* device, float3* scale) {
	if (!fs::exists(folder)) return nullptr;

	vector<fs::path> images;
	for (const auto& p : fs::directory_iterator(folder))
		if (ExtensionMap.count(p.path().extension().string()) && ExtensionMap.find(p.path().extension().string())->second == IMAGE_STACK_RAW)
			images.push_back(p.path());
	if (images.empty()) return nullptr;
	std::sort(images.begin(), images.end(), [](const fs::path& a, const fs::path& b) {
		return a.string() < b.string();
	});

	return nullptr;
}
