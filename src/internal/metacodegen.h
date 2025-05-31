#pragma once

#include "pfrtools.h"

#include <regex>

//this shouldn't /need/ core.h, it just needs BufferRef, but it's not that big of a deal
#include "medea/core.h"
#include "medea/primitives.h"

#include <memory>
#include <string>
#include <sstream>
#include <string_view>

#include "metaio.h"

namespace Medea::Internal {

    #pragma region VkData

    template<typename T>
    struct VKData;

    template<typename T>
    void defaultVertexWrite(std::stringstream& buf, int ithField, std::string_view argName, std::string_view inout) {
        buf << "layout (location = "<<ithField<<") "<<inout<<" "<<VKData<T>::glslName<<" "<<argName<<";\n";
    }

    template<typename T>
    void defaultUniformWrite(std::stringstream& buf, int descSet, int ithField, int& bindings, std::string_view argName) {
        buf << "layout (set = "<<descSet<<", location = "<<ithField<<") uniform "<<VKData<T>::glslName<<" "<<argName<<";\n";
    }

    template<typename T>
    void opaqueUniformWrite(std::stringstream& buf, int descSet, int ithField, int& bindings, std::string_view argName) {
        buf << "layout (set = "<<descSet<<", binding = "<<ithField<<") uniform "<<VKData<T>::glslName<<" "<<argName<<";\n";
    }

    template<class T>
    struct VKData {
        static std::string postfix() {
            return "";
        }
    };


    template<>
    struct VKData<glm::avec4> {
        constexpr static std::string glslName = "vec4";
        //const static size_t argc = 4;

        static void vertexWrite(std::stringstream& buf,  int ithField, std::string_view argName, std::string_view inout) {
            defaultVertexWrite<glm::avec4>(buf, ithField, argName, inout);
        } 
        static void uniformWrite(std::stringstream& buf, int descSet, int ithField, int& bindings, std::string_view argName) {
            defaultUniformWrite<glm::avec4>(buf, descSet, ithField, bindings, argName);
        }
    };

    template<>
    struct VKData<glm::avec3> {
        //const static size_t argc = 3;
        constexpr static std::string glslName = "vec3";


        static void vertexWrite(std::stringstream& buf, int ithField, std::string_view argName, std::string_view inout) {
            defaultVertexWrite<glm::avec3>(buf, ithField, argName, inout);
        } 
        static void uniformWrite(std::stringstream& buf, int descSet, int ithField, int& bindings, std::string_view argName) {
            defaultUniformWrite<glm::avec3>(buf, descSet, ithField, bindings, argName);
        }
    };

    template<>
    struct VKData<glm::avec2> {
        //const static size_t argc = 2;
        constexpr static std::string glslName = "vec2";

        static void vertexWrite(std::stringstream& buf, int ithField, std::string_view argName, std::string_view inout) {
            defaultVertexWrite<glm::avec2>(buf, ithField, argName, inout);
        } 
        static void uniformWrite(std::stringstream& buf, int descSet, int ithField, int& bindings, std::string_view argName) {
            defaultUniformWrite<glm::avec2>(buf, descSet, ithField, bindings, argName);
        }
    };

    template<>
    struct VKData<glm::float32> {
        constexpr static std::string glslName = "float";

        static void vertexWrite(std::stringstream& buf, int ithField, std::string_view argName, std::string_view inout) {
            defaultVertexWrite<glm::float32>(buf, ithField, argName, inout);
        } 
        static void uniformWrite(std::stringstream& buf, int descSet, int ithField, int& bindings, std::string_view argName) {
            defaultUniformWrite<glm::float32>(buf, descSet, ithField, bindings, argName);
        }
    };

    template<>
    struct VKData<int32_t> {
        constexpr static std::string glslName = "int";

        static void vertexWrite(std::stringstream& buf, int ithField, std::string_view argName, std::string_view inout) {

            std::string newInout(inout);

            if (inout == "in") newInout = "flat "+newInout; //<- fragile, but w/e it works

            defaultVertexWrite<int32_t>(buf, ithField, argName, newInout);
        } 
        static void uniformWrite(std::stringstream& buf, int descSet, int ithField, int& bindings, std::string_view argName) {
            defaultUniformWrite<int32_t>(buf, descSet, ithField, bindings, argName);
        }

    };

    template<>
    struct VKData<uint32_t> {
        constexpr static std::string glslName = "uint";

        static void vertexWrite(std::stringstream& buf, int ithField, std::string_view argName, std::string_view inout) {
            std::string newInout(inout);

            if (inout == "in") newInout = "flat "+newInout;//<- fragile, but w/e it works

            defaultVertexWrite<uint32_t>(buf, ithField, argName, newInout);
        } 
        static void uniformWrite(std::stringstream& buf, int descSet, int ithField, int& bindings, std::string_view argName) {
            defaultUniformWrite<uint32_t>(buf, descSet, ithField, bindings, argName);
        }

    };


    template<>
    struct VKData<int64_t> {
        constexpr static std::string glslName = "int64_t";

        static void vertexWrite(std::stringstream& buf, int ithField, std::string_view argName, std::string_view inout) {

            std::string newInout(inout);

            if (inout == "in") newInout = "flat "+newInout; //<- fragile, but w/e it works

            defaultVertexWrite<int64_t>(buf, ithField, argName, newInout);
        } 
        static void uniformWrite(std::stringstream& buf, int descSet, int ithField, int& bindings, std::string_view argName) {
            defaultUniformWrite<int64_t>(buf, descSet, ithField, bindings, argName);
        }

    };

    template<>
    struct VKData<uint64_t> {
        constexpr static std::string glslName = "uint64_t";

        static void vertexWrite(std::stringstream& buf, int ithField, std::string_view argName, std::string_view inout) {
            std::string newInout(inout);

            if (inout == "in") newInout = "flat "+newInout;//<- fragile, but w/e it works

            defaultVertexWrite<uint64_t>(buf, ithField, argName, newInout);
        } 
        static void uniformWrite(std::stringstream& buf, int descSet, int ithField, int& bindings, std::string_view argName) {
            defaultUniformWrite<uint64_t>(buf, descSet, ithField, bindings, argName);
        }

    };


    template<>
    struct VKData<BufferRef> {
        constexpr static std::string glslName = "uint64_t";

        static void vertexWrite(std::stringstream& buf, int ithField, std::string_view argName, std::string_view inout) {
            std::string newInout(inout);

            if (inout == "in") newInout = "flat "+newInout;

            defaultVertexWrite<uint64_t>(buf, ithField, argName, newInout);
        } 
        static void uniformWrite(std::stringstream& buf, int descSet, int ithField, int& bindings, std::string_view argName) {
            defaultUniformWrite<uint64_t>(buf, descSet, ithField, bindings, argName);
        }

    };

    template<>
    struct VKData<glm::mat4> {
        constexpr static std::string glslName = "mat4";

        static void vertexWrite(std::stringstream& buf, int ithField, std::string_view argName, std::string_view inout) {
            defaultVertexWrite<glm::mat4>(buf, ithField, argName, inout);
        } 
        static void uniformWrite(std::stringstream& buf, int descSet, int ithField, int& bindings, std::string_view argName) {
            defaultUniformWrite<glm::mat4>(buf, descSet, ithField, bindings, argName);
        }
    };

    template<>
    struct VKData<bool> {
        constexpr static std::string glslName = "bool";

        static void vertexWrite(std::stringstream& buf, int ithField, std::string_view argName, std::string_view inout) {
            defaultVertexWrite<glm::mat4>(buf, ithField, argName, inout);
        } 
        static void uniformWrite(std::stringstream& buf, int descSet, int ithField, int& bindings, std::string_view argName) {
            defaultUniformWrite<glm::mat4>(buf, descSet, ithField, bindings, argName);
        }
    };


    template<class T>
    struct VecTest : std::false_type {};

    template<class T>
    struct VecTest<std::vector<T>> : std::true_type {};

    template<class T, int IDX>
    struct vkStructFieldReflect {
        void operator()(std::stringstream& str) {
            auto strName = std::string(boost::pfr::get_name<IDX, T>());

            using FTYPE = boost::pfr::tuple_element_t<IDX, T>;

            str << "\t" << VKData<FTYPE>::glslName << " " << strName << ";\n";

            vkStructFieldReflect<T, IDX+1>()(str);
        }
    };

    //base case
    template<class T>
    struct vkStructFieldReflect<T, TotalElements<T>::value> {

        void operator()(std::stringstream& str) {}
    };

    template<class T>
    void structFieldReflect(std::stringstream& str) {

        vkStructFieldReflect<T,0>()(str);
    }

    template<class Type, int IDX>
    struct writeVertexIOLoop {
        constexpr void operator()(std::stringstream& buf, std::string_view inout, uint32_t offset) {
            typedef boost::pfr::tuple_element_t<IDX, Type> FType;

            VKData<FType>::vertexWrite(buf, IDX + offset, boost::pfr::get_name<IDX, Type>(), inout);

            writeVertexIOLoop<Type, IDX+1>()(buf, inout, offset);
        }
    };

    template<class Type>
    struct writeVertexIOLoop<Type, boost::pfr::tuple_size<Type>::value> {
        constexpr void operator()(std::stringstream& buf, std::string_view inout, uint32_t offset) {}
    };

    template<class Type>
    struct writeShaderVertexIO {
        constexpr void operator()(std::stringstream& buf, std::string_view inout, uint32_t offset = 0) {
            writeVertexIOLoop<Type, 0>()(buf, inout, offset);
            buf<<"\n";
        }
    };

    template<typename Type, int IDX>
    struct writeUniformLoop {
        constexpr void operator()(std::stringstream& buf, int set, int& binding) {
            typedef boost::pfr::tuple_element_t<IDX, Type> FType;

            VKData<FType>::uniformWrite(buf, set, IDX, binding, boost::pfr::get_name<IDX, Type>());

            writeUniformLoop<Type, IDX+1>()(buf, set, binding);
        }
    };

    template<typename Type>
    struct writeUniformLoop<Type, TotalElements<Type>::value> {
        constexpr void operator()(std::stringstream& buf, int set, int& binding) {}
    };

    template<typename Type>
    struct writeShaderUniforms {
        constexpr void operator()(std::stringstream& buf, int set, int& binding) {
            writeUniformLoop<Type, 0>()(buf, set, binding);
            buf << "\n";
        }
    };

    template<class Uniform>
    inline void shaderUniforms(std::stringstream& source, int set) {
        int binding = 1;

        writeShaderUniforms<Uniform>()(source, set, binding);
    }


    #pragma endregion VkData

    inline bool findReplace(std::string& str, std::string_view findStr, std::string_view replaceStr) {
        size_t idx = str.find(findStr);

        if (idx > str.size()) return false;

        str = str.replace(idx, findStr.size(), replaceStr);

        return true;
    }

    template<typename U>
    void cppStructToGLSL(std::stringstream& out, std::string_view name) {
        out << "struct " << name << " {\n";

        Internal::structFieldReflect<U>(out);

        out << "};\n";
    };


    inline void printStructArray(std::stringstream& out, std::string_view arrayName, std::string_view structName, bool hasHeader = false) {
        out << "layout(buffer_reference, std430) readonly buffer " << arrayName<< "{\n";

        if (hasHeader) out << "\tArrayHeader _header;\n";

        out << "\t" <<structName<<" data[];\n"
            << "};\n";
    }


    template<typename U>
    inline VMaterialFragment materialToLibFrag(std::string_view initialSrc, const std::string& entryName, uint32_t materialID) {
        std::string base(initialSrc);

        std::string uStructName = entryName+"Uniform";
        std::string uArrayName = entryName+"UniformArray";
        std::string mainName = entryName+"Main";

        bool foundEntry = findReplace(base, "_medeaMain(model, view, projection, u)", mainName+"(mat4 model, mat4 view, mat4 projection, "+uStructName+" u)");
        assert(foundEntry);

        std::stringstream out;

        cppStructToGLSL<U>(out, uStructName);

        printStructArray(out, uArrayName, uStructName, true);

        out << base;

        return {out.str(), uStructName, uArrayName, mainName, materialID};
    }


    template<typename U, typename V>
    inline VMaterialVertex materialToLibVtx(std::string_view initialSrc, const std::string& entryName, uint32_t materialID) {
        std::string base(initialSrc);

        std::string uStructName = entryName+"Uniform";
        std::string uArrayName = entryName+"UniformArray";
        std::string vStructName = entryName+"Vertex";
        std::string vArrayName = entryName+"VertexArray";
        std::string mainName = entryName+"Main";

        bool foundEntry = findReplace(base, "_medeaMain(model, view, projection, pos, normal, v, u)", mainName+"(mat4 model, mat4 view, mat4 projection, vec3 pos, vec3 normal, "+vStructName+" v, "+uStructName+" u)");
        assert(foundEntry);

        std::stringstream out;

        cppStructToGLSL<V>(out, vStructName);

        printStructArray(out, vArrayName, vStructName);

        cppStructToGLSL<U>(out, uStructName);

        printStructArray(out, uArrayName, uStructName, true);


        out << base;

        return {out.str(), uStructName, uArrayName, vStructName, vArrayName, mainName, materialID};
    }

    inline void vmaterialHeader(std::stringstream& out) {
        out << "#version 460\n"
            << "#extension GL_EXT_buffer_reference : require\n"
            << "#extension GL_EXT_nonuniform_qualifier : require\n"
            << "#extension GL_EXT_shader_explicit_arithmetic_types : require\n"
            << "#extension GL_KHR_shader_subgroup_vote : require\n\n";
    }
    
    struct IntrinsicV2F {
        uint32_t _fragInstanceIndex;
    };

    template<typename V2F, typename FOut>
    std::string vmaterialSrcFrag(std::span<VMaterialFragment> arr, std::string_view bonusSrc) {
        std::stringstream out;

        vmaterialHeader(out);

        out << "// === bonus src begin \n"<<bonusSrc<<"//bonus src end\n";

        writeShaderVertexIO<V2F>()(out, "in");
        writeShaderVertexIO<IntrinsicV2F>()(out, "in", TotalElements<V2F>::value);
        writeShaderVertexIO<FOut>()(out, "out");


        std::string globalBuiltins = readFile("./shader/shared/builtins.slib").value();
        std::string fragBuiltins = readFile("./shader/shared/builtins-frag.slib").value();

        out << globalBuiltins << "\n" << fragBuiltins << "\n";

        for (auto& vmfrag : arr) {
            out << vmfrag.src;
        }


        out << "\n\nvoid main() {\n"

            <<"\tRenderEntity entity = _getRenderEntity();\n"
            <<"\tmat4 model = _medeaEntityToModel(entity);\n"
            <<"\tmat4 view = _medeaGetView();\n"
            <<"\tmat4 proj = _medeaGetProj();\n"    

            //<<"\tif (_push.depthOnly != 0) return;"
            << "\tswitch (entity.materialID) {\n";

        for (auto& vmfrag : arr) {
            out << "\t\tcase "<<vmfrag.materialID<<":\n"
                << "\t\t"<<vmfrag.entryFuncName<<"("
                    <<"model, view, proj,"
                    <<vmfrag.uArrayName<<"(entity.materialUniformArrayAddress).data[entity.materialUniformIdx]);\n"
                << "\t\tbreak;\n";
        }

        out << "\t}\n"
            << "\t fragColor.rgb = colorCorrect(fragColor.rgb);\n"
            << "\t fragColor.rgb += vec3(bayerDither());"
            << "}\n";

        return out.str();
    }

    ///WARN: this is fragile & highly specialized to my use case & could be coded to be more reusable
    template<typename V2F>
    std::string vmaterialSrcVtx(std::span<VMaterialVertex> arr, std::string_view bonusSrc) {
        std::stringstream out;

        vmaterialHeader(out);

        out << "// === bonus src begin \n"<<bonusSrc<<"//bonus src end\n";

        writeShaderVertexIO<V2F>()(out, "out");
        writeShaderVertexIO<IntrinsicV2F>()(out, "out", TotalElements<V2F>::value);


        std::string globalBuiltins = readFile("./shader/shared/builtins.slib").value();

        out << globalBuiltins << "\n";


        std::string vertBuiltins = readFile("./shader/shared/builtins-vert.slib").value();

        out << vertBuiltins << "\n";

        for (auto& vmfrag : arr) {
            out << vmfrag.src;
        }


        out << "\n\nvoid main() {\n"
            <<"\tRenderEntity entity = _getRenderEntity();\n"
            <<"\tmat4 model = _medeaEntityToModel(entity);\n"
            <<"\tmat4 view = _medeaGetView();\n"
            <<"\tmat4 proj = _medeaGetProj();\n"
            <<"\tvec3 pos; vec3 normal; _medeaGetPosNormal(entity, pos, normal);"
            <<"\tswitch (entity.materialID) {\n";

        for (auto& vmfrag : arr) {
            out << "\t\tcase "<<vmfrag.materialID<<":\n"
            
                << "\t\t"<<vmfrag.entryFuncName<<"("
                    <<"model, view, proj, pos, normal, "
                    <<vmfrag.vArrayName<<"(entity.meshAddress).data[gl_VertexIndex],  "
                    <<vmfrag.uArrayName<<"(entity.materialUniformArrayAddress).data[entity.materialUniformIdx]);\n"
                << "\t\tbreak;\n";
        } 
        out << "\t}\n";
        
        out << "\t_fragInstanceIndex = gl_InstanceIndex;\n}\n";

        return out.str();
    }
}