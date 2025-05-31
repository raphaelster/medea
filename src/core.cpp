#include "core.h"
#include "meta.h"

#include "internal/metacodegen.h"

namespace Medea {
    BufferRef BufferRef::null = BufferRef::makeNull();

    Core Core::make(vk::raii::Context& context, Medea::Window& window) {
        vkb::InstanceBuilder builder; 

        constexpr bool USE_VALIDATION_LAYERS = true;

        auto inst_ret = builder.set_app_name("tntts")
            .request_validation_layers(USE_VALIDATION_LAYERS)
            .use_default_debug_messenger()
            .require_api_version(1, 3, 0)
            .build();

        vkb::Instance vkb_inst = VKB_UNWRAP(inst_ret, "Couldn't create vkBootstrap instance");

        //uint32_t size;
        //const char** glfwExts = glfwGetRequiredInstanceExtensions(&size);

        vk::raii::Instance outInstance(context, vkb_inst.instance);
        vk::raii::DebugUtilsMessengerEXT outDebugMessenger(outInstance, vkb_inst.debug_messenger);
        
        // ====
        
        VkPhysicalDeviceVulkan13Features features13{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        features13.dynamicRendering = true;
        features13.synchronization2 = true;

        VkPhysicalDeviceVulkan12Features features12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        features12.bufferDeviceAddress = true;
        features12.descriptorIndexing = true;
        features12.shaderSampledImageArrayNonUniformIndexing = true;
        features12.drawIndirectCount = true;

        vk::PhysicalDeviceVulkan11Features features11 = {};

        features11.setStorageBuffer16BitAccess(true);

        vk::PhysicalDeviceFeatures features10 = {};
        features10.shaderInt64 = true;
        features10.samplerAnisotropy = true;
        features10.shaderInt16 = true;
        features10.shaderCullDistance = true;
        //  

        vkb::PhysicalDeviceSelector selector(vkb_inst);

        VkSurfaceKHR rawSurface = {};

        assert(glfwVulkanSupported() == GLFW_TRUE);

        VK_REQUIRE(glfwCreateWindowSurface(vkb_inst.instance, window.window, nullptr, &rawSurface));


        vkb::PhysicalDevice physicalDevice = 
            VKB_UNWRAP(selector
            .set_minimum_version(1, 3)
            .set_required_features_13(features13)
            .set_required_features_12(features12)
            .set_required_features_11(features11)
            .set_required_features(features10)
            .defer_surface_initialization()
            .set_surface(rawSurface)
            //.add_required_extension(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME) //<- renderdoc doesn't support descriptor buffers :(
            .select(), "Couldn't find PhysicalDevice");


        physicalDevice.enable_extension_if_present(vk::EXTDynamicRenderingUnusedAttachmentsExtensionName);


        //vk::raii::SurfaceKHR outDummySurface(outInstance, rawSurface);

        vkb::DeviceBuilder deviceBuilder(physicalDevice);

        vk::PhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT unusedAttachFeature(true);

        deviceBuilder.add_pNext(&unusedAttachFeature);

        vkb::Device vkbDevice = VKB_UNWRAP(deviceBuilder.build(), "Couldn't find device");

        vk::raii::PhysicalDevice outGpu(outInstance, vkbDevice.physical_device);
        vk::raii::Device outDevice(outGpu, vkbDevice.device);

        vk::raii::Queue outGraphicsQueue(outDevice, VKB_UNWRAP(vkbDevice.get_queue(vkb::QueueType::graphics), "Couldn't find queue"));
        uint32_t outGraphicsQueueFamily = VKB_UNWRAP(vkbDevice.get_queue_index(vkb::QueueType::graphics), "Couldn't find queue index");

        uint32_t count = 0;
        //auto features = outGpu.enumerateDeviceExtensionProperties();

        //for (auto& f : features) std::cout<<f.extensionName<<std::endl;

        bool foundDynRenderUnusedAttachments = false;

        for (auto& str : outGpu.enumerateDeviceExtensionProperties()) {
            if (std::string(str.extensionName) == vk::EXTDynamicRenderingUnusedAttachmentsExtensionName) {
                foundDynRenderUnusedAttachments = true;
                //break;
            }
        }
        if (!foundDynRenderUnusedAttachments) {
            std::cerr<<"ERROR: Couldn't find physical device that supports \""<<vk::EXTDynamicRenderingUnusedAttachmentsExtensionName<<"\".\n"
                     <<"Assuming the extension still exists, since running under Renderdoc hides this (despite Renderdoc supporting it?).\n";
        }

        // ====
        VmaAllocatorCreateInfo allocCreateInfo = {};

        allocCreateInfo.device = *outDevice;
        allocCreateInfo.instance = *outInstance;
        allocCreateInfo.physicalDevice = *outGpu;
        allocCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

        VmaAllocator outAlloc;
        vmaCreateAllocator(&allocCreateInfo, &outAlloc);

        vkDestroySurfaceKHR(*outInstance, rawSurface, nullptr);

        return Core(std::move(outInstance), std::move(outGpu), std::move(outDevice), outAlloc, std::move(outDebugMessenger),
                        std::move(outGraphicsQueue), std::move(outGraphicsQueueFamily), window);
    }

    MVKWindow MVKWindow::make(vk::raii::Instance& instance, vk::raii::Device& device, vk::raii::PhysicalDevice& gpu, 
                                    vk::raii::Queue& queue, uint32_t graphicsQueueFamily, Medea::Window& w) {
        VkSurfaceKHR rawSurface;

        VK_REQUIRE(glfwCreateWindowSurface(*instance, w.window, nullptr, &rawSurface));

        vk::raii::SurfaceKHR outSurface(instance, rawSurface);

        

        vkb::SwapchainBuilder swapBuilder{*gpu, *device, *outSurface};

        VkFormat outSwapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

        vkb::Swapchain vkbSwapchain = 
            VKB_UNWRAP(swapBuilder
            .set_desired_format(VkSurfaceFormatKHR{.format = outSwapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR) //vsync
            .set_desired_extent(w.span.x, w.span.y)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build(), "Couldn't setup swapchain");

        vk::raii::SwapchainKHR outSwapchain(device, vkbSwapchain.swapchain);

        std::vector<VkImage> outSwapchainImages = VKB_UNWRAP(vkbSwapchain.get_images(), "Couldn't find swapchain images");
        std::vector<vk::raii::ImageView> outSwapchainImageViews;



        for (auto& iv : VKB_UNWRAP(vkbSwapchain.get_image_views(), "Couldn't find swapchain ImageViews")) {
            outSwapchainImageViews.push_back(vk::raii::ImageView(device, iv));
        }

        VkExtent2D swapchainExtent = vkbSwapchain.extent;

        const int NUM_FRAMES = 2;

        std::vector<Frame> frames;

        for (int i=0; i<NUM_FRAMES; i++) frames.push_back(Frame::make(device, graphicsQueueFamily));


        return MVKWindow{device, queue, std::move(outSurface), w, std::move(outSwapchain), std::move(outSwapchainImageFormat), 
                            std::move(outSwapchainImages), std::move(outSwapchainImageViews), swapchainExtent, std::move(frames)};
    }

}