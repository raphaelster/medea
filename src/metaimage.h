#pragma once

#include "core.h"
#include <set>

namespace Medea {
    

    struct AllocatedImage {
        vk::raii::Image image;
        vk::raii::ImageView imageView;
        vk::Format format;
        Internal::WrappedAllocation allocation;
        VkExtent3D imageExtent;
        uint32_t queueFamily;
        uint32_t mipDepth;
        vk::ImageCreateInfo info;

        bool isDepth;

        [[nodiscard]] vk::ImageMemoryBarrier2 transition(vk::ImageLayout newLayout, bool undefinedSrc = false) {
            vk::ImageAspectFlags subresourceFlags = isDepth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;

            vk::ImageLayout oldLayout = undefinedSrc ? vk::ImageLayout::eUndefined : _currentLayout;

            vk::ImageMemoryBarrier2 barrier(
                    vk::PipelineStageFlagBits2::eAllCommands,
                    vk::AccessFlagBits2::eMemoryWrite,
                    vk::PipelineStageFlagBits2::eAllCommands,
                    vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
                    oldLayout, newLayout, queueFamily, queueFamily, image,
                    vk::ImageSubresourceRange(subresourceFlags, 0, vk::RemainingMipLevels, 0, vk::RemainingArrayLayers)
            );

            _currentLayout = newLayout;

            return barrier;
        }


        void generateMipmaps(vk::CommandBuffer cmd, AllocatedImage& trg);


        ///Just does a singular image transition. No batching, so potentially suboptimal
        void transitionSync(vk::CommandBuffer cmd, vk::ImageLayout newLayout, bool undefinedSrc = false) {
            auto imgBarrier = transition(newLayout, undefinedSrc);

            cmd.pipelineBarrier2(vk::DependencyInfo({}, {}, {}, imgBarrier));
        }
        
        void tryTransitionSync(vk::CommandBuffer cmd, vk::ImageLayout newLayout, bool undefinedSrc = false) {
            if (_currentLayout == newLayout && !undefinedSrc) return;

            transitionSync(cmd, newLayout, undefinedSrc);
        }

        //has to return a (smart) ptr because this has to asynchronously transfer data to the GPU via callback
        static std::shared_ptr<AllocatedImage> load(Medea::Core& core, CommandJobQueueCallback callback, 
                    std::string_view filepath, vk::ImageUsageFlags usageFlags, bool mipmaps);

        static AllocatedImage makeNoView(Medea::Core& core, vk::ImageCreateInfo ici);

        static AllocatedImage make(Medea::Core& core, vk::ImageCreateInfo ici, vk::ImageViewCreateInfo ivci, vk::ImageAspectFlags aspectFlags, 
                                    vk::ImageViewType viewType, uint mipDepth, bool isDepth);
        static AllocatedImage make(Medea::Core& core, vk::ImageCreateInfo ici, vk::ImageAspectFlags aspectFlags, 
                                    vk::ImageViewType viewType, bool mipmaps, bool isDepth);

        static AllocatedImage make(Medea::Core& core, vk::ImageUsageFlags usageFlags, vk::ImageAspectFlags aspectFlags, 
                                    vk::Format format, VkExtent3D extent, bool mipmaps, bool isDepth);

        vk::ImageLayout _currentLayout = vk::ImageLayout::eUndefined;
    };

    /// NOTE: causes a pipeline barrier; condense when possible
    inline void multiTransition(vk::CommandBuffer cmd, const std::vector<std::reference_wrapper<AllocatedImage>>& images, const std::vector<vk::ImageLayout>& layouts, bool undefinedSrc = false) {
        assert(images.size() == layouts.size());
        
        if (images.size() == 0) return;

        std::vector<vk::ImageMemoryBarrier2> barrier;
        barrier.reserve(images.size());
        
        for (size_t i=0; i<images.size(); i++) barrier.push_back(images[i].get().transition(layouts[i], undefinedSrc));

        vk::DependencyInfo dep({}, {}, {}, barrier);

        cmd.pipelineBarrier2(dep);
    }

    /// NOTE: causes a pipeline barrier; condense when possible
    inline void multiTransition(vk::CommandBuffer cmd, const std::vector<std::reference_wrapper<AllocatedImage>>& images, vk::ImageLayout layout, bool undefinedSrc = false) {
        if (images.size() == 0) return;

        std::vector<vk::ImageMemoryBarrier2> barrier;
        barrier.reserve(images.size());
        
        for (size_t i=0; i<images.size(); i++) barrier.push_back(images[i].get().transition(layout, undefinedSrc));

        vk::DependencyInfo dep({}, {}, {}, barrier);

        cmd.pipelineBarrier2(dep);
    }

    /// NOTE: causes a pipeline barrier; condense when possible
    inline void multiTransition(vk::CommandBuffer cmd, std::vector<AllocatedImage>& images, vk::ImageLayout layout, bool undefinedSrc = false) {
        if (images.size() == 0) return;

        std::vector<vk::ImageMemoryBarrier2> barrier;
        barrier.reserve(images.size());
        
        for (size_t i=0; i<images.size(); i++) barrier.push_back(images[i].transition(layout, undefinedSrc));

        vk::DependencyInfo dep({}, {}, {}, barrier);

        cmd.pipelineBarrier2(dep);
    }

    namespace Internal {
        struct WriteGroup {
            vk::WriteDescriptorSet write;
            std::vector<vk::DescriptorImageInfo> info;
        };
    }

    class RenderTexture {
        static vk::SamplerCreateInfo getSCI() {
            vk::SamplerCreateInfo out;

            out.compareOp = vk::CompareOp::eLessOrEqual;
            out.compareEnable = true;

            return out;
        }

        RenderTexture(vk::Format fmt, std::shared_ptr<AllocatedImage> img, vk::raii::Sampler&& smpl)
            : format(fmt), image(img), sampler(std::move(smpl)) {}

        vk::Format format;
        public:
        std::shared_ptr<AllocatedImage> image;
        vk::raii::Sampler sampler;

        static RenderTexture makeDepth(Core& core, Coord res, vk::Format fmt = vk::Format::eD32Sfloat) {
            auto img = std::make_shared<AllocatedImage>(AllocatedImage::make(core, 
                vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransferDst, 
                vk::ImageAspectFlagBits::eDepth,
                fmt, {(u32) res.x, (u32) res.y, 1}, false, true));

            return RenderTexture(
                fmt,
                img,
                vk::raii::Sampler(core.device, getSCI()));
        }

        static RenderTexture makeColor(Core& core, Coord res, vk::Format fmt = vk::Format::eR8G8B8A8Snorm) {
            auto img = std::make_shared<AllocatedImage>(AllocatedImage::make(core, 
                vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment, 
                vk::ImageAspectFlagBits::eColor,
                fmt, {(u32) res.x, (u32) res.y, 1}, false, false));

            return RenderTexture(
                fmt,
                img,
                vk::raii::Sampler(core.device, getSCI()));
        }


        static vk::DescriptorSetLayoutBinding makeBinding(uint32_t bindIdx, vk::ShaderStageFlags stages) {
            vk::DescriptorSetLayoutBinding binding(bindIdx, vk::DescriptorType::eCombinedImageSampler, stages, {});

            binding.setDescriptorCount(1);

            return binding;
        }


        std::unique_ptr<Internal::WriteGroup> getWrite(vk::DescriptorSet trg, size_t bindIdx) {
            //auto out = std::make_unique<WriteGroup>(vk::WriteDescriptorSet(target, bindIdx, 0, vk::DescriptorType::eCombinedImageSampler, {}));

            auto out = std::make_unique<Internal::WriteGroup>(vk::WriteDescriptorSet(trg, bindIdx, 0, vk::DescriptorType::eCombinedImageSampler, {}));

            assert(image->_currentLayout == vk::ImageLayout::eShaderReadOnlyOptimal);

            out->info.push_back(vk::DescriptorImageInfo(sampler, image->imageView, vk::ImageLayout::eShaderReadOnlyOptimal));

            out->write.setImageInfo(out->info);

            return out;
        }
    };



    //basically, texture memory manager
    class BindlessTextureArray {
        struct BTex {
            std::shared_ptr<AllocatedImage> image;
            vk::raii::Sampler sampler;
        };
        std::vector<BTex> textures;
        //std::set<size_t> emptySlots; <- uncomment when texture deletion is a feature I need

        public:

        static const int MAX_TEXTURES = 2048;

        BTex& getTexture(size_t idx) {
            //assert(!emptySlots.count(idx)); empty check
            //assert(!textures.at(idx).image.image == nullptr); empty check 2

            return textures.at(idx);
        }

        size_t addTexture(Medea::Core& core, CommandJobQueueCallback callback, std::string_view filepath, vk::SamplerCreateInfo samplerInfo, bool mipmaps) {
            auto flags = vk::ImageUsageFlagBits::eSampled;

            std::shared_ptr<AllocatedImage> ptr = AllocatedImage::load(core, callback, filepath, flags, mipmaps);

            callback([ptr] (vk::CommandBuffer cmd, CleanupJobQueueCallback) {
                multiTransition(cmd, {*ptr}, {vk::ImageLayout::eShaderReadOnlyOptimal});
            });

            textures.push_back(BTex{ptr, core.device.createSampler(samplerInfo)});

            return textures.size()-1;
        }

        size_t addTexture(Medea::Core& core, std::shared_ptr<AllocatedImage> image, vk::SamplerCreateInfo samplerInfo) {
            textures.push_back(BTex{image, core.device.createSampler(samplerInfo)});

            return textures.size()-1;
        }

        ///I don't actually need texture deletion for a while
        //void removeTexture(size_t idx) {}

        static vk::DescriptorSetLayoutBinding makeBinding(uint32_t bindIdx, vk::ShaderStageFlags stages) {
            vk::DescriptorSetLayoutBinding binding(bindIdx, vk::DescriptorType::eCombinedImageSampler, stages, {});

            binding.setDescriptorCount(MAX_TEXTURES);

            return binding;
        }

        //attaches all textures that have their image in eShaderReadOnlyOptimal mode.
        // why: makes render-to-framebuffer work better; swap into attach, render to it, then swap into readonly again
        // empty entries are set to texture 0 (error texture)
        std::unique_ptr<Internal::WriteGroup> getArrayUpdate(vk::DescriptorSet target, uint32_t bindIdx) {
            assert(textures.size() < MAX_TEXTURES);
            assert(textures.size() > 0);

            auto out = std::make_unique<Internal::WriteGroup>(vk::WriteDescriptorSet(target, bindIdx, 0, vk::DescriptorType::eCombinedImageSampler, {}));

            out->write.setDescriptorCount(MAX_TEXTURES);

            out->info.reserve(MAX_TEXTURES);

            for (size_t i=0; i<MAX_TEXTURES; i++) {
                //set to error texture
                if (i >= textures.size() || /*textures[i] is removed || */ textures.at(i).image->_currentLayout != vk::ImageLayout::eShaderReadOnlyOptimal) {
                    auto& t0 = textures.at(0);

                    assert(t0.image->_currentLayout == vk::ImageLayout::eShaderReadOnlyOptimal);

                    out->info.push_back(vk::DescriptorImageInfo(t0.sampler, t0.image->imageView, vk::ImageLayout::eShaderReadOnlyOptimal));
                }
                else  {
                    auto& tex = textures.at(i);

                    out->info.push_back(vk::DescriptorImageInfo(tex.sampler, tex.image->imageView, vk::ImageLayout::eShaderReadOnlyOptimal));
                }
            }

            out->write.setImageInfo(out->info);

            return out;
        }
    };

    //blyh bleh bloy

}