#pragma once

#include "core.h"
#include "shaderc/shaderc.hpp"

namespace Medea {

    enum class ShaderStage {
        compute,
        vertex,
        fragment,
        mesh
    };

    namespace Internal {
        consteval shaderc_shader_kind scShaderStage(ShaderStage ss) {
            switch (ss) {
                case ShaderStage::compute:
                return shaderc_shader_kind::shaderc_compute_shader;

                case ShaderStage::vertex:
                return shaderc_shader_kind::shaderc_glsl_vertex_shader;

                case ShaderStage::fragment:
                return shaderc_shader_kind::shaderc_glsl_fragment_shader;

                case ShaderStage::mesh:
                return shaderc_shader_kind::shaderc_glsl_mesh_shader;
            }

            throw;
        }

        inline vk::ShaderStageFlagBits vkShaderStage(ShaderStage ss) {
            switch (ss) {
                case ShaderStage::compute:
                return vk::ShaderStageFlagBits::eCompute;

                case ShaderStage::vertex:
                return vk::ShaderStageFlagBits::eVertex;

                case ShaderStage::fragment:
                return vk::ShaderStageFlagBits::eFragment;

                case ShaderStage::mesh:
                return vk::ShaderStageFlagBits::eMeshEXT;
            }

            throw;
        }
    }

    struct ShaderInclude : public shaderc::CompileOptions::IncluderInterface {
        static shaderc_include_result* strsToResult(std::string_view sourceName, std::string_view content) {
            auto* out = new shaderc_include_result();

            out->content = strdup(content.data());
            out->content_length = content.size();
            out->source_name = strdup(sourceName.data());
            out->source_name_length = sourceName.size();
            out->user_data = nullptr;

            return out;
        }

        shaderc_include_result* GetInclude(const char* requested_source,
                                               shaderc_include_type type,
                                               const char* requesting_source,
                                               size_t include_depth);

    // Handles shaderc_include_result_release_fn callbacks.
        void ReleaseInclude(shaderc_include_result* data) {
            free((void*) data->content);
            free((void*) data->source_name);
            //delete data->source_name;
            delete data;
        }

        static std::unique_ptr<ShaderInclude> getIncluder();
    };

    inline void debugCompileErrorReport(std::string_view src, const std::string& msg) {
        FILE* pipe = popen("kate -i", "w");
        if (!pipe) throw;

        std::string curSrc(src);
        fprintf(pipe, "%s\n\nERROR:\n%s", curSrc.c_str(), msg.c_str());

        pclose(pipe);
    }

    extern std::optional<vk::raii::ShaderModule> compileShader(vk::raii::Device& device, std::string_view src, std::string_view debugFilename, 
        shaderc_shader_kind shaderKind);

    template<ShaderStage STAGE>
    std::optional<vk::raii::ShaderModule> compileShader(vk::raii::Device& device, std::string_view src, std::string_view debugFilename) {
        return compileShader(device, src, debugFilename, Internal::scShaderStage(STAGE));
    }

    /// naive: compiles shader from GLSL
    template<ShaderStage STAGE>
    std::optional<vk::raii::ShaderModule> loadShader(vk::raii::Device& device, std::string_view path) {
        std::string src = readFile(path).value();

        return compileShader<STAGE>(device, src, path);
    }

}