#pragma once

#include "intdef.h"
#include "vkfw.h"

#include "VkBootstrap.h"

#include "vk_mem_alloc.h"

#include "window.h"

#include <vulkan/vk_enum_string_helper.h>

#include "constants.h"

#include <sstream>
#include <fstream>

#define VK_REQUIRE(x) { auto _my_result = x; Medea::_vkAssert<decltype(_my_result)>()(_my_result);}
#define VK_UNWRAP(x) Medea::_vkUnwrap(x)
#define VK_REQUIRE_ERR(x, error) Medea::Internal::_vkRequireErr(x, error)
#define VKB_UNWRAP(x, error) Medea::Internal::_vkbUnwrapErr(x, error)

namespace Medea {
    namespace RenderConstants {
        const vk::Format screenFormat = vk::Format::eR8G8B8A8Unorm;
    }

    using CleanupJob = std::function<void()>;
    using CleanupJobQueueCallback = std::function<void(CleanupJob)>;

    using CommandJob = std::function<void(vk::CommandBuffer, CleanupJobQueueCallback)>;
    using CommandJobQueueCallback = std::function<void(CommandJob)>;

    template<typename T>
    struct _vkAssert {
    };

    namespace Internal {
        inline void _vkRequireErr(vk::Result res, std::string_view msg) {
            if (res != vk::Result::eSuccess) {
                std::cerr<<"VK_REQUIRE failed:\n"<<msg<<std::endl;
            }
        }
        inline void _vkRequireErr(VkResult res, std::string_view msg) {
            if (res != VK_SUCCESS) {
                std::cerr<<"VK_REQUIRE failed:\n"<<msg<<std::endl;
            }
        }


        auto _vkbUnwrapErr(auto x, std::string_view msg) {
            if (x) return x.value();

            std::cerr<<"VKB_UNWRAP failed:\n"<<msg<<std::endl;
            abort();
        }
    }

    template<>
    struct _vkAssert<VkResult> {
        void operator()(VkResult result) {
            if (result != VK_SUCCESS) std::cerr<<"VK_REQUIRE failed with code "<<string_VkResult(result)<<std::endl;

            assert(result == VK_SUCCESS);
        }
    };

    template<>
    struct _vkAssert<vk::Result> {
        void operator()(vk::Result result) {
            if (result != vk::Result::eSuccess) std::cerr<<"VK_REQUIRE failed with code "<<result<<std::endl;

            //

            assert(result == vk::Result::eSuccess);
        }
    };

    auto _vkUnwrap(auto x) {
        VK_REQUIRE(x.first);
        return x.second;
    }




    inline vk::raii::Semaphore makeSemaphore(vk::raii::Device& device, VkSemaphoreCreateFlags flags) {
        VkSemaphoreCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        info.pNext = nullptr;
        info.flags = flags;

        VkSemaphore out;

        VK_REQUIRE(vkCreateSemaphore(*device, &info, nullptr, &out));

        return vk::raii::Semaphore(device, out);
    }


    inline vk::raii::Fence makeFence(vk::raii::Device& device, VkFenceCreateFlags flags) {
        VkFenceCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        info.pNext = nullptr;
        info.flags = flags;

        VkFence out;

        VK_REQUIRE(vkCreateFence(*device, &info, nullptr, &out));

        return vk::raii::Fence(device, out);
    }


    inline void beginCommandBuffer(VkCommandBuffer buffer, VkCommandBufferUsageFlags flags) {
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.pNext = nullptr;
        info.pInheritanceInfo = nullptr;
        info.flags = flags;

        VK_REQUIRE(vkBeginCommandBuffer(buffer, &info));
    }


    /// basically, this can limit something (barrier, clear, etc) to parts of an image (specific mip levels, or specific indices of array texture) 
    ///  kinda like glScissor, but for mip/array elements
    inline VkImageSubresourceRange imageSubresourceRange(VkImageAspectFlags flags) {
        VkImageSubresourceRange out = {};

        out.aspectMask = flags;
        out.baseMipLevel = 0;
        out.levelCount = VK_REMAINING_MIP_LEVELS;
        out.baseArrayLayer = 0;
        out.layerCount = VK_REMAINING_ARRAY_LAYERS;

        return out;
    }

    /// TODO: more optimal access masks 
    inline void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout l0, VkImageLayout l1, bool depth = false) {
        VkImageMemoryBarrier2 imageBarrier {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        imageBarrier.pNext = nullptr;

        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;          //disallows writing to src
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;    //disallows reading, writing to dst

        imageBarrier.oldLayout = l0;
        imageBarrier.newLayout = l1;

        ///specialization; we primarily care about color images. Switch to depth for depth optimal stuff
        VkImageAspectFlags aspectMask = (depth) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        imageBarrier.subresourceRange = imageSubresourceRange(aspectMask);
        imageBarrier.image = image;

        VkDependencyInfo depInfo = {};  ///can chain dependencies via pNext? So, if we're rendering to 5 different images, we can chain 5 dependencies and wait for them all to finish
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.pNext = nullptr;

        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &imageBarrier;

        vkCmdPipelineBarrier2(cmd, &depInfo);
    }

    inline void blitImage(vk::CommandBuffer cmd, vk::Image src, vk::Image dst, vk::Extent2D srcDim, vk::Extent2D dstDim) {
        vk::ImageSubresourceLayers srcSub(vk::ImageAspectFlags(vk::ImageAspectFlagBits::eColor), 0, 0, 1);
        vk::ImageSubresourceLayers dstSub(vk::ImageAspectFlags(vk::ImageAspectFlagBits::eColor), 0, 0, 1);

        vk::ImageBlit2 blitRegion(srcSub, {0U, {(int32_t) srcDim.width, (int32_t) srcDim.height, 1U}}, dstSub, {0U, {(int32_t) dstDim.width, (int32_t) dstDim.height, 1U}});

        vk::BlitImageInfo2 blitInfo(src, vk::ImageLayout::eTransferSrcOptimal, dst, vk::ImageLayout::eTransferDstOptimal, blitRegion, vk::Filter::eLinear);

        cmd.blitImage2(blitInfo);    
    }

    inline VkSemaphoreSubmitInfo getSemSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore sem) {
        return VkSemaphoreSubmitInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .semaphore = sem,
            .value = 1,
            .stageMask = stageMask,
            .deviceIndex = 0
        };
    }

    inline VkCommandBufferSubmitInfo getCommandBufSubmitInfo(VkCommandBuffer cmd) {
        return VkCommandBufferSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .pNext = nullptr,
            .commandBuffer = cmd,
            .deviceMask = 0
        };
    }

    inline VkSubmitInfo2 getSubmitInfo(VkCommandBufferSubmitInfo& cmd, VkSemaphoreSubmitInfo* signalSem, VkSemaphoreSubmitInfo* waitSem) {
        return VkSubmitInfo2 {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext = nullptr,

            .waitSemaphoreInfoCount = (unsigned int) (waitSem ? 1 : 0),
            .pWaitSemaphoreInfos = waitSem,
            
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &cmd,

            .signalSemaphoreInfoCount = (unsigned int) (signalSem ? 1 : 0),
            .pSignalSemaphoreInfos = signalSem
        };
    }




    struct Frame {
        vk::raii::CommandPool pool;
        vk::raii::CommandBuffer mainBuffer;

        vk::raii::Fence fence;
        vk::raii::Semaphore swapchainSemaphore, renderSemaphore;

        std::vector<CleanupJob> cleanupJobs;

        CleanupJobQueueCallback getCleanupCallback() {
            return [&] (CleanupJob job) {
                cleanupJobs.push_back(job);
            };
        }

        static Frame make(vk::raii::Device& device, uint32_t graphicsQueueFamily) {

            auto cpFlag = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

            vk::raii::CommandPool pool(device, vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlags(cpFlag), graphicsQueueFamily));

            auto buffers = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo(*pool, vk::CommandBufferLevel::ePrimary, 1));

            assert(buffers.size() == 1);
            
            return Frame{
                std::move(pool),
                std::move(buffers.at(0)),
                makeFence(device, VK_FENCE_CREATE_SIGNALED_BIT),
                makeSemaphore(device, 0),
                makeSemaphore(device, 0)};
        }
    };

    namespace Internal {
        struct WrappedAllocation {
            VmaAllocator allocator; //<- this should probably be a global?
            VmaAllocation memory;

            WrappedAllocation(VmaAllocator alloc, VmaAllocation mem)
                : allocator(alloc), memory(mem) {}

            WrappedAllocation(const WrappedAllocation&) = delete;
            WrappedAllocation& operator=(const WrappedAllocation&) = delete;

            WrappedAllocation(WrappedAllocation&& old)
            : allocator(old.allocator), memory(old.memory) {
                old.clear();
            }

            WrappedAllocation& operator=(WrappedAllocation&& old) {
                vmaFreeMemory(allocator, memory);

                allocator = old.allocator;
                memory = old.memory;

                old.clear();

                return *this;
            }

            ~WrappedAllocation() {
                vmaFreeMemory(allocator, memory);
            }

            private:
            void clear() {
                memory = VK_NULL_HANDLE;
            }
        };
    }


    class AllocatedBuffer {
        vk::DeviceAddress address;

        public:
        VmaAllocator allocator;

        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo info;
        vk::DeviceSize size;

        //AllocatedBuffer()
            //: allocator(nullptr), buffer(nullptr), allocation(nullptr), info({}) {}

        AllocatedBuffer(vk::Device device, VmaAllocator inAllocator, vk::DeviceSize allocSize, vk::BufferUsageFlags flags, VmaMemoryUsage memoryUsage)
            : allocator(inAllocator), buffer(nullptr), size(allocSize) {

            assert(allocSize > 0);
            
            vk::BufferCreateInfo bufInfo({}, allocSize, flags);

            VmaAllocationCreateInfo vmaInfo = {};
            vmaInfo.usage = memoryUsage;
            //vmaInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

            VkBufferCreateInfo cBufInfo = bufInfo;

            VK_REQUIRE(vmaCreateBuffer(allocator, &cBufInfo, &vmaInfo, &this->buffer, &this->allocation, &this->info));

            if (flags & vk::BufferUsageFlagBits::eShaderDeviceAddress) address = device.getBufferAddress(vk::BufferDeviceAddressInfo(buffer));
            else address = 0;
        }

        AllocatedBuffer(vk::Device device, VmaAllocator inAllocator, vk::DeviceSize allocSize, vk::BufferUsageFlags flags, VmaAllocationCreateFlags vmaFlags, VmaMemoryUsage memoryUsage)
            : allocator(inAllocator), buffer(nullptr), size(allocSize) {

            assert(allocSize > 0);
            
            vk::BufferCreateInfo bufInfo({}, allocSize, flags);

            VmaAllocationCreateInfo vmaInfo = {};
            vmaInfo.usage = memoryUsage;
            vmaInfo.flags = vmaFlags;

            VkBufferCreateInfo cBufInfo = bufInfo;

            VK_REQUIRE(vmaCreateBuffer(allocator, &cBufInfo, &vmaInfo, &this->buffer, &this->allocation, &this->info));

            if (flags & vk::BufferUsageFlagBits::eShaderDeviceAddress) address = device.getBufferAddress(vk::BufferDeviceAddressInfo(buffer));
            else address = 0;
        }

        AllocatedBuffer(vk::Device device, VmaAllocator allocator_, VkBuffer buffer_, VmaAllocation allocation_, VmaAllocationInfo info_, vk::DeviceSize size_)
            : allocator(allocator_), buffer(buffer_), allocation(allocation_), info(info_), size(size_) {
            address = 0;
        }

        AllocatedBuffer(const AllocatedBuffer&) = delete;
        AllocatedBuffer& operator=(const AllocatedBuffer&) = delete;

        AllocatedBuffer(AllocatedBuffer&& old) 
            : allocator(old.allocator), buffer(old.buffer), allocation(old.allocation), info(old.info), size(old.size) {
            address = old.address;

            old.allocator = nullptr;
            old.buffer = nullptr;
            old.allocation = nullptr;
            old.address = 0;
        }

        AllocatedBuffer& operator=(AllocatedBuffer&& old) {
            if (allocator) vmaDestroyBuffer(allocator, buffer, allocation);

            allocator = old.allocator;
            buffer = old.buffer;
            allocation = old.allocation;
            address = old.address;
            size = old.size;
            info = old.info;

            old.allocation = nullptr;
            old.buffer = nullptr;
            old.allocator = nullptr;
            old.address = 0;
            old.size = 0;

            return *this;
        }

        ~AllocatedBuffer() {
            if (allocator) vmaDestroyBuffer(allocator, buffer, allocation);
        }

        ///NOTE: default usage of CPU-to-GPU
        /// could use some method of automatically scheduling transfers or w/e, but that seems like not really worth the effort when the GPU side command
        /// has to go in a command buffer somewhere
        template<typename T>
        static AllocatedBuffer loadCPU(VmaAllocator allocator, vk::Device device, std::span<T> data, vk::BufferUsageFlags flags) {        
            assert(data.size_bytes() > 0);

            vk::BufferCreateInfo bufInfo({}, data.size_bytes(), flags);

            VmaAllocationCreateInfo vmaInfo = {};
            vmaInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
            vmaInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

            VkBufferCreateInfo cBufInfo = bufInfo;            

            VkBuffer buffer;
            VmaAllocation allocation;
            VmaAllocationInfo info;

            VK_REQUIRE(vmaCreateBuffer(allocator, &cBufInfo, &vmaInfo, &buffer, &allocation, &info));


            VK_REQUIRE(vmaCopyMemoryToAllocation(allocator, data.data(), allocation, 0, data.size_bytes()));

            AllocatedBuffer out(device, allocator, buffer, allocation, info, data.size_bytes());

            if (flags & vk::BufferUsageFlagBits::eShaderDeviceAddress) out.address = device.getBufferAddress(vk::BufferDeviceAddressInfo(out.buffer));

            return out;
        }


        template<typename T>
        static AllocatedBuffer loadCPUWithHeader(VmaAllocator allocator, vk::Device device, std::span<T> data, vk::BufferUsageFlags flags, size_t overridenHeaderSize = 0) {
            size_t offset = RenderConstants::arrayHeaderSize;

            //if (alignof(T) > sizeof(uint32_t)) offset = alignof(T);

            size_t totalSize = data.size_bytes() + offset;

            vk::BufferCreateInfo bufInfo({}, totalSize, flags);

            VmaAllocationCreateInfo vmaInfo = {};
            vmaInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
            vmaInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

            VkBufferCreateInfo cBufInfo = bufInfo;            

            VkBuffer buffer;
            VmaAllocation allocation;
            VmaAllocationInfo info;

            VK_REQUIRE(vmaCreateBuffer(allocator, &cBufInfo, &vmaInfo, &buffer, &allocation, &info));

            uint32_t size = (uint32_t) data.size();
            if (overridenHeaderSize) size = (u32) overridenHeaderSize;

            void* ptr;

            //vmaCopyMemoryToAllocation(allocator, chars.data(), allocation, 0, totalSize);

            VK_REQUIRE(vmaMapMemory(allocator, allocation, &ptr));

            void* ptr2 = ((char*) ptr) + offset;


            memset(ptr, 123, totalSize);

            memcpy(ptr, &size, sizeof(size));
            memcpy(ptr2, data.data(), data.size_bytes());

            vmaUnmapMemory(allocator, allocation);
            VK_REQUIRE(vmaFlushAllocation(allocator, allocation, 0, totalSize));

            //VK_REQUIRE(vmaCopyMemoryToAllocation(allocator, &size, allocation, 0, sizeof(uint32_t)));
            //VK_REQUIRE(vmaCopyMemoryToAllocation(allocator, data.data(), allocation, offset, data.size_bytes()));

            return AllocatedBuffer(device, allocator, buffer, allocation, info, totalSize);
        }

        

        ///Does not resize the buffer. Buffer overflow triggers an assert fail
        template<typename T>
        void reload(std::span<T> data) {
            assert(data.size_bytes() <= this->size);

            VK_REQUIRE(vmaCopyMemoryToAllocation(allocator, data.data(), allocation, 0, data.size_bytes()));
        }
        template<typename T>
        void reload(const T& data) {
            size_t dataSize = sizeof(T);

            assert(dataSize <= this->size);

            VK_REQUIRE(vmaCopyMemoryToAllocation(allocator, &data, allocation, 0, dataSize));
        }


        vk::DeviceAddress getAddress() {
            assert(address != 0);

            return address;
        }
    };

    class BufferRef {
        BufferRef(vk::DeviceAddress addr)
            : address(addr) {}
        public:
        vk::DeviceAddress address;

        BufferRef(AllocatedBuffer& buf) 
            : address(buf.getAddress()) {}

        static BufferRef null;

        static BufferRef makeNull() {
            return BufferRef(vk::DeviceAddress(0));
        }
    };

    struct DrawingFrame {
        vk::raii::Device& device;
        Frame& frame;
        vk::raii::SwapchainKHR& swapchain;
        vk::Image swapchainImage;
        vk::Fence renderFence;

        uint32_t swapchainImageIdx;

        //DrawingFrame(vk::raii::Device& d, Frame& f, vk::raii::SwapchainKHR& s)
        //: device(d), frame(f), swapchain(s) {}
    };

    struct MVKWindow {
        vk::raii::Device& device;

        /// TODO: abstract out, have submit() pass to queue thread
        vk::raii::Queue& queue;

        vk::raii::SurfaceKHR surface;
        Medea::Window& window;
        
        vk::raii::SwapchainKHR swapchain;
        VkFormat swapchainImageFormat;

        std::vector<VkImage> swapchainImages;
        std::vector<vk::raii::ImageView> swapchainImageViews;
        VkExtent2D swapchainExtent;

        std::vector<Frame> frames;

        Frame& getFrame() {
            return frames.at(_currentFrame % frames.size());
        }

        void drainFrame(Frame& f) {
            const uint64_t SECOND_NS = 1000000000;

            VK_REQUIRE(device.waitForFences(*f.fence, true, SECOND_NS));

            for (auto& job : f.cleanupJobs) job();

            f.cleanupJobs.clear();
        }

        void drain() {
            for (auto& f : frames) drainFrame(f);
        }

        DrawingFrame startDraw() {
            assert(!_frameStarted);
            _frameStarted = true;
            
            const uint64_t SECOND_NS = 1000000000;
            
            Frame& frame = getFrame();

            drainFrame(frame);
            device.resetFences(*frame.fence);

            _lastSwapchainImageIdx = VK_UNWRAP(swapchain.acquireNextImage(SECOND_NS, *frame.swapchainSemaphore));
            
            frame.mainBuffer.reset();

            
            frame.mainBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlags(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)));

            
            VkImage scImage = swapchainImages.at(_lastSwapchainImageIdx);

            //image from [don't care] to general rendering R/W format
            transitionImage(*frame.mainBuffer, scImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            return DrawingFrame{device, frame, swapchain, scImage, frame.fence, _lastSwapchainImageIdx};
        }

        void endDraw(vk::Image src, VkExtent2D extents) {
            assert(_frameStarted);

            Frame& frame = getFrame();

            vk::Image swapImg = swapchainImages.at(_lastSwapchainImageIdx);

            blitImage(*frame.mainBuffer, src, swapImg, extents, swapchainExtent);
            
            transitionImage(*frame.mainBuffer, swapImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

            frame.mainBuffer.end();

            auto c0 = vk::CommandBufferSubmitInfo(*frame.mainBuffer, 0);
            auto w0 = vk::SemaphoreSubmitInfo(*frame.swapchainSemaphore, 1, vk::PipelineStageFlags2(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT));
            auto s0 = vk::SemaphoreSubmitInfo(*frame.renderSemaphore, 1, vk::PipelineStageFlags2(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT));

            auto sub = vk::SubmitInfo2(vk::SubmitFlags(), w0, c0, s0);

            submit(sub, *frame.fence, *frame.renderSemaphore);

            _currentFrame++;
            _frameStarted = false;
        }

        static MVKWindow make(vk::raii::Instance& instance, vk::raii::Device& device, vk::raii::PhysicalDevice& gpu, vk::raii::Queue& queue, 
                                uint32_t graphicsQueueFamily, Medea::Window& w);

        //private:
        
        void submit(vk::SubmitInfo2& sub, vk::Fence fence, vk::Semaphore renderSemaphore) {
            //queue.submit(sub);
            queue.submit2(sub, fence);

            vk::PresentInfoKHR present(renderSemaphore, *swapchain, _lastSwapchainImageIdx);

            VK_REQUIRE(queue.presentKHR(present));
        }

        bool _frameStarted = false;
        uint32_t _lastSwapchainImageIdx = 0;
        long long _currentFrame = 0;

    };

    namespace Internal {
        struct RAIIAllocator {
            VmaAllocator allocator;

            ~RAIIAllocator() {
                vmaDestroyAllocator(allocator);
            }
        };
    }

    /// @brief Represents all of the global state the Vulkan renderer needs
    struct Core {
        vk::raii::Instance instance;
        vk::raii::PhysicalDevice gpu;
        vk::raii::Device device;
        Internal::RAIIAllocator _internalAllocator;
        VmaAllocator allocator;
        vk::raii::DebugUtilsMessengerEXT debugMessenger;

        /// @brief Just using a single queue. Vulkan docs say it's fine, and GPUs can have as few as 1 queue per type.
        ///         Not thread safe; to multithread, have a single thread that submits & other threads give it data
        /// And, re: compute queues: AMD suggests that multiple compute queues aren't useful
        vk::raii::Queue graphicsQueue; 
        uint32_t graphicsQueueFamily;

        MVKWindow primaryWindow;

        Core(vk::raii::Instance i, vk::raii::PhysicalDevice _gpu, vk::raii::Device d, VmaAllocator alloc, vk::raii::DebugUtilsMessengerEXT msg, 
                    vk::raii::Queue gq, uint32_t graphicsQFamily, Medea::Window& w)
            : instance(std::move(i)), _internalAllocator{alloc}, gpu(_gpu), device(std::move(d)), allocator(alloc), debugMessenger(std::move(msg)), graphicsQueue(gq), graphicsQueueFamily(graphicsQFamily),
            primaryWindow(MVKWindow::make(instance, device, gpu, graphicsQueue, graphicsQueueFamily, w)) {}

        ~Core() {
            primaryWindow.drain();
        }

        static Core make(vk::raii::Context& context, Medea::Window& window);
    };


}




namespace Medea {

    /// @brief  Represents a singular DescriptorLayout, tagged with some metadata for better automatic piping
    struct TaggedDescriptorLayout {
        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        vk::raii::DescriptorSetLayout layout;
    };

    /// @brief Pretty much just a thin wrapper for a vk::raii::DescriptorPool, with minor syntax sugar:
    ///   auto x = pool.allocate(device, layout) instead of 
    ///   auto x = vk::raii::DescriptorSets(device, vk::DescriptorSetAllocateInfo(*pool, *layout))
    ///
    ///  Note: this is slow. I'm using RAII wrappers for descriptor sets, which means the driver has to be able to free individual desc sets,
    ///  which is slower path than having multiple pools per frame, and resetting pools that don't have any data in flight
    struct DescriptorAllocator {

        /// @brief helps set up size for descriptor pool, since that's a fixed block of GPU memory that gets suballocated into DescriptorSets
        struct PoolSizeRatio {
            vk::DescriptorType type;
            double ratio;
        };

        vk::raii::DescriptorPool pool;

        void clear() {
            pool.reset();
        }
        
        static DescriptorAllocator make(vk::raii::Device& device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios) {
            std::vector<vk::DescriptorPoolSize> poolSizes;

            for (auto& ratio : poolRatios) poolSizes.push_back(vk::DescriptorPoolSize(ratio.type, ratio.ratio * maxSets));

            ///WARNING: pessimization. Manually managing my descriptor set memory frame-by-frame is faster than using RAII to free individual desc sets.
            vk::DescriptorPoolCreateInfo poolInfo({vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet}, maxSets, poolSizes);

            return DescriptorAllocator{device.createDescriptorPool(poolInfo)};
        }

    
        vk::raii::DescriptorSet allocate(vk::raii::Device& device, vk::raii::DescriptorSetLayout& layout) {
            //auto layoutArr = {layout};

            vk::DescriptorSetAllocateInfo allocInfo(*pool, *layout);

            return std::move(vk::raii::DescriptorSets(device, allocInfo).at(0));
            //return device.allocateDescriptorSets(allocInfo);

            //return std::move(device.allocateDescriptorSets(allocInfo).at(0));
        }


        /*
        std::vector<vk::raii::DescriptorSet> allocate(vk::raii::Device& device, std::vector<TaggedDescriptorLayout>& layouts) {
            std::vector<vk::raii::DescriptorSet> out;

            out.reserve(layouts.size());

            for (auto& l : layouts) out.push_back(allocate(device, l.layout));

            return out;
        }*/
    };




    inline std::optional<std::string> readFile(std::string_view path) {
        std::ifstream file;

        file.exceptions (std::ifstream::failbit | std::ifstream::badbit);

        std::stringstream stream;

        try {
            file.open(std::string(path));
            
            stream << file.rdbuf();

            file.close();

            return stream.str();

        }
        catch (std::ifstream::failure& e) {
            std::cerr<<"Failed to read file at "<<path<<std::endl;
        }

        return std::nullopt;
    }


}

