#pragma once

#include "core.h"

#include "compile.h"

namespace Medea {
    template<typename PushConstant>
    struct ComputeShader {
        vk::raii::Pipeline pipeline;
        vk::raii::PipelineLayout layout;
        vk::raii::DescriptorSetLayout descLayout;

        static ComputeShader make(vk::raii::Device& device, std::string_view srcPath, const std::vector<vk::DescriptorSetLayoutBinding>& descBindings) {
            std::string src = readFile(srcPath).value();

            vk::raii::ShaderModule mdl = compileShader<ShaderStage::compute>(device, src, srcPath).value();

            auto pushRange = vk::PushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(PushConstant));

            bool hasDescriptors = descBindings.size() > 0;

            vk::raii::DescriptorSetLayout descLayout = !hasDescriptors ? 
                  vk::raii::DescriptorSetLayout(nullptr)
                : vk::raii::DescriptorSetLayout(device, vk::DescriptorSetLayoutCreateInfo({}, descBindings));

            vk::PipelineLayoutCreateInfo lci;
            
            if (hasDescriptors) lci = vk::PipelineLayoutCreateInfo({}, *descLayout, pushRange);
            else lci = vk::PipelineLayoutCreateInfo({}, {}, pushRange);

            vk::raii::PipelineLayout layout(device, lci);

            vk::PipelineShaderStageCreateInfo ssci({}, vk::ShaderStageFlagBits::eCompute, *mdl, "main");

            vk::ComputePipelineCreateInfo cpci({}, ssci, *layout);

            vk::raii::Pipeline out = device.createComputePipeline(nullptr, cpci);

            return ComputeShader{std::move(out), std::move(layout), std::move(descLayout)};
        }

        static ComputeShader make(vk::raii::Device& device, std::string_view srcPath) {
            std::vector<vk::DescriptorSetLayoutBinding> noDesc;
            return make(device, srcPath, noDesc);
        }

        static ComputeShader* makePtr(vk::raii::Device& device, std::string_view srcPath) {
            return new ComputeShader(std::move(make(device, srcPath)));
        }

        void setPush(vk::CommandBuffer cmd, const PushConstant& pc) {
            cmd.pushConstants<PushConstant>(layout, vk::ShaderStageFlagBits::eCompute, 0, pc);
        }
    };
};