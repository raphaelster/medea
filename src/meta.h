#pragma once

#include "core.h"
#include "graphics.h"

#include "renderentity.h"

#include "metaimage.h"

#include <memory>
#include "vertex.h"

#include "constants.h"

namespace Medea {

    


    namespace Internal {

        ///todo: compress
        /// Doom Eternal siggraph talk: can compress normal + tangent frame into 3 floats

        template<typename T>
        void transferToGPU(CommandJobQueueCallback callback, VmaAllocator allocator, vk::Device device, std::span<T> data, vk::Buffer target) {
            AllocatedBuffer* staging = new AllocatedBuffer(AllocatedBuffer::loadCPU(allocator, device, data, vk::BufferUsageFlagBits::eTransferSrc));

            VkBuffer stageBuf = staging->buffer;

            size_t vbSize = data.size_bytes();

            CommandJob job = [=] (vk::CommandBuffer cmd, CleanupJobQueueCallback cleanup) {
                cmd.copyBuffer(stageBuf, target, vk::BufferCopy(0, 0, vbSize));

                void* ptr = staging->info.pMappedData;

                cleanup([staging] () {
                    delete staging;
                });
            };

            callback(job);
        }

        inline void transferToGPU(CommandJobQueueCallback callback, VmaAllocator allocator, vk::Device device, void* pixelData, vk::Extent3D size, vk::Format format,
                            AllocatedImage& target) {

            size_t colorWidth = 1;
            switch (format) {
                case vk::Format::eR8G8B8A8Unorm:
                colorWidth = 4;
                break;

                default:
                std::cerr<<"Trying to transferToGPU with unsupported color format "<<vk::to_string(format)<<"; assuming colorWidth = 1"<<std::endl;
                break;
            }

            std::span<char> data((char*) pixelData, size.depth * size.width * size.height * colorWidth);

            AllocatedBuffer* staging = new AllocatedBuffer(AllocatedBuffer::loadCPU(allocator, device, data, vk::BufferUsageFlagBits::eTransferSrc));


            VkBuffer stageBuf = staging->buffer;
            size_t vbSize = data.size_bytes();


            CommandJob job = [=, &target] (vk::CommandBuffer cmd, CleanupJobQueueCallback cleanup) {
                if (target._currentLayout != vk::ImageLayout::eTransferDstOptimal) target.transitionSync(cmd, vk::ImageLayout::eTransferDstOptimal, true);

                cmd.copyBufferToImage(stageBuf, *target.image, target._currentLayout,
                        vk::BufferImageCopy(0,0,0, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), vk::Offset3D(0,0,0), size));

                cleanup([staging] () {
                    delete staging;
                });
            };

            callback(job);
        }


        inline void transferToGPU(CommandJobQueueCallback callback, VmaAllocator allocator, vk::Device device, void* pixelData, vk::Extent3D size, vk::Format format,
                            std::shared_ptr<AllocatedImage> target) {
            transferToGPU(callback, allocator, device, pixelData, size, format, *target);
        }


    }

    /// unindexed
    template<typename Vertex>
    struct MeshBuffer {
        uint32_t totalVertices;
        AllocatedBuffer buffer;

        /// uploads mesh to GPU. 
        /// WARNING: I am unsure if this does any synchronization
        static MeshBuffer make(CommandJobQueueCallback callback, VmaAllocator allocator, vk::Device device, std::span<Vertex> vertices) {
            size_t vbSize = vertices.size() * sizeof(Vertex);

            assert(vertices.size() % 3 == 0);


            AllocatedBuffer final(device, allocator, vbSize, 
                    vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst,
                    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

            //transfer
            Internal::transferToGPU<Vertex>(callback, allocator, device, vertices, final.buffer);

            return MeshBuffer(vertices.size(), std::move(final));
        }


        static MeshBuffer make(CommandJobQueueCallback callback,  VmaAllocator allocator, vk::Device device, std::vector<Vertex>&& vertices) {
            std::vector<Vertex> stored(vertices);

            return make(callback, allocator, device, stored);
        }
    };

    struct MeshCollider {
        double sphereRad;
    };

    /// NOTE: creating these requires an extraneous full copy of the mesh data, to extract the position stream out of the interleaved vertex data array
    ///  Not optimal, but the performance likely doesn't matter
    template<typename VertexAttrib>
    struct FullMesh {
        MeshBuffer<VertexAttrib> vertexAttributes;
        MeshBuffer<VertexPosition> vertexBasePositions;
        const size_t totalVertices;

        MeshCollider collider;

        static std::shared_ptr<FullMesh<VertexAttrib>> make(CommandJobQueueCallback callback, VmaAllocator allocator, vk::Device device, std::span<MVertex<VertexAttrib>> vertices) {
            std::vector<VertexAttrib> deinterleavedAttrib;
            std::vector<VertexPosition> deinterleavedPos;

            deinterleavedAttrib.reserve(vertices.size());
            deinterleavedPos.reserve(vertices.size());

            double sphereRad = 0.0;

            for (auto& v : vertices) {
                deinterleavedAttrib.push_back(v.attributes);
                deinterleavedPos.push_back(v.position);

                sphereRad = std::max(sphereRad, Vec3(v.position.pos).mag());
            }

            auto va = MeshBuffer<VertexAttrib>::make(callback, allocator, device, deinterleavedAttrib);
            auto vp = MeshBuffer<VertexPosition>::make(callback, allocator, device, deinterleavedPos);

            size_t totalVertices = vertices.size();

            assert(va.totalVertices == totalVertices);
            assert(vp.totalVertices == totalVertices);

            return std::make_shared<FullMesh>(std::move(va), std::move(vp), totalVertices, MeshCollider{sphereRad});
        }

        static std::shared_ptr<FullMesh<VertexAttrib>> make(CommandJobQueueCallback callback,  VmaAllocator allocator, vk::Device device, std::vector<MVertex<VertexAttrib>>&& vertices) {
            std::vector<MVertex<VertexAttrib>> stored(vertices);

            return make(callback, allocator, device, stored);
        }
    };


}