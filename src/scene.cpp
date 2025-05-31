#include "scene.h"
#include "constants.h"

using namespace Medea;

namespace Internal {
    struct SceneGraphState {
    };


}



Medea::Internal::PerFrameDynamicBuffers Medea::Internal::PerFrameDynamicBuffers::make(vk::Device device, VmaAllocator allocator, const glist<RenderEntity>& entities) {
    vk::BufferUsageFlags defaultBufFlags = vk::BufferUsageFlagBits::eStorageBuffer;

    const VmaMemoryUsage vmaCPU = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    const VmaMemoryUsage vmaGPU = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;


    vk::BufferUsageFlags bufDefault = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;

    //size_t entitySize = entities.size;
    //size_t totalVertices = 0;
    //for (auto& e : entities) totalVertices += e.meshSize;

    return Medea::Internal::PerFrameDynamicBuffers {
        //std::move(cpuEntities),
        AllocatedBuffer(device, allocator, entities.getBuffer().size, 
            bufDefault | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst, {}, vmaGPU)
    };
}


struct alignas(sizeof(float) * 4) RenderGlobal {
    glm::mat4 camView, camProjection;
    BufferRef drawcallUniformArrayAddress;
    BufferRef lightBuffer;
};


RenderWorld::RenderWorld(Core& core, vk::CommandBuffer cmd, GPUSceneGraph& graph)
    : entities(core.allocator, core.device, cmd),
      uniformDeleteCallback([&] (size_t materialID, size_t materialIdx) {
        graph.deleteUniform(materialID, materialIdx);
      }) {
    //
}

std::vector<vk::DescriptorSetLayoutBinding> shadowLayoutBinding() {
    vk::DescriptorSetLayoutBinding binding(0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute, {});

    binding.setDescriptorCount(RenderConstants::maxLights);

    return {binding};
}

std::vector<vk::DescriptorSetLayoutBinding> scatterBinding() {
    vk::DescriptorSetLayoutBinding b0(0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute, {});
    vk::DescriptorSetLayoutBinding b1(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute, {});
    vk::DescriptorSetLayoutBinding bf(2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute, {});

    b0.setDescriptorCount(1);
    b1.setDescriptorCount(1);
    bf.setDescriptorCount(RenderConstants::maxLights);

    return {b0, b1, bf};
}

DescriptorAllocator makeVolShadowDescAllocator(vk::raii::Device& device) {
    std::vector<DescriptorAllocator::PoolSizeRatio> poolRatios = 
        {DescriptorAllocator::PoolSizeRatio(vk::DescriptorType::eStorageBuffer, RenderConstants::maxLights),
         DescriptorAllocator::PoolSizeRatio(vk::DescriptorType::eCombinedImageSampler, RenderConstants::maxLights)};

    return DescriptorAllocator::make(device, 5, poolRatios);
}

vk::ImageCreateInfo volLightingImageICI(Core& core) {
    return vk::ImageCreateInfo(
        {}, vk::ImageType::e3D, vk::Format::eR16G16B16A16Sfloat, 
        vk::Extent3D(VolLighting::VOL_LIGHTING_RES.x, VolLighting::VOL_LIGHTING_RES.y, VolLighting::VOL_LIGHTING_RES.z),
        1, 1, 
        vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst, 
        vk::SharingMode::eExclusive, core.graphicsQueueFamily);
}

vk::ImageViewCreateInfo volLightingImageIVCI() {
    vk::ImageViewCreateInfo ivci({}, nullptr, vk::ImageViewType::e3D, vk::Format::eR16G16B16A16Sfloat);
    ivci.subresourceRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

    return ivci;
}

vk::SamplerCreateInfo bilinearClampedSCI() {
    return vk::SamplerCreateInfo({}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eNearest,
        vk::SamplerAddressMode::eClampToEdge, vk::SamplerAddressMode::eClampToEdge, vk::SamplerAddressMode::eClampToEdge);
}

vk::SamplerCreateInfo volLightingSCI() {
    return vk::SamplerCreateInfo({}, vk::Filter::eNearest, vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest,
        vk::SamplerAddressMode::eClampToEdge, vk::SamplerAddressMode::eClampToEdge, vk::SamplerAddressMode::eClampToEdge);
}


GPUSceneGraph::GPUSceneGraph(Core& core, vk::CommandBuffer cmd, BindlessTextureArray& texRef)
      //magic numbers in froxelArray from constants in froxel.slib
    : froxelArray(core.device, core.allocator, Froxel::FROXELS_W*Froxel::FROXELS_W*Froxel::FROXELS_Z*(sizeof(uint16_t) + sizeof(uint16_t) * RenderConstants::maxLightsPerTile), 
                    vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst,
                    VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE),
      broadphaseCullShader(decltype(broadphaseCullShader)::make(core.device, "./shader/broadphaseCull.comp")),
      clusterLightShader(decltype(clusterLightShader)::make(core.device, "./shader/setupTiled.comp")),
      shadowTransmittanceShader(decltype(shadowTransmittanceShader)::make(core.device, "./shader/shadowTransmittance.comp", shadowLayoutBinding())),
      volScatteringShader(decltype(volScatteringShader)::make(core.device, "./shader/volumetricScattering.comp", scatterBinding())),
      volAccumulateShader(decltype(volAccumulateShader)::make(core.device, "./shader/accumulateVolumetricLighting.comp", scatterBinding())),

      shadowTransmittanceArrayAllocator(makeVolShadowDescAllocator(core.device)),
      lights(core.allocator, core.device, cmd, Medea::RenderConstants::maxLights),
      textures(texRef),
      volLightingImage(AllocatedImage::make(core, volLightingImageICI(core), volLightingImageIVCI(), vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e3D, 0, false)),
      volLightingSampler(core.device, bilinearClampedSCI()) {}



void GPUSceneGraph::render(Core& core, vk::CommandBuffer cmd, CleanupJobQueueCallback cleanup, glm::mat4 camView, glm::mat4 camProj,
                    RenderWorld& world,
                    vk::Viewport viewport,
                    const std::vector<AllocatedImage2Ref>& color, std::optional<AllocatedImage2Ref> depth, std::optional<vk::CompareOp> depthOp,
                    double currentTime) {

    Vec3 cameraWorldPos = Vec3::GlmXYZ(glm::inverse(camView) * glm::vec4(0, 0, 0, 1));
    
    glist<RenderEntity>& entities = world.entities;

    if (entities.size() == 0) {
        return;
    }

    //gotta update before we set size of dynamicBuffer.broadphaseCulledEntities
    entities.gpuUpdate(core.allocator, core.device, cmd);

    if (lights.size() > RenderConstants::maxLights) {
        std::cerr<<"ERROR: RenderEngine.render() was passed a light list with size "<<lights.size()<<", which is greater than MAX_LIGHTS ("<<RenderConstants::maxLights<<"). Truncating lightdef list."<<std::endl;
        while (lights.size() > RenderConstants::maxLights) lights.pop_back();
    }

    assert(lights.size() <= RenderConstants::maxLights);

    lights.gpuUpdate(core.allocator, core.device, cmd);

    while (volumetricShadows.size() < lights.size()) {
        vk::Extent3D span(VolShadow::VOL_SHADOW_RES.x, VolShadow::VOL_SHADOW_RES.y, VolShadow::VOL_SHADOW_RES.z);

        vk::ImageUsageFlags imgFlags = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;

        vk::ImageCreateInfo ici({}, vk::ImageType::e3D, vk::Format::eR16Unorm, span, 1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
            imgFlags, vk::SharingMode::eExclusive, core.graphicsQueueFamily);


        volumetricShadows.push_back(AllocatedImage::makeNoView(core, ici));

        vk::ImageViewCreateInfo ivci({}, *volumetricShadows.at(volumetricShadows.size()-1).image, vk::ImageViewType::e3D, vk::Format::eR16Unorm);
        ivci.subresourceRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

        volShadowViews.push_back(vk::raii::ImageView(core.device, ivci));

        volShadowSamplers.push_back(vk::raii::Sampler(core.device, bilinearClampedSCI()));
    }

    assert(volumetricShadows.size() == volShadowSamplers.size() 
           && volShadowSamplers.size() == volShadowViews.size() 
           && volShadowViews.size() >= lights.size());

    {
        std::vector<AllocatedImage2Ref> allRefs = color;
        if (depth) allRefs.push_back(depth.value());
        allRefs.push_back(*megashader->shadowAtlas.image);
        allRefs.push_back(volLightingImage);

        Medea::multiTransition(cmd, allRefs, vk::ImageLayout::eGeneral, true);

        Medea::multiTransition(cmd, volumetricShadows, vk::ImageLayout::eGeneral, false);
    }


    std::vector<vk::DeviceAddress> materialUniformPtrMapping;
    
    for (size_t i=0; i<materialSets.size(); i++) {
        auto& mset = materialSets.at(i);

        vk::DeviceAddress uptr = mset.get().update(core.allocator, *core.device, cmd);

        materialUniformPtrMapping.push_back(uptr);
    }

    vk::BufferUsageFlags bufDefault = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;
    auto gpuMaterialUniformMap = std::make_shared<AllocatedBuffer>(AllocatedBuffer::loadCPU<vk::DeviceAddress>(core.allocator, core.device, materialUniformPtrMapping, bufDefault));


    std::shared_ptr<Internal::PerFrameDynamicBuffers> dynamicBufPtr = 
        std::make_shared<Internal::PerFrameDynamicBuffers>(Internal::PerFrameDynamicBuffers::make(core.device, core.allocator, entities));

    Internal::PerFrameDynamicBuffers& dynamicBuf = *dynamicBufPtr;
    cleanup([dynamicBufPtr, gpuMaterialUniformMap] () {});

    {    
        //zero out broadphase cull header (can't be done in CS invocation)
        cmd.fillBuffer(dynamicBuf.broadphaseCulledEntities.buffer, 0, 16, 0);

        //-1 initialize froxel array, 
        cmd.fillBuffer(froxelArray.buffer, 0, froxelArray.info.size, -1);

        //clear
        auto clearRangeC = Medea::imageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        auto clearRangeDepthC = Medea::imageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT);

        vk::ImageSubresourceRange clearRange(clearRangeC);
        vk::ImageSubresourceRange clearRangeDepth(clearRangeDepthC);
        vk::ClearColorValue clearColor{0.f, 0.f, 0.f, 1.f};

        cmd.clearColorImage(color.at(0).get().image, vk::ImageLayout::eGeneral, clearColor, clearRange);
        cmd.clearDepthStencilImage(depth.value().get().image, vk::ImageLayout::eGeneral, vk::ClearDepthStencilValue(1.0, 0), clearRangeDepth);
        cmd.clearDepthStencilImage(megashader->shadowAtlas.image->image, vk::ImageLayout::eGeneral, vk::ClearDepthStencilValue(1.0, 0), clearRangeDepth);

        vk::ClearColorValue volClear{0.f, 0.f, 0.f, 0.f};



        vk::MemoryBarrier2 transferBarrier(
            vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
            vk::PipelineStageFlagBits2::eDrawIndirect | vk::PipelineStageFlagBits2::eAllGraphics, 
            vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eIndirectCommandRead);


        cmd.pipelineBarrier2(vk::DependencyInfo({}, transferBarrier, {}, {}));
    }
    using bufFlags = vk::BufferUsageFlagBits;
    auto flags = bufFlags::eStorageBuffer | bufFlags::eIndirectBuffer | bufFlags::eShaderDeviceAddress;



    RenderGlobal outGlobal{camView, camProj, entities.getBuffer(), lights.getBuffer()};

    //setup froxel array (belongs in transfer pass, since shadows are defined and rendered exogenously, and this just sets up indices for froxels)
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, clusterLightShader.pipeline);
    clusterLightShader.setPush(cmd, Medea::Internal::FroxelPush(glm::inverse(camProj * camView), camView, lights.getBuffer(), froxelArray));

    cmd.dispatch(Froxel::FROXELS_W, Froxel::FROXELS_H, Froxel::FROXELS_Z);

    //all of our draw passes go here

    //BROADPHASE CULLING

    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, broadphaseCullShader.pipeline);
    broadphaseCullShader.setPush(cmd, Medea::Internal::CullCSPush{entities.getBuffer(), dynamicBuf.broadphaseCulledEntities, *gpuMaterialUniformMap});

    {
        const int LOCAL_W = 64;
        cmd.dispatch(entities.size() / LOCAL_W + ((entities.size() % LOCAL_W) == 0 ? 0 : 1), 1, 1);
    }

    //SHADOW TRANSMITTANCE (just needs post-cull lightlist & fog list)

    multiTransition(cmd, volumetricShadows, vk::ImageLayout::eGeneral);

    {
        shadowTransmittanceShader.setPush(cmd, Medea::Internal::ShadowTransmittancePush{BufferRef::null, lights.getBuffer(), float(currentTime)});


        std::shared_ptr<vk::raii::DescriptorSet> dset
            = std::make_shared<vk::raii::DescriptorSet>(std::move(shadowTransmittanceArrayAllocator.allocate(core.device, shadowTransmittanceShader.descLayout)));
    
        std::vector<vk::DescriptorImageInfo> descImgInfo;

        descImgInfo.reserve(lights.size());
        

        for (size_t i=0; i<lights.size(); i++) {
            descImgInfo.push_back(vk::DescriptorImageInfo({}, volShadowViews.at(i), vk::ImageLayout::eGeneral));
        }

        vk::WriteDescriptorSet write(*dset, 0, 0, vk::DescriptorType::eStorageImage, descImgInfo, {}, {});

        core.device.updateDescriptorSets(write, {});

        //capture group holds onto shared ptr until frame is known to be done processing
        ///TODO: reconsider
        cleanup([dset] () {});

        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, shadowTransmittanceShader.pipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, shadowTransmittanceShader.layout, 0, **dset, {});

        cmd.dispatch(2, 2, lights.size());
    }


    //SKINNING

    //SHADOW PASS (and per-frustrum culling step?)
    multiTransition(cmd, {*megashader->shadowAtlas.image}, {vk::ImageLayout::eDepthAttachmentOptimal});

    vk::Extent3D saDim = megashader->shadowAtlas.image->imageExtent;
    vk::Viewport shadowAtlasViewport(0, 0, saDim.width, saDim.height, 0.0, 1.0);

    bool volLightReady = false;

    auto volShadowUpdateFunc = [&] (vk::DescriptorSet dset) {
        
        std::vector<vk::DescriptorImageInfo> descImgInfo;

        for (size_t i=0; i<lights.size(); i++) {
            descImgInfo.push_back(vk::DescriptorImageInfo(volShadowSamplers.at(i), volShadowViews.at(i), vk::ImageLayout::eShaderReadOnlyOptimal));
        }

        vk::ImageLayout volLightLayout = volLightReady ? vk::ImageLayout::eShaderReadOnlyOptimal : vk::ImageLayout::eGeneral;

        vk::DescriptorImageInfo vlightInfo(volLightingSampler, volLightingImage.imageView, volLightLayout);

        vk::WriteDescriptorSet w0(dset, 0, 0, vk::DescriptorType::eCombinedImageSampler, vlightInfo, {}, {});
        vk::WriteDescriptorSet write(dset, 1, 0, vk::DescriptorType::eCombinedImageSampler, descImgInfo, {}, {});

        core.device.updateDescriptorSets({w0, write}, {});
    };


    multiTransition(cmd, volumetricShadows, vk::ImageLayout::eShaderReadOnlyOptimal);

    {
        vk::MemoryBarrier2 vsBarrier(
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite,
            vk::PipelineStageFlagBits2::eDrawIndirect | vk::PipelineStageFlagBits2::eAllGraphics, 
            vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eIndirectCommandRead);


        cmd.pipelineBarrier2(vk::DependencyInfo({}, vsBarrier, {}, {}));
    }


    megashader->v2Bind(core.device, cmd, cleanup, shadowAtlasViewport, {}, *megashader->shadowAtlas.image, vk::CompareOp::eLess, volShadowUpdateFunc);

    //doing a different (indirect) drawcall per light feels suboptimal, but if I'm doing per light culling it's necessary
    // and, this means I don't need weird shader hacks to do viewport/scissor limiting
    for (LightDef ldef : lights) {
        glm::vec2 saRes = glm::vec2(RenderConstants::shadowAtlasResolution.toVec2().toGlmVec2());

        glm::avec4 atlasPosExtents = ldef.getOldAtlasPosExtents();

        glm::vec4 pe = atlasPosExtents * glm::vec4(saRes, saRes);

        vk::Viewport curViewport(pe.x, pe.y + pe.w, pe.z, -pe.w, 0.0, 1.0);
        vk::Rect2D curScissor({(i32) pe.x, (i32) pe.y}, {(u32) pe.z, (u32) pe.w});

        Internal::GPUDrivenPush curPush {
            glm::mat4(1),
            ldef.getViewProj(),
            dynamicBuf.broadphaseCulledEntities,
            BufferRef::null,
            BufferRef::null,
            0,
            currentTime
        };

        cmd.pushConstants<Medea::Internal::GPUDrivenPush>(megashader->layout, vk::ShaderStageFlagBits::eAllGraphics, 0, curPush);

        cmd.setViewport(0, curViewport);
        cmd.setScissor(0, curScissor);

        cmd.drawIndirectCount(dynamicBuf.broadphaseCulledEntities.buffer, RenderConstants::arrayHeaderSize + offsetof(RenderEntity, meshSize), 
            dynamicBuf.broadphaseCulledEntities.buffer, 0, entities.size(), sizeof(RenderEntity));
    }
    cmd.endRendering();

    multiTransition(cmd, {*megashader->shadowAtlas.image}, vk::ImageLayout::eShaderReadOnlyOptimal);

    //Extra culling for main pass?

    Internal::GPUDrivenPush mainPush {
        camView,
        camProj,
        dynamicBuf.broadphaseCulledEntities,
        lights,
        froxelArray,
        0,
        currentTime
    };


    //VOL LIGHTING PASS
    multiTransition(cmd, {*megashader->shadowAtlas.image}, {vk::ImageLayout::eShaderReadOnlyOptimal});

    //In-scattering (needs shadow atlas)
    {
        volScatteringShader.setPush(cmd, 
            Medea::Internal::ScatteringPush{
                glm::inverse(camProj * camView), 
                glm::vec4(cameraWorldPos.toGlmVec3(), currentTime),
                BufferRef::null,
                lights.getBuffer(),
                froxelArray
                });

        std::shared_ptr<vk::raii::DescriptorSet> dset
            = std::make_shared<vk::raii::DescriptorSet>(std::move(shadowTransmittanceArrayAllocator.allocate(core.device, volScatteringShader.descLayout)));
    
        std::vector<vk::DescriptorImageInfo> descImgInfo;

        descImgInfo.reserve(lights.size());
        

        for (size_t i=0; i<lights.size(); i++) {
            descImgInfo.push_back(vk::DescriptorImageInfo(volShadowSamplers.at(i), volShadowViews.at(i), vk::ImageLayout::eGeneral));
        }

        vk::DescriptorImageInfo volLightDII({}, volLightingImage.imageView, vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo shadowAtlasDII(megashader->shadowAtlas.sampler, megashader->shadowAtlas.image->imageView, vk::ImageLayout::eShaderReadOnlyOptimal);

        vk::WriteDescriptorSet w0(*dset, 0, 0, vk::DescriptorType::eStorageImage, volLightDII, {}, {});
        vk::WriteDescriptorSet w1(*dset, 1, 0, vk::DescriptorType::eCombinedImageSampler, shadowAtlasDII, {}, {});

        vk::WriteDescriptorSet w2(*dset, 2, 0, vk::DescriptorType::eCombinedImageSampler, descImgInfo, {}, {});

        core.device.updateDescriptorSets({w0, w1, w2}, {});

        //capture group holds onto shared ptr until frame is known to be done processing
        ///TODO: reconsider
        cleanup([dset] () {});

        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, volScatteringShader.pipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, volScatteringShader.layout, 0, **dset, {});

        cmd.dispatch(VolLighting::VOL_LIGHTING_RES.x / 8, VolLighting::VOL_LIGHTING_RES.y/8, VolLighting::VOL_LIGHTING_RES.z);

        vk::MemoryBarrier2 csBarrier(
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite,
            vk::PipelineStageFlagBits2::eComputeShader, 
            vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite);


        cmd.pipelineBarrier2(vk::DependencyInfo({}, csBarrier, {}, {}));


        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, volAccumulateShader.pipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, volAccumulateShader.layout, 0, **dset, {});

        cmd.dispatch(VolLighting::VOL_LIGHTING_RES.x / 8, VolLighting::VOL_LIGHTING_RES.y/8, 1);

        vk::MemoryBarrier2 accumBarrier(
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite,
            vk::PipelineStageFlagBits2::eFragmentShader, 
            vk::AccessFlagBits2::eShaderRead);

        cmd.pipelineBarrier2(vk::DependencyInfo({}, accumBarrier, {}, {}));

    }

    multiTransition(cmd, {volLightingImage}, vk::ImageLayout::eShaderReadOnlyOptimal);

    volLightReady = true;


    //PRE-Z (Note: no dependencies; can be way earlier)
    cmd.pushConstants<Medea::Internal::GPUDrivenPush>(megashader->layout, vk::ShaderStageFlagBits::eAllGraphics, 0, mainPush);

    megashader->v2Bind(core.device, cmd, cleanup, viewport, {}, depth, vk::CompareOp::eLess, volShadowUpdateFunc);
    
    cmd.drawIndirectCount(dynamicBuf.broadphaseCulledEntities.buffer, RenderConstants::arrayHeaderSize + offsetof(RenderEntity, meshSize), 
        dynamicBuf.broadphaseCulledEntities.buffer, 0, entities.size(), sizeof(RenderEntity));

    cmd.endRendering();


    {
        vk::ImageMemoryBarrier2 depthBarrier(
            vk::PipelineStageFlagBits2::eLateFragmentTests, vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests, vk::AccessFlagBits2::eDepthStencilAttachmentRead,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eUndefined,
            core.graphicsQueueFamily,
            core.graphicsQueueFamily,
            depth.value().get().image,
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, vk::RemainingMipLevels, 0, vk::RemainingArrayLayers));

        
        cmd.pipelineBarrier2(vk::DependencyInfo({}, {}, {}, depthBarrier));
    }


    //Main pass
    megashader->v2Bind(core.device, cmd, cleanup, viewport, color, depth, vk::CompareOp::eEqual, volShadowUpdateFunc);

    cmd.drawIndirectCount(dynamicBuf.broadphaseCulledEntities.buffer, RenderConstants::arrayHeaderSize + offsetof(RenderEntity, meshSize), 
        dynamicBuf.broadphaseCulledEntities.buffer, 0, entities.size(), sizeof(RenderEntity));

    cmd.endRendering();

}