#include "Dicom.hpp"

#include <dcmtk/dcmimgle/dcmimage.h>
#include <dcmtk/dcmdata/dctk.h>

using namespace std;

struct Slice {
	DicomImage* image;
	double3 spacing;
	double location;
};

Slice ReadDicomSlice(const string& file) {
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

Texture* Dicom::LoadDicomStack(const string& folder, Device* device, float3* size) {
	double3 maxSpacing = 0;
	vector<Slice> images = {};
	for (const auto& p : fs::directory_iterator(folder))
		if (p.path().extension().string() == ".dcm") {
			images.push_back(ReadDicomSlice(p.path().string()));
			maxSpacing = max(maxSpacing, images[images.size() - 1].spacing);
		}

	if (images.empty()) return nullptr;

	std::sort(images.begin(), images.end(), [](const Slice& a, const Slice& b) {
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

	Texture* tex = new Texture(folder, device, data, w*h*d*sizeof(uint16_t), w, h, d, VK_FORMAT_R16_UNORM, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	delete[] data;
	for (auto& i : images) delete i.image;
	return tex;
}