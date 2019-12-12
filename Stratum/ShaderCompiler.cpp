#include <chrono>
#include <iostream>
#include <sstream>
#include <unordered_map>

#ifdef __GNUC__
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif

#include <Util/Util.hpp>
#include <shaderc/shaderc.hpp>
#include <../spirv_cross.hpp>

using namespace std;
using namespace shaderc;

CompileOptions options;

class Includer : public CompileOptions::IncluderInterface {
public:
	virtual shaderc_include_result* GetInclude(const char* requested_source, shaderc_include_type type, const char* requesting_source, size_t include_depth) override {
		auto src = fs::absolute(requesting_source).parent_path();

		string fullpath = src.generic_u8string() + "/" + requested_source;

		shaderc_include_result* response = new shaderc_include_result();

		string& data = mFiles[fullpath];
		if (data.empty() && !ReadFile(fullpath, data)) {
			char* err = new char[128];
			sprintf(err, "Failed to read include file: %s while compiling %s\n", fullpath.c_str(), requesting_source);

			response->source_name = "";
			response->source_name_length = 0;
			response->content = err;
			response->content_length = strlen(response->content);;
			response->user_data = err;
		} else {
			mFullPaths.emplace(fullpath, fullpath);
			const string& name = mFullPaths.at(fullpath);

			response->source_name = name.data();
			response->source_name_length = strlen(response->source_name);
			response->content = data.data();
			response->content_length = strlen(response->content);
			response->user_data = nullptr;
		}

		return response;
	}
	virtual void ReleaseInclude(shaderc_include_result* data) override {
		if (data->user_data) delete[] (char*)data->user_data;
		delete data;
	}

private:
	unordered_map<string, string> mFiles;
	unordered_map<string, string> mFullPaths;
};

bool CompileStage(Compiler* compiler, const CompileOptions& options, ostream& output, const string& source, const string& filename, shaderc_shader_kind stage, const string& entryPoint,
	unordered_map<string, pair<uint32_t, VkDescriptorSetLayoutBinding>>& descriptorBindings, unordered_map<string, VkPushConstantRange>& pushConstants) {
	
	SpvCompilationResult result = compiler->CompileGlslToSpv(source.c_str(), source.length(), stage, filename.c_str(), entryPoint.c_str(), options);
	
	string error = result.GetErrorMessage();
	if (error.size()) fprintf(stderr, "%s\n", error.c_str());
	switch (result.GetCompilationStatus()) {
	case shaderc_compilation_status_success:
		VkShaderStageFlagBits vkstage;
		switch (stage) {
		case shaderc_vertex_shader:
			vkstage = VK_SHADER_STAGE_VERTEX_BIT;
			break;
		case shaderc_geometry_shader:
			vkstage = VK_SHADER_STAGE_GEOMETRY_BIT;
			break;
		case shaderc_tess_control_shader:
			vkstage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
			break;
		case shaderc_tess_evaluation_shader:
			vkstage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
			break;
		case shaderc_fragment_shader:
			vkstage = VK_SHADER_STAGE_FRAGMENT_BIT;
			break;
		case shaderc_compute_shader:
			vkstage = VK_SHADER_STAGE_COMPUTE_BIT;
			break;
		}

		vector<uint32_t> spirv;
		for (auto d = result.cbegin(); d != result.cend(); d++)
			spirv.push_back(*d);

		spirv_cross::Compiler comp(spirv.data(), spirv.size());
		spirv_cross::ShaderResources res = comp.get_shader_resources();

		#pragma region Register resources
		auto registerResource = [&](const spirv_cross::Resource& res, VkDescriptorType type) {
			auto& binding = descriptorBindings[res.name];

			binding.first = comp.get_decoration(res.id, spv::DecorationDescriptorSet);

			binding.second.stageFlags |= vkstage;
			binding.second.binding = comp.get_decoration(res.id, spv::DecorationBinding);
			binding.second.descriptorCount = 1;
			binding.second.descriptorType = type;
		};

		for (const auto& r : res.sampled_images)
			registerResource(r, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		for (const auto& r : res.separate_images)
			if (comp.get_type(r.type_id).image.dim == spv::DimBuffer)
				registerResource(r, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER);
			else
				registerResource(r, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
		for (const auto& r : res.storage_images)
			if (comp.get_type(r.type_id).image.dim == spv::DimBuffer)
				registerResource(r, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER);
			else
				registerResource(r, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		for (const auto& r : res.storage_buffers)
			registerResource(r, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		for (const auto& r : res.separate_samplers)
			registerResource(r, VK_DESCRIPTOR_TYPE_SAMPLER);
		for (const auto& r : res.uniform_buffers)
			registerResource(r, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

		for (const auto& r : res.push_constant_buffers) {
			uint32_t index = 0;

			const auto& type = comp.get_type(r.base_type_id);

			if (type.basetype == spirv_cross::SPIRType::Struct) {
				for (unsigned int i = 0; i < type.member_types.size(); i++) {
					const auto& mtype = comp.get_type(type.member_types[i]);

					const string name = comp.get_member_name(r.base_type_id, index);
					auto& range = pushConstants[name];
					range.stageFlags |= vkstage;
					range.offset = comp.type_struct_member_offset(type, index);

					switch (mtype.basetype) {
					case spirv_cross::SPIRType::Boolean:
					case spirv_cross::SPIRType::SByte:
					case spirv_cross::SPIRType::UByte:
						range.size = 1;
						break;
					case spirv_cross::SPIRType::Short:
					case spirv_cross::SPIRType::UShort:
					case spirv_cross::SPIRType::Half:
						range.size = 2;
						break;
					case spirv_cross::SPIRType::Int:
					case spirv_cross::SPIRType::UInt:
					case spirv_cross::SPIRType::Float:
						range.size = 4;
						break;
					case spirv_cross::SPIRType::Int64:
					case spirv_cross::SPIRType::UInt64:
					case spirv_cross::SPIRType::Double:
						range.size = 8;
						break;
					case spirv_cross::SPIRType::Struct:
						range.size = (uint32_t)comp.get_declared_struct_size(mtype);
						break;
					case spirv_cross::SPIRType::Unknown:
					case spirv_cross::SPIRType::Void:
					case spirv_cross::SPIRType::AtomicCounter:
					case spirv_cross::SPIRType::Image:
					case spirv_cross::SPIRType::SampledImage:
					case spirv_cross::SPIRType::Sampler:
					case spirv_cross::SPIRType::AccelerationStructureNV:
						break;
					}
					range.size *= mtype.columns * mtype.vecsize;
					index++;
				}
			} else
				printf("Push constant data is not a struct! Reflection will not work.\n");
		}
		#pragma endregion
		
		uint32_t elen = (uint32_t)entryPoint.length();
		output.write(reinterpret_cast<const char*>(&elen), sizeof(uint32_t));
		output.write(entryPoint.c_str(), entryPoint.length());

		uint32_t spirvSize = (uint32_t)spirv.size();
		output.write(reinterpret_cast<const char*>(&spirvSize), sizeof(uint32_t));
		output.write(reinterpret_cast<const char*>(spirv.data()), spirv.size() * sizeof(uint32_t));

		if (vkstage == VK_SHADER_STAGE_COMPUTE_BIT) {
			uint32_t workgroupSize[3]{ 0,0,0 };
			auto entryPoints = comp.get_entry_points_and_stages();
			for (const auto& e : entryPoints) {
				if (e.name == entryPoint) {
					auto& ep = comp.get_entry_point(e.name, e.execution_model);
					workgroupSize[0] = ep.workgroup_size.x;
					workgroupSize[1] = ep.workgroup_size.y;
					workgroupSize[2] = ep.workgroup_size.z;
				}
			}
			output.write(reinterpret_cast<const char*>(workgroupSize), sizeof(uint32_t) * 3);
		}

		return true;
	}
	return false;
}


VkCompareOp atocmp(const string& str) {
	if (str == "less")		return VK_COMPARE_OP_LESS;
	if (str == "greater")	return VK_COMPARE_OP_GREATER;
	if (str == "lequal")	return VK_COMPARE_OP_LESS_OR_EQUAL;
	if (str == "gequal")	return VK_COMPARE_OP_GREATER_OR_EQUAL;
	if (str == "equal")		return VK_COMPARE_OP_EQUAL;
	if (str == "nequal")	return VK_COMPARE_OP_NOT_EQUAL;
	if (str == "never")		return VK_COMPARE_OP_NEVER;
	if (str == "always")	return VK_COMPARE_OP_ALWAYS;
	fprintf(stderr, "Unknown comparison: %s\n", str.c_str());
	throw;
}
VkColorComponentFlags atomask(const string& str) {
	VkColorComponentFlags mask = 0;
	if (str.find("r") != string::npos) mask |= VK_COLOR_COMPONENT_R_BIT;
	if (str.find("g") != string::npos) mask |= VK_COLOR_COMPONENT_G_BIT;
	if (str.find("b") != string::npos) mask |= VK_COLOR_COMPONENT_B_BIT;
	if (str.find("a") != string::npos) mask |= VK_COLOR_COMPONENT_A_BIT;
	return mask;
}
VkFilter atofilter(const string& str) {
	if (str == "nearest") return VK_FILTER_NEAREST;
	if (str == "linear")  return VK_FILTER_LINEAR;
	if (str == "cubic")   return VK_FILTER_CUBIC_IMG;
	fprintf(stderr, "Unknown filter: %s\n", str.c_str());
	throw;
}
VkSamplerAddressMode atoaddressmode(const string& str) {
	if (str == "repeat")		    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	if (str == "mirrored_repeat")   return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	if (str == "clamp_edge")	    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	if (str == "clamp_border")	    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	if (str == "mirror_clamp_edge") return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
	fprintf(stderr, "Unknown address mode: %s\n", str.c_str());
	throw;
}
VkBorderColor atobordercolor(const string& str) {
	if (str == "float_transparent_black") return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	if (str == "int_transparent_black")	  return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
	if (str == "float_opaque_black")	  return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	if (str == "int_opaque_black")		  return VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	if (str == "float_opaque_white")	  return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	if (str == "int_opaque_white")		  return VK_BORDER_COLOR_INT_OPAQUE_WHITE;
	fprintf(stderr, "Unknown border color: %s\n", str.c_str());
	throw;
}
VkSamplerMipmapMode atomipmapmode(const string& str) {
	if (str == "nearest") return VK_SAMPLER_MIPMAP_MODE_NEAREST;
	if (str == "linear") return VK_SAMPLER_MIPMAP_MODE_LINEAR;
	fprintf(stderr, "Unknown mipmap mode: %s\n", str.c_str());
	throw;
}

bool Compile(shaderc::Compiler* compiler, const string& filename, ostream& output) {
	string source;
	if (!ReadFile(filename, source)) {
		printf("Failed to read %s!\n", filename.c_str());
		return false;
	}

	unordered_map<shaderc_shader_kind, string> stages;
	vector<string> kernels;
	vector<pair<string, VkSamplerCreateInfo>> staticSamplers;
	vector<pair<string, uint32_t>> arrays; // name, size

	uint32_t renderQueue = 1000;

	VkColorComponentFlags colorMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
	VkPolygonMode fillMode = VK_POLYGON_MODE_FILL;
	BlendMode blendMode = Opaque;
	VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
	depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilState.depthTestEnable = VK_TRUE;
	depthStencilState.depthWriteEnable = VK_TRUE;
	depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencilState.front = depthStencilState.back;
	depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;

	vector<set<string>> variants{ {} };

	istringstream srcstream(source);

	string line;
	while (getline(srcstream, line)) {
		istringstream linestream(line);
		vector<string> words{ istream_iterator<string>{linestream}, istream_iterator<string>{} };
		
		size_t kwc = variants.size();

		for (auto it = words.begin(); it != words.end(); ++it) {
			if (*it == "#pragma") {
				if (++it == words.end()) break;
				if (*it == "multi_compile") {
					if (++it == words.end()) break;
					// iterate all the keywords added by this multi_compile
					while (it != words.end()) {
						// duplicate all existing variants, add this keyword to each
						for (unsigned int i = 0; i < kwc; i++) {
							variants.push_back(variants[i]);
							variants.back().insert(*it);
						}
						++it;
					}
				
				} else if (*it == "vertex") {
					if (++it == words.end()) return false;
					stages[shaderc_vertex_shader] = *it;

				} else if (*it == "fragment") {
					if (++it == words.end()) return false;
					stages[shaderc_fragment_shader] = *it;

				} else if (*it == "geometry") {
					if (++it == words.end()) return false;
					stages[shaderc_geometry_shader] = *it;

				} else if (*it == "tess_control") {
					if (++it == words.end()) return false;
					stages[shaderc_tess_control_shader] = *it;

				} else if (*it == "tess_evaluation") {
					if (++it == words.end()) return false;
					stages[shaderc_tess_evaluation_shader] = *it;

				} else if (*it == "kernel") {
					if (++it == words.end()) return false;
					kernels.push_back(*it);

				} else if (*it == "render_queue"){
					if (++it == words.end()) return false;
					renderQueue = atoi(it->c_str());

				} else if (*it == "color_mask") {
					if (++it == words.end()) return false;
					colorMask = atomask(*it);

				} else if (*it == "zwrite") {
					if (++it == words.end()) return false;
					if (*it == "true") depthStencilState.depthWriteEnable = VK_TRUE;
					else if (*it == "false") depthStencilState.depthWriteEnable = VK_FALSE;
					else {
						fprintf(stderr, "zwrite must be true or false.\n");
						return false;
					}

				} else if (*it == "ztest") {
					if (++it == words.end()) return false;
					if (*it == "true")
						depthStencilState.depthTestEnable = VK_TRUE;
					else if (*it == "false")
						depthStencilState.depthTestEnable = VK_FALSE;
					else{
						fprintf(stderr, "ztest must be true or false.\n");
						return false;
					}

				} else if (*it == "depth_op") {
					if (++it == words.end()) return false;
					depthStencilState.depthCompareOp = atocmp(*it);

				} else if (*it == "cull") {
					if (++it == words.end()) return false;
					if (*it == "front") cullMode = VK_CULL_MODE_FRONT_BIT;
					else if (*it == "back") cullMode = VK_CULL_MODE_BACK_BIT;
					else if (*it == "false") cullMode = VK_CULL_MODE_NONE;
					else {
						fprintf(stderr, "Unknown cull mode: %s\n", it->c_str());
						return false;
					}

				} else if (*it == "fill") {
					if (++it == words.end()) return false;
					if (*it == "solid") fillMode = VK_POLYGON_MODE_FILL;
					else if (*it == "line") fillMode = VK_POLYGON_MODE_LINE;
					else if (*it == "point") fillMode = VK_POLYGON_MODE_POINT;
					else {
						fprintf(stderr, "Unknown fill mode: %s\n", it->c_str());
						return false;
					}

				} else if (*it == "blend") {
					if (++it == words.end()) return false;
					if (*it == "opaque") blendMode = Opaque;
					else if (*it == "alpha") blendMode = Alpha;
					else if (*it == "add") blendMode = Additive;
					else if (*it == "multiply")	blendMode = Multiply;
					else {
						fprintf(stderr, "Unknown blend mode: %s\n", it->c_str());
						return false;
					}

				} else if (*it == "static_sampler") {
					if (++it == words.end()) return false;
					string name = *it;

					VkSamplerCreateInfo samplerInfo = {};
					samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
					samplerInfo.magFilter = VK_FILTER_LINEAR;
					samplerInfo.minFilter = VK_FILTER_LINEAR;
					samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
					samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
					samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
					samplerInfo.anisotropyEnable = VK_TRUE;
					samplerInfo.maxAnisotropy = 2;
					samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
					samplerInfo.unnormalizedCoordinates = VK_FALSE;
					samplerInfo.compareEnable = VK_FALSE;
					samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
					samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
					samplerInfo.minLod = 0;
					samplerInfo.maxLod = 12;
					samplerInfo.mipLodBias = 0;

					while (++it != words.end()) {
						size_t eq = it->find('=');
						if (eq == string::npos) continue;
						string id = it->substr(0, eq);
						string val = it->substr(eq + 1);
						if (id == "magFilter")		    samplerInfo.magFilter = atofilter(val);
						else if (id == "minFilter")		samplerInfo.minFilter = atofilter(val);
						else if (id == "filter")		samplerInfo.minFilter = samplerInfo.magFilter = atofilter(val);
						else if (id == "addressModeU")	samplerInfo.addressModeU = atoaddressmode(val);
						else if (id == "addressModeV")	samplerInfo.addressModeV = atoaddressmode(val);
						else if (id == "addressModeW")	samplerInfo.addressModeW = atoaddressmode(val);
						else if (id == "addressMode")	samplerInfo.addressModeU = samplerInfo.addressModeV = samplerInfo.addressModeW = atoaddressmode(val);
						else if (id == "maxAnisotropy") {
							float aniso = (float)atof(val.c_str());
							samplerInfo.anisotropyEnable = aniso <= 0 ? VK_FALSE : VK_TRUE;
							samplerInfo.maxAnisotropy = aniso;
						} else if (id == "borderColor")				samplerInfo.borderColor = atobordercolor(val);
						else if (id == "unnormalizedCoordinates")	samplerInfo.unnormalizedCoordinates = val == "true" ? VK_TRUE : VK_FALSE;
						else if (id == "compareOp") {
							VkCompareOp cmp = atocmp(val);
							samplerInfo.compareEnable = cmp == VK_COMPARE_OP_MAX_ENUM ? VK_FALSE : VK_TRUE;
							samplerInfo.compareOp = cmp;
						} else if (id == "mipmapMode") samplerInfo.mipmapMode = atomipmapmode(val);
						else if (id == "minLod") samplerInfo.minLod = (float)atof(val.c_str());
						else if (id == "maxLod") samplerInfo.maxLod = (float)atof(val.c_str());
						else if (id == "mipLodBias") samplerInfo.mipLodBias = (float)atof(val.c_str());
					}

					staticSamplers.push_back(make_pair(name, samplerInfo));

				} else if (*it == "array") {
					if (++it == words.end()) return false;
					string name = *it;
					if (++it == words.end()) return false;
					arrays.push_back(make_pair(name, (uint32_t)atoi(it->c_str())));
				}
				break;
			}
		}
	}

	uint32_t vc = (uint32_t)variants.size();
	output.write(reinterpret_cast<const char*>(&vc), sizeof(uint32_t));
	for (const auto& variant : variants) {
		auto variantOptions = options;

		uint32_t vsize = (uint32_t)variant.size();
		output.write(reinterpret_cast<const char*>(&vsize), sizeof(uint32_t));
		for (const auto& kw : variant) {
			if (kw.empty()) continue;
			uint32_t klen = (uint32_t)kw.length();
			output.write(reinterpret_cast<const char*>(&klen), sizeof(uint32_t));
			output.write(kw.c_str(), klen);
			variantOptions.AddMacroDefinition(kw);
		}

		unordered_map<string, pair<uint32_t, VkDescriptorSetLayoutBinding>> descriptorBindings;
		unordered_map<string, VkPushConstantRange> pushConstants;

		auto writeBindingsAndConstants = [&]() {
			uint32_t bc = (uint32_t)descriptorBindings.size();
			output.write(reinterpret_cast<const char*>(&bc), sizeof(uint32_t));
			for (const auto& b : descriptorBindings) {
				uint32_t descriptorCount = b.second.second.descriptorCount;
				VkSamplerCreateInfo samplerInfo;
				uint32_t static_sampler = 0;
				for (const auto& s : staticSamplers)
					if (s.first == b.first) {
						static_sampler = 1;
						samplerInfo = s.second;
						break;
					}
				for (const auto& s : arrays)
					if (s.first == b.first) {
						descriptorCount = s.second;
						break;
					}

				uint32_t nlen = (uint32_t)b.first.length();
				output.write(reinterpret_cast<const char*>(&nlen), sizeof(uint32_t));
				output.write(b.first.c_str(), nlen);
				output.write(reinterpret_cast<const char*>(&b.second.first), sizeof(uint32_t));
				output.write(reinterpret_cast<const char*>(&b.second.second.binding), sizeof(uint32_t));
				output.write(reinterpret_cast<const char*>(&descriptorCount), sizeof(uint32_t));
				output.write(reinterpret_cast<const char*>(&b.second.second.descriptorType), sizeof(uint32_t));
				output.write(reinterpret_cast<const char*>(&b.second.second.stageFlags), sizeof(VkShaderStageFlagBits));
				output.write(reinterpret_cast<const char*>(&static_sampler), sizeof(uint32_t));
				if (static_sampler) output.write(reinterpret_cast<const char*>(&samplerInfo), sizeof(VkSamplerCreateInfo));
			}

			bc = (uint32_t)pushConstants.size();
			output.write(reinterpret_cast<const char*>(&bc), sizeof(uint32_t));
			for (const auto& b : pushConstants) {
				uint32_t nlen = (uint32_t)b.first.length();
				output.write(reinterpret_cast<const char*>(&nlen), sizeof(uint32_t));
				output.write(b.first.c_str(), nlen);
				output.write(reinterpret_cast<const char*>(&b.second.offset), sizeof(uint32_t));
				output.write(reinterpret_cast<const char*>(&b.second.size), sizeof(uint32_t));
				output.write(reinterpret_cast<const char*>(&b.second.stageFlags), sizeof(VkShaderStageFlagBits));
			}
		};

		uint32_t is_compute = !!kernels.size();
		output.write(reinterpret_cast<const char*>(&is_compute), sizeof(uint32_t));
		if (is_compute) {
			auto stageOptions = variantOptions;
			stageOptions.AddMacroDefinition("SHADER_STAGE_COMPUTE");
			uint32_t kernelc = (uint32_t)kernels.size();
			output.write(reinterpret_cast<const char*>(&kernelc), sizeof(uint32_t));
			for (const auto& k : kernels) {
				descriptorBindings.clear();
				if (!CompileStage(compiler, stageOptions, output, source, filename, shaderc_compute_shader, k, descriptorBindings, pushConstants)) return false;

				writeBindingsAndConstants();
			}
		} else {
			uint32_t stagec = (uint32_t)stages.size();
			output.write(reinterpret_cast<const char*>(&stagec), sizeof(uint32_t));
			for (const auto& s : stages) {
				auto stageOptions = variantOptions;
				VkShaderStageFlagBits vkstage;
				switch (s.first) {
				case shaderc_vertex_shader:
					vkstage = VK_SHADER_STAGE_VERTEX_BIT;
					stageOptions.AddMacroDefinition("SHADER_STAGE_VERTEX");
					break;
				case shaderc_fragment_shader:
					vkstage = VK_SHADER_STAGE_FRAGMENT_BIT;
					stageOptions.AddMacroDefinition("SHADER_STAGE_FRAGMENT");
					break;
				case shaderc_geometry_shader:
					vkstage = VK_SHADER_STAGE_GEOMETRY_BIT;
					stageOptions.AddMacroDefinition("SHADER_STAGE_GEOMETRY");
					break;
				case shaderc_tess_control_shader:
					vkstage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
					stageOptions.AddMacroDefinition("SHADER_STAGE_TESSELLATION_CONTROL");
					break;
				case shaderc_tess_evaluation_shader:
					vkstage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
					stageOptions.AddMacroDefinition("SHADER_STAGE_TESSELLATION_EVALUATION");
					break;
				}

				output.write(reinterpret_cast<const char*>(&vkstage), sizeof(VkShaderStageFlagBits));
				if (!CompileStage(compiler, stageOptions, output, source, filename, s.first, s.second, descriptorBindings, pushConstants)) return false;
			}

			writeBindingsAndConstants();
		}
	}

	output.write(reinterpret_cast<const char*>(&colorMask), sizeof(VkColorComponentFlags));
	output.write(reinterpret_cast<const char*>(&renderQueue), sizeof(uint32_t));
	output.write(reinterpret_cast<const char*>(&cullMode), sizeof(VkCullModeFlags));
	output.write(reinterpret_cast<const char*>(&fillMode), sizeof(VkPolygonMode));
	output.write(reinterpret_cast<const char*>(&blendMode), sizeof(BlendMode));
	output.write(reinterpret_cast<const char*>(&depthStencilState), sizeof(VkPipelineDepthStencilStateCreateInfo));
	return true;
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <input> <output>\n", argv[0]);
		return EXIT_FAILURE;
	}

	const char* inputFile = argv[1];
	const char* outputFile = argv[2];

	printf("Compiling %s\n", inputFile);
	
	options.SetIncluder(make_unique<Includer>());
	options.SetSourceLanguage(shaderc_source_language_hlsl);

	ofstream output(outputFile, ios::binary);

	Compiler* compiler = new Compiler();
	if (!Compile(compiler, inputFile, output))
		return EXIT_FAILURE;
	output.close();

	delete compiler;

	return EXIT_SUCCESS;
}