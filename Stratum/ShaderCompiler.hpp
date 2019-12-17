#pragma once

#include <fstream>
#include <Util/Util.hpp>

struct SpirvModule {
	std::vector<uint32_t> mSpirv;

	inline SpirvModule() {}
	inline void Read(std::ifstream& file) {
		uint32_t l;
		file.read(reinterpret_cast<char*>(&l), sizeof(uint32_t));
		mSpirv.resize(l);
		file.read(reinterpret_cast<char*>(mSpirv.data()), mSpirv.size() * sizeof(uint32_t));
	}
	inline void Write(std::ofstream& file) {
		uint32_t l = (uint32_t)mSpirv.size();
		file.write(reinterpret_cast<char*>(&l), sizeof(uint32_t));
		file.write(reinterpret_cast<char*>(mSpirv.data()), mSpirv.size() * sizeof(uint32_t));
	}
};

struct CompiledVariant {
	std::unordered_map<std::string, std::pair<uint32_t, VkDescriptorSetLayoutBinding>> mDescriptorBindings;
	std::unordered_map<std::string, VkPushConstantRange> mPushConstants;
	std::unordered_map<std::string, VkSamplerCreateInfo> mStaticSamplers;

	PassType mPass; // 0 for compute

	uint3 mWorkgroupSize;

	std::string mEntryPoints[2];
	uint32_t mModules[2];

	std::vector<std::string> mKeywords;

	inline CompiledVariant() {}
	inline void Read(std::ifstream& file) {
		uint32_t c;
		file.read(reinterpret_cast<char*>(&c), sizeof(uint32_t));
		for (uint32_t i = 0; i < c; i++) {
			std::string name;
			uint32_t set;
			VkDescriptorSetLayoutBinding binding;

			uint32_t l;
			file.read(reinterpret_cast<char*>(&l), sizeof(uint32_t));
			name.resize(l);
			file.read(name.data(), l);

			file.read(reinterpret_cast<char*>(&set), sizeof(uint32_t));
			file.read(reinterpret_cast<char*>(&binding), sizeof(VkDescriptorSetLayoutBinding));

			mDescriptorBindings.emplace(name, std::make_pair(set, binding));
		}

		file.read(reinterpret_cast<char*>(&c), sizeof(uint32_t));
		for (uint32_t i = 0; i < c; i++) {
			std::string name;
			VkPushConstantRange range;

			uint32_t l;
			file.read(reinterpret_cast<char*>(&l), sizeof(uint32_t));
			name.resize(l);
			file.read(name.data(), l);

			file.read(reinterpret_cast<char*>(&range), sizeof(VkPushConstantRange));

			mPushConstants.emplace(name, range);
		}

		file.read(reinterpret_cast<char*>(&c), sizeof(uint32_t));
		for (uint32_t i = 0; i < c; i++) {
			std::string name;
			VkSamplerCreateInfo range;

			uint32_t l;
			file.read(reinterpret_cast<char*>(&l), sizeof(uint32_t));
			name.resize(l);
			file.read(name.data(), l);

			file.read(reinterpret_cast<char*>(&range), sizeof(VkSamplerCreateInfo));

			mStaticSamplers.emplace(name, range);
		}

		uint32_t kl;
		file.read(reinterpret_cast<char*>(&kl), sizeof(uint32_t));
		mEntryPoints[0].resize(kl);
		if (kl) file.read(mEntryPoints[0].data(), kl);

		file.read(reinterpret_cast<char*>(&kl), sizeof(uint32_t));
		mEntryPoints[1].resize(kl);
		if (kl) file.read(mEntryPoints[1].data(), kl);

		file.read(reinterpret_cast<char*>(&mPass), sizeof(PassType));
		file.read(reinterpret_cast<char*>(&mWorkgroupSize), sizeof(uint3));
		file.read(reinterpret_cast<char*>(&mModules), 2*sizeof(uint32_t));

		uint32_t kwc;
		file.read(reinterpret_cast<char*>(&kwc), sizeof(uint32_t));
		mKeywords.resize(kwc);
		for (uint32_t i = 0; i < kwc; i++) {
			uint32_t l;
			file.read(reinterpret_cast<char*>(&l), sizeof(uint32_t));
			mKeywords[i].resize(l);
			if (l) file.read(const_cast<char*>(mKeywords[i].data()), l);
		}

	}
	inline void Write(std::ofstream& file) {
		uint32_t c = (uint32_t)mDescriptorBindings.size();
		file.write(reinterpret_cast<char*>(&c), sizeof(uint32_t));
		for (auto& p : mDescriptorBindings) {
			uint32_t l = (uint32_t)p.first.length();
			file.write(reinterpret_cast<char*>(&l), sizeof(uint32_t));
			file.write(p.first.data(), p.first.length());
			file.write(reinterpret_cast<char*>(&p.second.first), sizeof(uint32_t));
			file.write(reinterpret_cast<char*>(&p.second.second), sizeof(VkDescriptorSetLayoutBinding));
		}

		c = (uint32_t)mPushConstants.size();
		file.write(reinterpret_cast<char*>(&c), sizeof(uint32_t));
		for (auto& p : mPushConstants) {
			uint32_t l = (uint32_t)p.first.length();
			file.write(reinterpret_cast<char*>(&l), sizeof(uint32_t));
			file.write(p.first.data(), p.first.length());
			file.write(reinterpret_cast<char*>(&p.second), sizeof(VkPushConstantRange));
		}

		c = (uint32_t)mStaticSamplers.size();
		file.write(reinterpret_cast<char*>(&c), sizeof(uint32_t));
		for (auto& p : mStaticSamplers) {
			uint32_t l = (uint32_t)p.first.length();
			file.write(reinterpret_cast<char*>(&l), sizeof(uint32_t));
			file.write(p.first.data(), p.first.length());
			file.write(reinterpret_cast<char*>(&p.second), sizeof(VkSamplerCreateInfo));
		}

		uint32_t kl = (uint32_t)mEntryPoints[0].length();
		file.write(reinterpret_cast<char*>(&kl), sizeof(uint32_t));
		if (kl) file.write(mEntryPoints[0].data(), mEntryPoints[0].length());

		kl = (uint32_t)mEntryPoints[1].length();
		file.write(reinterpret_cast<char*>(&kl), sizeof(uint32_t));
		if (kl) file.write(mEntryPoints[1].data(), mEntryPoints[1].length());

		file.write(reinterpret_cast<char*>(&mPass), sizeof(PassType));
		file.write(reinterpret_cast<char*>(&mWorkgroupSize), sizeof(uint3));
		file.write(reinterpret_cast<char*>(mModules), 2*sizeof(uint32_t));

		uint32_t kwc = (uint32_t)mKeywords.size();
		file.write(reinterpret_cast<char*>(&kwc), sizeof(uint32_t));
		for (std::string& kw : mKeywords) {
			uint32_t l = (uint32_t)kw.length();
			file.write(reinterpret_cast<char*>(&l), sizeof(uint32_t));
			if (l) file.write(kw.data(), kw.length());
		}
	}
};

struct CompiledShader {
	std::vector<SpirvModule> mModules;
	std::vector<CompiledVariant> mVariants;

	uint32_t mRenderQueue;
	VkColorComponentFlags mColorMask;
	VkCullModeFlags mCullMode;
	VkPolygonMode mFillMode;
	BlendMode mBlendMode;
	VkPipelineDepthStencilStateCreateInfo mDepthStencilState;

	inline CompiledShader() {}
	inline CompiledShader(std::ifstream& file) {
		uint32_t mc;
		file.read(reinterpret_cast<char*>(&mc), sizeof(uint32_t));
		mModules.resize(mc);
		for (uint32_t i = 0; i < mc; i++)
			mModules[i].Read(file);

		uint32_t vc;
		file.read(reinterpret_cast<char*>(&vc), sizeof(uint32_t));
		mVariants.resize(vc);
		for (uint32_t i = 0; i < vc; i++)
			mVariants[i].Read(file);

		file.read(reinterpret_cast<char*>(&mRenderQueue), sizeof(uint32_t));
		file.read(reinterpret_cast<char*>(&mColorMask), sizeof(VkColorComponentFlags));
		file.read(reinterpret_cast<char*>(&mCullMode), sizeof(VkCullModeFlags));
		file.read(reinterpret_cast<char*>(&mFillMode), sizeof(VkPolygonMode));
		file.read(reinterpret_cast<char*>(&mBlendMode), sizeof(BlendMode));
		file.read(reinterpret_cast<char*>(&mDepthStencilState), sizeof(VkPipelineDepthStencilStateCreateInfo));
	}

	inline void Write(std::ofstream& file) {
		uint32_t mc = (uint32_t)mModules.size();
		file.write(reinterpret_cast<char*>(&mc), sizeof(uint32_t));
		for (SpirvModule& m : mModules)
			m.Write(file);

		uint32_t vc = (uint32_t)mVariants.size();
		file.write(reinterpret_cast<char*>(&vc), sizeof(uint32_t));
		for (CompiledVariant& v : mVariants)
			v.Write(file);

		file.write(reinterpret_cast<char*>(&mRenderQueue), sizeof(uint32_t));
		file.write(reinterpret_cast<char*>(&mColorMask), sizeof(VkColorComponentFlags));
		file.write(reinterpret_cast<char*>(&mCullMode), sizeof(VkCullModeFlags));
		file.write(reinterpret_cast<char*>(&mFillMode), sizeof(VkPolygonMode));
		file.write(reinterpret_cast<char*>(&mBlendMode), sizeof(BlendMode));
		file.write(reinterpret_cast<char*>(&mDepthStencilState), sizeof(VkPipelineDepthStencilStateCreateInfo));
	}
};