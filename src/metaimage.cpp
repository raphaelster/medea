#include "metaimage.h"

#include "stb_image.h"

#include "meta.h"

using namespace Medea;

void AllocatedImage::generateMipmaps(vk::CommandBuffer cmd, AllocatedImage& trg) {
    trg.transitionSync(cmd, vk::ImageLayout::eTransferDstOptimal);

    using psflag = vk::PipelineStageFlagBits2;
    using aflag = vk::AccessFlagBits2;

    

    vk::ImageMemoryBarrier2 barrier(psflag::eTransfer, aflag::eMemoryWrite, psflag::eTransfer, aflag::eMemoryRead | aflag::eMemoryWrite);

    barrier.setImage(trg.image);

    int32_t mipWidth = trg.imageExtent.width;
    int32_t mipHeight = trg.imageExtent.height;    

    for (uint32_t i=1; i<trg.mipDepth; i++) {

        vk::ImageSubresourceRange sub(vk::ImageAspectFlagBits::eColor, i-1, 1, 0, 1);
        barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal)
            .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
            .setSubresourceRange(sub);

        cmd.pipelineBarrier2(vk::DependencyInfo({}, {}, {}, barrier));

        vk::Offset3D zeroes = {0,0,0};
        vk::Offset3D baseMipSpan = {mipWidth, mipHeight, 1};
        vk::Offset3D nextMipSpan = {mipWidth/2, mipHeight/2, 1};

        //vk::ImageBlit2()

        vk::ImageSubresourceLayers srcLayer(vk::ImageAspectFlagBits::eColor, i-1, 0, 1);
        vk::ImageSubresourceLayers dstLayer(vk::ImageAspectFlagBits::eColor, i, 0, 1);

        vk::ImageBlit2 blit(srcLayer, {zeroes, baseMipSpan}, dstLayer, {zeroes, nextMipSpan});

        cmd.blitImage2(vk::BlitImageInfo2(trg.image, vk::ImageLayout::eTransferSrcOptimal, trg.image, vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eLinear));

        mipWidth /= 2;
        mipHeight /= 2;
    }

    vk::ImageSubresourceRange sub(vk::ImageAspectFlagBits::eColor, trg.mipDepth-1, 1, 0, 1);

    barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal)
        .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
        .setSubresourceRange(sub);

    //set the last image in the mip chain to the global format
    cmd.pipelineBarrier2(vk::DependencyInfo({}, {}, {}, barrier));


    trg._currentLayout = vk::ImageLayout::eTransferSrcOptimal;
}


AllocatedImage AllocatedImage::make(Medea::Core& core, vk::ImageCreateInfo ici, vk::ImageViewCreateInfo ivci, vk::ImageAspectFlags aspectFlags, 
                            vk::ImageViewType viewType, uint mipDepth, bool isDepth) {
    VkImage rawImage;

    VmaAllocation rawAllocation;
    
    VmaAllocationCreateInfo imgAllocInfo = {};
    imgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    imgAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImageCreateInfo cici = ici;

    VK_REQUIRE(vmaCreateImage(core.allocator, &cici, &imgAllocInfo, &rawImage, &rawAllocation, nullptr));

    vk::raii::Image outImage(core.device, rawImage);

    vk::ImageSubresourceRange ivRange(aspectFlags, 0, vk::RemainingMipLevels, 0, vk::RemainingArrayLayers);

    //vk::ImageViewCreateInfo ivci({}, *outImage, viewType, ici.format, vk::ComponentMapping(), ivRange);

    vk::ImageViewCreateInfo();

    ivci.setImage(*outImage);
    ivci.setSubresourceRange(ivRange);

    vk::raii::ImageView outImageView(core.device, ivci);

    return AllocatedImage{std::move(outImage), std::move(outImageView), ici.format, Internal::WrappedAllocation(core.allocator, rawAllocation), ici.extent, 
                            core.graphicsQueueFamily, mipDepth, ici, isDepth};
}

AllocatedImage AllocatedImage::make(Medea::Core& core, vk::ImageCreateInfo ici, vk::ImageAspectFlags aspectFlags, 
                            vk::ImageViewType viewType, bool mipmaps, bool isDepth) {
    

    uint32_t mipDepth = std::floor(std::log2(std::max(ici.extent.width, ici.extent.height))) + 1;

    if (mipmaps) {
        ///TODO: check if log_2 mips makes sense? Means we have 1x1, 2x2, 4x4 etc mips, but the storage for that is obviously negligible
        ici.setMipLevels(mipDepth);
    }
    

    //vk::ImageViewCreateInfo ivci({}, *outImage, viewType, ici.format, vk::ComponentMapping(), ivRange);
    vk::ImageViewCreateInfo ivci({}, {}, viewType, ici.format, vk::ComponentMapping(), {});

    return make(core, ici, ivci, aspectFlags, viewType, mipDepth, isDepth);

}

AllocatedImage AllocatedImage::make(Medea::Core& core, vk::ImageUsageFlags usageFlags, vk::ImageAspectFlags aspectFlags, 
                            vk::Format format, VkExtent3D extent, bool mipmaps, bool isDepth) {
    vk::ImageType imageType = extent.depth > 1 ? vk::ImageType::e3D : vk::ImageType::e2D;
    vk::ImageViewType ivType;

    if (imageType == vk::ImageType::e2D) ivType = vk::ImageViewType::e2D;
    else if (imageType == vk::ImageType::e3D) ivType = vk::ImageViewType::e3D;
    else assert(false);

    //sample count: enable MSAA. tiling: linear for CPU readback, optimal otherwise
    //sharing mode: can be used between different queue families (setting to exclusive for now, might need shared for compute <-> )
    //optimal tiling: won't work well for GPU -> CPU transfer stuff. For implementing readPixel(), just copy a small amount of data into a CPU-visible buffer
    vk::ImageCreateInfo ici(vk::ImageCreateFlags(), imageType, format, extent, 1, 1, vk::SampleCountFlagBits::e1, 
                            vk::ImageTiling::eOptimal, usageFlags, vk::SharingMode::eExclusive, core.graphicsQueueFamily);

    return make(core, ici, aspectFlags, ivType, mipmaps, isDepth);
}
AllocatedImage AllocatedImage::makeNoView(Medea::Core& core, vk::ImageCreateInfo ici) {
    VkImage rawImage;

    VmaAllocation rawAllocation;
    
    VmaAllocationCreateInfo imgAllocInfo = {};
    imgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    imgAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImageCreateInfo cici = ici;

    VK_REQUIRE(vmaCreateImage(core.allocator, &cici, &imgAllocInfo, &rawImage, &rawAllocation, nullptr));

    vk::raii::Image outImage(core.device, rawImage);

    return AllocatedImage{std::move(outImage), nullptr, ici.format, Internal::WrappedAllocation(core.allocator, rawAllocation), ici.extent, 
                            core.graphicsQueueFamily, 1, ici, false};
}

std::shared_ptr<AllocatedImage> AllocatedImage::load(Medea::Core& core, CommandJobQueueCallback callback, 
                    std::string_view filepath, vk::ImageUsageFlags usageFlags, bool mipmaps) {
    //

    usageFlags |= vk::ImageUsageFlagBits::eTransferDst;

    if (mipmaps) usageFlags |= vk::ImageUsageFlagBits::eTransferSrc;

    vk::Format fmt = vk::Format::eR8G8B8A8Unorm;



    stbi_set_flip_vertically_on_load(true);
    int nrChannels, width, height;

    unsigned char* data = stbi_load(filepath.data(), &width, &height, &nrChannels, STBI_rgb_alpha);

    if (!data) {
        std::cerr<<"Failed to load texture; filepath is \""<<filepath<<"\"\n";
        return std::make_shared<AllocatedImage>(make(core, usageFlags, vk::ImageAspectFlagBits::eColor, fmt, vk::Extent3D(1,1,1), false, false));
    }

    vk::Extent3D dim(width, height, 1);

    std::shared_ptr<AllocatedImage> out = std::make_shared<AllocatedImage>(make(core, usageFlags, vk::ImageAspectFlagBits::eColor, fmt, dim, mipmaps, false));

    Internal::transferToGPU(callback, core.allocator, core.device, data, dim, fmt, out);

    callback([out] (vk::CommandBuffer cmd, CleanupJobQueueCallback cleanup) {
        out->generateMipmaps(cmd, *out);
    });

    stbi_image_free(data);

    return out;
}