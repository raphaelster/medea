#pragma once

#include "core.h"

#include "compile.h"

namespace Medea {

    struct PipelineBuilder {
        std::vector<vk::raii::ShaderModule> shaderModules;
        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;

        vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
        vk::PipelineRasterizationStateCreateInfo rasterizer;
        vk::PipelineColorBlendAttachmentState colorBlendAttachment;
        vk::PipelineMultisampleStateCreateInfo msaa;
        vk::raii::PipelineLayout& pipelineLayout;
        vk::PipelineDepthStencilStateCreateInfo depth;
        vk::PipelineRenderingCreateInfo renderInfo;
        vk::Format colorAttachmentFormat;

        PipelineBuilder(vk::raii::PipelineLayout& layout)
            : pipelineLayout(layout) {}

        void clear() {
            shaderModules.clear();
            shaderStages.clear();

            inputAssembly = decltype(inputAssembly){};
            rasterizer = decltype(rasterizer){};
            colorBlendAttachment = decltype(colorBlendAttachment){};
            msaa = decltype(msaa){};
            pipelineLayout.clear();
            depth = decltype(depth){};
            renderInfo = decltype(renderInfo){};
        }

        vk::raii::Pipeline build(vk::raii::Device& device) {
            vk::PipelineViewportStateCreateInfo viewport({}, 1, nullptr, 1, nullptr);

            vk::PipelineColorBlendStateCreateInfo colorBlending;
            colorBlending.setLogicOpEnable(false)
                .setLogicOp(vk::LogicOp::eCopy)
                .setAttachmentCount(1)
                .setPAttachments(&colorBlendAttachment);

            std::vector<vk::DynamicState> dynamics = {vk::DynamicState::eViewport, vk::DynamicState::eCullMode, vk::DynamicState::eScissor, 
                    vk::DynamicState::eDepthCompareOp, vk::DynamicState::eDepthTestEnable};

            vk::PipelineDynamicStateCreateInfo dynamicInfo({}, dynamics);

            vk::GraphicsPipelineCreateInfo gfxInfo;

            vk::PipelineVertexInputStateCreateInfo vtxInfo;
            vk::PipelineViewportStateCreateInfo vpInfo({}, 1, nullptr, 1, nullptr);

            gfxInfo.setStages(shaderStages)
                .setLayout(*pipelineLayout)
                .setPInputAssemblyState(&inputAssembly)
                .setPRasterizationState(&rasterizer)
                .setPMultisampleState(&msaa)
                .setPColorBlendState(&colorBlending)
                .setPDepthStencilState(&depth)
                .setPDynamicState(&dynamicInfo)
                .setPNext(&renderInfo)
                .setPVertexInputState(&vtxInfo)
                .setPViewportState(&vpInfo);


            return vk::raii::Pipeline(device, nullptr, gfxInfo);
        }

        PipelineBuilder& loadShaders(vk::raii::Device& device, std::string_view vtx, std::string_view frag) {
            shaderModules.clear();
            shaderModules.reserve(2);

            shaderModules.push_back(loadShader<ShaderStage::vertex>(device, vtx).value());
            shaderModules.push_back(loadShader<ShaderStage::fragment>(device, frag).value());

            shaderStages.push_back(vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, *shaderModules.at(0), "main"));
            shaderStages.push_back(vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, *shaderModules.at(1), "main"));

            return *this;
        }

        PipelineBuilder& setShaders(vk::raii::Device& device, std::string_view vtxSrc, std::string_view fragSrc, std::string_view vtxName, std::string_view fragName) {
            shaderModules.clear();
            shaderModules.reserve(2);

            shaderModules.push_back(compileShader<ShaderStage::vertex>(device, vtxSrc, vtxName).value());
            shaderModules.push_back(compileShader<ShaderStage::fragment>(device, fragSrc, fragName).value());

            shaderStages.push_back(vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, *shaderModules.at(0), "main"));
            shaderStages.push_back(vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, *shaderModules.at(1), "main"));

            return *this;
        }

        PipelineBuilder& setTopology(vk::PrimitiveTopology t) {
            inputAssembly.setTopology(t)
                .setPrimitiveRestartEnable(false);

            return *this;
        }

        PipelineBuilder& setPolygonMode(vk::PolygonMode mode) {
            rasterizer.setPolygonMode(mode)
                .setLineWidth(1.);

            return *this;
        }

        PipelineBuilder& setCullMode(vk::CullModeFlags cull = vk::CullModeFlagBits::eBack, vk::FrontFace front = vk::FrontFace::eCounterClockwise) {
            rasterizer.cullMode = cull;
            rasterizer.frontFace = front;

            return *this;
        }

        PipelineBuilder& setMultisampleDisable() {
            msaa.setSampleShadingEnable(false)
                .setRasterizationSamples(vk::SampleCountFlagBits::e1)
                .setMinSampleShading(1.0)
                .setPSampleMask(nullptr)
                .setAlphaToCoverageEnable(false)
                .setAlphaToOneEnable(false);

            return *this;
        }

        PipelineBuilder& setBlendingDisable() {
            using CC = vk::ColorComponentFlagBits;

            colorBlendAttachment.setColorWriteMask(CC::eR | CC::eG | CC::eB | CC::eA)
                .setBlendEnable(false);

            return *this;
        }

        PipelineBuilder& setColorAttachmentFormat(vk::Format f) {
            colorAttachmentFormat = f;

            renderInfo.setColorAttachmentFormats(colorAttachmentFormat);

            return *this;
        }

        PipelineBuilder& setDepthFormat(vk::Format f) {
            renderInfo.setDepthAttachmentFormat(f);

            return *this;
        }

        PipelineBuilder& setDepthTestDisable() {
            depth.setDepthTestEnable(false)
                .setDepthWriteEnable(false)
                .setDepthCompareOp(vk::CompareOp::eNever)
                .setDepthBoundsTestEnable(false)
                .setStencilTestEnable(false)
                .setFront({})
                .setBack({})
                .setMinDepthBounds(0.f)
                .setMaxDepthBounds(0.f);

            return *this;
        }

        PipelineBuilder& setDepthTestEnable(bool depthWriteEnable, vk::CompareOp op) {
            depth.setDepthTestEnable(true)
                .setDepthWriteEnable(depthWriteEnable)
                .setDepthCompareOp(op)
                .setDepthBoundsTestEnable(false)
                .setStencilTestEnable(false)
                .setFront({})
                .setBack({})
                .setMinDepthBounds(0.f)
                .setMaxDepthBounds(1.f);

            return *this;
        }
    };

    
}