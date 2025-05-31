#pragma once

#include "meta.h"
#include <vector>

#include "intdef.h"

#include "constants.h"
#include <unordered_set>
#include "compute.h"

#include "internal/metacodegen.h"

///current TODO: get some way of streaming the uniform buffers to the GPU
/// maybe this should all be uploaded as a single buffer? Idk.

///real TODO: I need a bimap data structure. That's all.

namespace Medea {
    
    ///TODO: this is an overestimate, couple to the actual value when finalized
    const int BUF_FRAMES_IN_FLIGHT = 3;


    using AllocatedImage2Ref = std::reference_wrapper<AllocatedImage>;

    ///TODO: have some way of deleting "old" entries w/o having to fully cycle the rolling buf
    template<typename T>
    class RollingBufferBase {
        std::vector<T> buffers;

        int idx = -1;

        bool initialized = false;

        public:
        RollingBufferBase(T&& initial) {
            //we do not want resizing to invalidate ptrs when this has an intended fixed size anyways
            buffers.reserve(BUF_FRAMES_IN_FLIGHT);
            buffers.push_back(std::move(initial));
            initialized = true;
            idx = 0;
        }

        RollingBufferBase(const RollingBufferBase&) = delete;
        RollingBufferBase& operator=(const RollingBufferBase&) = delete;

        RollingBufferBase(RollingBufferBase&&) = default;
        RollingBufferBase& operator=(RollingBufferBase&&) = default;

        RollingBufferBase() {
            buffers.reserve(BUF_FRAMES_IN_FLIGHT);
        }

        T& get() {
            assert(initialized && idx >= 0);

            return buffers.at(idx);
        }

        const T& get() const {
            assert(initialized && idx >= 0);

            return buffers.at(idx);
        }

        bool isInitialized() { return initialized; }

        /// NOTE: this should really only be called once per frame; I'm sidestepping syncing issues by just storing old data until we know we've waited on
        ///  the frame it was sent to be finished
        T& next(T&& newObj) {
            initialized = true;
            idx = (idx+1) % BUF_FRAMES_IN_FLIGHT;

            if (buffers.size() < BUF_FRAMES_IN_FLIGHT) buffers.push_back(std::move(newObj));
            else buffers.at(idx) = std::move(newObj);

            return buffers.at(idx);
        }
    };

    using RollingBuffer = RollingBufferBase<AllocatedBuffer>;
    using RollingBufferImage = RollingBufferBase<AllocatedImage>;


    template<typename T>
    class gvector {
        std::vector<T> backing;
        std::set<size_t> modified;

        RollingBuffer gpuBacking;   //on-gpu memory
        RollingBuffer transferDataBuf;  //transfer buffer; on-CPU

        size_t gpuCapacity;

        const size_t INITIAL_SIZE = 50;

        size_t getBytesFromSize(size_t capacity) {
            return capacity * sizeof(T) + RenderConstants::arrayHeaderSize;
        }
        
        void makeGpuBacking(VmaAllocator alloc, vk::Device device, vk::CommandBuffer cmd, size_t capacity) {
            gpuBacking.next(AllocatedBuffer(device, alloc, getBytesFromSize(capacity), 
                    vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress
                     | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc,
                    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE));
        }

        void resizeGpuBacking(VmaAllocator alloc, vk::Device device, vk::CommandBuffer cmd, size_t newCapacity) {
            AllocatedBuffer& old = gpuBacking.get();

            size_t copySize = old.size;

            assert(getBytesFromSize(newCapacity) > old.info.size);

            makeGpuBacking(alloc, device, cmd, newCapacity);
            
            AllocatedBuffer& cur = gpuBacking.get();

            auto copy = {vk::BufferCopy2(0, 0, copySize)};

            vk::CopyBufferInfo2 cbuf(old.buffer, cur.buffer, copy);

            cmd.copyBuffer2(cbuf);
        }

        public:
        gvector(const gvector&) = delete;
        gvector& operator=(const gvector&) = delete;

        gvector(VmaAllocator allocator, vk::raii::Device& device, vk::CommandBuffer cmd, const std::vector<T>& initial, size_t initialCapacity) {
            backing = initial;

            gpuCapacity = std::max(INITIAL_SIZE, initialCapacity);

            for (size_t i=0; i<initial.size(); i++) modified.insert(i);
            
            makeGpuBacking(allocator, device, cmd, gpuCapacity);
        }

        gvector(VmaAllocator allocator, vk::raii::Device& device, vk::CommandBuffer cmd, const std::vector<T>& initial)
            : gvector(allocator, device, cmd, initial, initial.size() * 2) {}


        gvector(VmaAllocator allocator, vk::raii::Device& device, vk::CommandBuffer cmd, size_t initialCapacity)
            : gvector(allocator, device, cmd, {}, initialCapacity) {}

        gvector(VmaAllocator allocator, vk::raii::Device& device, vk::CommandBuffer cmd)
            : gvector(allocator, device, cmd, {}, INITIAL_SIZE) {}

        gvector(gvector&& old) = default;
        gvector& operator=(gvector&& old) = default;

        void push_back(const T& obj) {
            modified.insert(backing.size());
            backing.push_back(obj);
        }

        void pop_back() {
            backing.pop_back();
        }

        void clear() {
            backing.clear();
        }

        void remove(size_t i) {
            assert(i >= 0 && i < backing.size());

            modified.insert(i);
            backing.at(i) = backing.at(backing.size()-1);
            backing.pop_back();
        }

        auto begin() const {
            return backing.begin();
        }

        auto end() const {
            return backing.end();
        }

        T& atMut(size_t i) {
            modified.insert(i);
            return backing.at(i);
        }

        const T& at(size_t i) const {
            return backing.at(i);
        }

        const T& operator[](size_t i) const {
            return backing.at(i);
        }

        size_t size() const {
            return backing.size();
        }

        operator BufferRef() {
            return gpuBacking.get();
        }

        AllocatedBuffer& getBuffer() {
            return gpuBacking.get();
        }
        const AllocatedBuffer& getBuffer() const {
            return gpuBacking.get();
        }

        void gpuUpdate(VmaAllocator allocator, vk::Device device, vk::CommandBuffer cmd) {
            if (modified.size() == 0) return;

            std::vector<size_t> idxList;
            std::vector<T> patchList;

            for (size_t idx : modified) {
                if (idx >= backing.size()) continue;

                idxList.push_back(idx);
                patchList.push_back(backing.at(idx));
            }

            modified.clear();

            vk::BufferUsageFlags tbufDefault = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc;

            transferDataBuf.next(AllocatedBuffer::loadCPUWithHeader(allocator, device, std::span<T>(patchList), tbufDefault, backing.size()));
            //transferIdxBuf.next( AllocatedBuffer::loadCPUWithSize(allocator, idxList,   bufDefault));


            if (backing.size() > gpuCapacity) {
                gpuCapacity *= 2;

                resizeGpuBacking(allocator, device, cmd, gpuCapacity);
            }



            std::vector<vk::BufferCopy2> copies = {vk::BufferCopy2(0, 0, RenderConstants::arrayHeaderSize)};
            for (size_t i=0; i<idxList.size(); i++) {
                size_t trgOff = idxList.at(i) * sizeof(T) + RenderConstants::arrayHeaderSize;
                size_t srcOff = i * sizeof(T) + RenderConstants::arrayHeaderSize;

                size_t objSize = sizeof(T);

                assert(trgOff + objSize <= gpuBacking.get().info.size);
                assert(srcOff + objSize <= transferDataBuf.get().info.size);

                if (i > 0 && idxList.at(i-1) == idxList.at(i)-1) {
                    copies.at(copies.size()-1).size += objSize;
                }
                else {
                    copies.push_back(vk::BufferCopy2(srcOff, trgOff, sizeof(T)));
                }
            }

            cmd.copyBuffer2(vk::CopyBufferInfo2(transferDataBuf.get().buffer, gpuBacking.get().buffer, copies));
        }
    };


    template<typename T>
    class glist { //like gvector, but removed entities are zombies (so indices are stable)
        gvector<T> backing;
        std::set<size_t> empty;

        public:

        glist(VmaAllocator allocator, vk::raii::Device& device, vk::CommandBuffer cmd) 
            : backing(allocator, device, cmd, {}) {}


        size_t add(const T& obj) {
            if (empty.size()) {
                size_t front = *empty.begin();

                if (front < backing.size()) {
                    backing.atMut(front) = obj;
                    empty.erase(front);
                    return front;
                }
                else empty.clear();
            }
            backing.push_back(obj);
            return backing.size()-1;
        }

        size_t size() const {
            return backing.size();
        }

        void pop_back() {
            backing.pop_back();
        }

        void remove(size_t i) {
            assert(!empty.count(i));
            empty.insert(i);
        }

        T& atMut(size_t i) {
            assert(!empty.count(i));

            return backing.atMut(i);
        }

        const T& at(size_t i) const {
            assert(!empty.count(i));

            return backing.at(i);
        }

        const T& operator[](size_t i) const {
            assert(!empty.count(i));

            return backing[i];
        }

        operator BufferRef() {
            return backing;
        }

        AllocatedBuffer& getBuffer() {
            return backing.getBuffer();
        }
        const AllocatedBuffer& getBuffer() const {
            return backing.getBuffer();
        }

        void gpuUpdate(VmaAllocator allocator, vk::Device device, vk::CommandBuffer cmd) {
            backing.gpuUpdate(allocator, device, cmd);
        }
    };

}