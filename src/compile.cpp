#include "compile.h"

#include "internal/metacodegen.h"

#include "renderentity.h"


namespace Medea {
    std::unique_ptr<ShaderInclude> ShaderInclude::getIncluder() {
        return std::make_unique<ShaderInclude>();
    }

    shaderc_include_result* ShaderInclude::GetInclude(const char* requested_source,
                                            shaderc_include_type type,
                                            const char* requesting_source,
                                            size_t include_depth) {
        std::string reqSrc(requested_source);

        std::string preprocessorPrefix = "auto/";

        auto* out = new shaderc_include_result();

        if (reqSrc.length() > preprocessorPrefix.size() && reqSrc.substr(0, 5) == preprocessorPrefix) {
            std::string suff = reqSrc.substr(5);

            std::stringstream out;

            std::string includeGuardStr = "AUTO_"+suff+"_INCLUDE";

            out <<"#ifndef "<<includeGuardStr<<"\n"
                <<"#define "<<includeGuardStr<<"\n";

            if (suff == "RenderEntity") {
                Internal::cppStructToGLSL<RenderEntity>(out, "RenderEntity");
            }
            else if (suff == "LightDef") {
                Internal::cppStructToGLSL<LightDef>(out, "LightDef");
            }
            else {  
                assert(false);
            }

            out <<"\n#endif\n";

            return strsToResult("./autoreflect/"+suff, out.str());
        }
        else {
            std::optional<std::string> file = readFile(reqSrc);

            std::string error = "Couldn't find file \""+reqSrc+"\"";

            if (!file) return strsToResult("", error);

            return strsToResult(reqSrc, file.value());
        }

        assert(false);
        throw;
    }

    std::optional<vk::raii::ShaderModule> compileShader(vk::raii::Device& device, std::string_view src, std::string_view debugFilename, shaderc_shader_kind shaderKind) {
        shaderc::Compiler compiler;

        shaderc::CompileOptions options;

        options.SetTargetSpirv(shaderc_spirv_version_1_6);
        options.SetIncluder(ShaderInclude::getIncluder());
        options.SetTargetEnvironment(shaderc_target_env::shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
        //options.SetOptimizationLevel(shaderc_optimization_level::shaderc_optimization_level_performance);

        //shaderc_compile_options_set_include_callbacks()



        ///TODO: disable in release
        std::cerr<<"WARN: runtime compiled shaders include debug info"<<std::endl;
        options.SetGenerateDebugInfo();

        shaderc::SpvCompilationResult res = compiler.CompileGlslToSpv(src.data(), shaderKind, debugFilename.data(), options);

        std::string errorMsg = res.GetErrorMessage();

        //if (errorMsg.size()) std::cerr<<"shaderc error:\n"<<errorMsg<<std::endl<<"SOURCE:\n"<<src<<std::endl;;

        if (errorMsg.size()) {
            //shaderc::PreprocessedSourceCompilationResult prepRes = compiler.PreprocessGlsl(src.data(), shaderKind, debugFilename.data(), options);
            //std::string prepStr(prepRes.begin(), prepRes.end());
            
            debugCompileErrorReport(src, errorMsg);
        }

        assert(res.GetCompilationStatus() == shaderc_compilation_status_success);

        std::vector<uint32_t> spirvSrc(res.begin(), res.end());

        vk::ShaderModuleCreateInfo smci({}, spirvSrc);   

        return vk::raii::ShaderModule(device, smci);
    }

}