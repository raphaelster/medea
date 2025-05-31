#pragma once

#include "meta.h"
#include <vector>

#include "intdef.h"

#include "constants.h"
#include <unordered_set>
#include "compute.h"

#include "internal/metacodegen.h"

#include "gvector.h"

///current TODO: get some way of streaming the uniform buffers to the GPU
/// maybe this should all be uploaded as a single buffer? Idk.

///real TODO: I need a bimap data structure. That's all.

namespace Medea {

    struct GPUSceneGraph;

    class IMaterialSet {
        public:

        virtual void removeUniform(size_t uid) =0;  //don't need to have an internal mset RID mapping, since the scene graph has to know the uniform idx anyways

        //returns uniform buffer ptr
        virtual vk::DeviceAddress update(VmaAllocator allocator, vk::Device device, vk::CommandBuffer cmd) =0;

        virtual std::pair<Internal::VMaterialVertex, Internal::VMaterialFragment> introspect(const std::string& entryName, uint32_t materialID) const =0;
    };

    namespace Internal {

        /// @brief GPU scene graph vertex 2 fragment
        struct GSGV2F {
            glm::avec3 fragWorldPos;
            glm::avec3 fragWorldNormal;
            glm::avec2 fragUV;
            uint32_t fragTex;
        };

        /// @brief GPU scene graph frag out;
        struct GSGFOut {
            glm::avec4 fragColor;
        };

        struct MeshPtr {
            BufferRef attributeAddress;
            BufferRef positionAddress;
            MeshCollider collider;
            size_t totalVertices;

            template<typename T>
            MeshPtr(FullMesh<T>& base)
                : attributeAddress(base.vertexAttributes.buffer), positionAddress(base.vertexBasePositions.buffer), 
                collider(base.collider), totalVertices(base.totalVertices) {}
        };

        struct RenderEntityInit {
            MeshPtr mesh;
            Placement pos;
            size_t materialID;
            size_t materialUniformIdx;
        };

        
    }

    class RenderWorld;

    template<typename Uniform, typename VIn>
    class MaterialSet : public IMaterialSet{
        //Medea::AllocatedBuffer

        GPUSceneGraph& base;
        glist<Uniform> backing;

        const size_t thisID;

        std::string vertexShaderPath, fragmentShaderPath;

        public:
        using UTYPE = Uniform;
        using VTYPE = VIn;

        MaterialSet(GPUSceneGraph& base_, Core& core, vk::CommandBuffer cmd, std::string_view vtxPath, std::string_view fragPath);

        std::pair<Internal::VMaterialVertex, Internal::VMaterialFragment> introspect(const std::string& entryName, uint32_t materialID) const override {
            std::string vtxSrc = readFile(vertexShaderPath).value();
            std::string fragSrc = readFile(fragmentShaderPath).value();

            return {
                Internal::materialToLibVtx<Uniform, VIn>(vtxSrc, entryName, materialID),
                Internal::materialToLibFrag<Uniform>(fragSrc, entryName, materialID)
            };
        }

        RenderEntityID add(RenderWorld& world, Uniform u, FullMesh<VIn>& mesh, Placement pos);

        vk::DeviceAddress update(VmaAllocator allocator, vk::Device device, vk::CommandBuffer cmd) override {
            backing.gpuUpdate(allocator, device, cmd);

            return backing.getBuffer().getAddress();
        }

        void removeUniform(size_t uid) override {
            backing.remove(uid);
        }

        /// @return Reference; potentially invalidated after adding or removing RenderEntities
        Uniform& getMut(const Medea::RenderWorld& world, RenderEntityID rid);

        /// @return Reference; potentially invalidated after adding or removing RenderEntities
        const Uniform& get(const Medea::RenderWorld& world, RenderEntityID rid) const;
    };

    namespace Internal {
        struct SceneGraphState;

        struct PerFrameDynamicBuffers {
            AllocatedBuffer broadphaseCulledEntities; //sizeof gpuEntities, to avoid worst case of no culling. Also works as indirect draw buffer

            //for now, no additional culling
            //AllocatedBuffer 


            static PerFrameDynamicBuffers make(vk::Device device, VmaAllocator allocator, const glist<RenderEntity>& entities);
        };

        struct FroxelPush {
            glm::mat4 inverseViewProj;
            glm::mat4 view;
            BufferRef inLightdefArray;
            BufferRef outFroxelArray;
        };

        struct GPUDrivenPush {
            glm::mat4 view;
            glm::mat4 projection;
            BufferRef renderEntityArray;
            BufferRef lightdefArray;
            BufferRef froxelArray;
            uint32_t drawcallIdx; 
            float currentTime;
        };

        struct CullCSPush {
            BufferRef inArr;
            BufferRef outArr;
            BufferRef materialUniformBufferMapping;
        };

        struct ShadowTransmittancePush {
            BufferRef volMaterialDataStructure;
            BufferRef lightDefArray;
            glm::float32 currentTime;
        };

        struct ScatteringPush {
            glm::mat4 cameraInvViewProj;
            glm::avec4 cameraPosCurrentTime;
            BufferRef volMaterialDataStructure;
            BufferRef lightDefArray;
            BufferRef froxelArray;
        };

        ///GPUSceneGraph vulkan backend, basically
        template<typename V2F, typename FOut>
        struct GSGBindlessShader {
            vk::raii::PipelineLayout layout;
            vk::raii::Pipeline pipeline;
            vk::raii::DescriptorSetLayout descLayout;
            vk::raii::DescriptorSetLayout volShadowDescLayout;

            VmaAllocator allocator;

            BindlessTextureArray& textures;

            RenderTexture shadowAtlas;
            RenderTexture dummyShadowAtlas;

            DescriptorAllocator dAllocator;

            static std::unique_ptr<GSGBindlessShader> make(Core& core, 
                                                           std::vector<std::reference_wrapper<IMaterialSet>> materials, BindlessTextureArray& textures) {

                std::vector<Internal::VMaterialVertex> vertexMaterials;
                std::vector<Internal::VMaterialFragment> fragMaterials;

                vk::raii::Device& device = core.device;
                VmaAllocator allocator = core.allocator;

                size_t idx = 0;
                for (auto& mat : materials) {
                    std::string name = "Mat"+std::to_string(idx);

                    auto pair = mat.get().introspect(name, idx);

                    vertexMaterials.push_back(std::move(pair.first));
                    fragMaterials.push_back(std::move(pair.second));

                    idx++;
                }

                RenderTexture shadowAtlas = RenderTexture::makeDepth(core, RenderConstants::shadowAtlasResolution);
                RenderTexture dummyShadowAtlas = RenderTexture::makeDepth(core, Coord(1));

                vk::DescriptorSetLayoutBinding shadowAtlasBinding = shadowAtlas.makeBinding(0, vk::ShaderStageFlagBits::eAllGraphics);
                vk::DescriptorSetLayoutBinding texBinding = textures.makeBinding(1, vk::ShaderStageFlagBits::eAllGraphics);

                vk::DescriptorSetLayoutBinding volLightBinding = 
                    vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eAllGraphics);
                vk::DescriptorSetLayoutBinding volShadowBinding = 
                    vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, RenderConstants::maxLights, vk::ShaderStageFlagBits::eAllGraphics);

                std::stringstream bonusStream;
                bonusStream << "layout (set = 0, binding = 0) uniform sampler2DShadow shadowAtlas;\n";
                bonusStream << "layout (set = 0, binding = 1) uniform sampler2D medeaTextures["+std::to_string(textures.MAX_TEXTURES)+"];\n";
                bonusStream << "layout (set = 1, binding = 0) uniform sampler3D volumetricLighting;\n";
                bonusStream << "layout (set = 1, binding = 1) uniform sampler3D volumetricShadows["+std::to_string(RenderConstants::maxLights)+"];\n";

                std::string vtxSrc  = Internal::vmaterialSrcVtx<V2F>(vertexMaterials, bonusStream.str());
                std::string fragSrc = Internal::vmaterialSrcFrag<V2F, FOut>(fragMaterials, bonusStream.str());

                std::vector<vk::DescriptorSetLayoutBinding> imageBindings = {shadowAtlasBinding, texBinding};
                std::vector<vk::DescriptorSetLayoutBinding> volBindings = {volLightBinding, volShadowBinding};

                vk::raii::DescriptorSetLayout descLayout(device, vk::DescriptorSetLayoutCreateInfo({}, imageBindings));

                vk::raii::DescriptorSetLayout vsDescLayout(device, vk::DescriptorSetLayoutCreateInfo({}, volBindings));

                std::vector<vk::DescriptorSetLayout> layouts = {*descLayout, *vsDescLayout};

                vk::PipelineLayoutCreateInfo plci({}, layouts);

                vk::PushConstantRange pushRange({vk::ShaderStageFlagBits::eAllGraphics}, 0, sizeof(Internal::GPUDrivenPush));

                //auto pushArr = {pushPerMat, pushPerInst};

                plci.setPushConstantRanges(pushRange);

                vk::raii::PipelineLayout layout = device.createPipelineLayout(plci);

                PipelineBuilder builder(layout);

                static int bsCount = 0;

                std::string namePref = "BindlessShader"+std::to_string(bsCount);
                std::string nameVtx = namePref+"_Vertex";
                std::string nameFrag = namePref+"_Fragment";

                vk::raii::Pipeline pipeline = builder
                    .setShaders(device, vtxSrc, fragSrc, nameVtx, nameFrag)
                    .setTopology(vk::PrimitiveTopology::eTriangleList)
                    .setPolygonMode(vk::PolygonMode::eFill)
                    .setCullMode()
                    .setMultisampleDisable()
                    .setBlendingDisable()
                    .setDepthTestEnable(true, vk::CompareOp::eLess)
                    .setColorAttachmentFormat(RenderConstants::screenFormat)
                    .setDepthFormat(vk::Format::eD32Sfloat)
                    .build(device);

                std::vector<DescriptorAllocator::PoolSizeRatio> poolRatios = {DescriptorAllocator::PoolSizeRatio(vk::DescriptorType::eCombinedImageSampler, 5 * textures.MAX_TEXTURES)};
                

                return std::make_unique<GSGBindlessShader>(std::move(layout), std::move(pipeline), std::move(descLayout), std::move(vsDescLayout),
                    allocator,  textures,
                    std::move(shadowAtlas),
                    std::move(dummyShadowAtlas),
                    DescriptorAllocator::make(device, 20, poolRatios));
            }
        
        
            void v2Bind(vk::raii::Device& device, vk::CommandBuffer cmd, CleanupJobQueueCallback cleanCallback, vk::Viewport viewport, 
                                const std::vector<AllocatedImage2Ref>& color, std::optional<AllocatedImage2Ref> depth, std::optional<vk::CompareOp> depthOp,
                                std::function<void(vk::DescriptorSet dset)> updateVolShadowDescriptor) {
                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);

                multiTransition(cmd, {*dummyShadowAtlas.image}, {vk::ImageLayout::eShaderReadOnlyOptimal});



                const bool DEPTH_PASS = color.size() == 0;

                if (DEPTH_PASS) {
                    cmd.setCullMode(vk::CullModeFlagBits::eBack);
                }

                if (depthOp) {
                    cmd.setDepthTestEnable(true);
                    cmd.setDepthCompareOp(depthOp.value());
                }
                else cmd.setDepthTestEnable(false);

                std::shared_ptr<vk::raii::DescriptorSet> dset = std::make_shared<vk::raii::DescriptorSet>(dAllocator.allocate(device, descLayout));
                std::shared_ptr<vk::raii::DescriptorSet> volShadowDset = std::make_shared<vk::raii::DescriptorSet>(dAllocator.allocate(device, volShadowDescLayout));

                
                updateVolShadowDescriptor(*volShadowDset);

                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, **dset, {});
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 1, **volShadowDset, {});

                // uniform memory handled via gvector; unnecessary
                //std::vector<std::optional<AllocatedBuffer>> uniformBuffers;

                //for (auto& m : materials) uniformBuffers.push_back(ss->uploadUniforms(allocator));

                std::vector<vk::RenderingAttachmentInfo> colorAttachments;

                {
                    std::vector<AllocatedImage2Ref> fullImgs;
                    std::vector<vk::ImageLayout> layouts;

                    if (depth) {
                        fullImgs.push_back(depth.value());
                        layouts.push_back(vk::ImageLayout::eDepthAttachmentOptimal);
                    }

                    for (auto& img : color) {
                        fullImgs.push_back(img);
                        layouts.push_back(vk::ImageLayout::eColorAttachmentOptimal);
                    }

                    multiTransition(cmd, fullImgs, layouts, false);
                }

                for (auto& c : color) {
                    colorAttachments.push_back(vk::RenderingAttachmentInfo(c.get().imageView, c.get()._currentLayout));
                }

                std::optional<vk::RenderingAttachmentInfo> depthAttachment;

                if (depth) depthAttachment = vk::RenderingAttachmentInfo(depth.value().get().imageView, depth.value().get()._currentLayout);

                vk::RenderingInfo renderInfo;

                vk::Rect2D drawArea({(int32_t) viewport.x, (int32_t) viewport.y}, {(uint32_t) viewport.width, (uint32_t) viewport.height});

                renderInfo.setRenderArea(drawArea)
                    .setColorAttachments(colorAttachments)
                    .setLayerCount(1);

                if (depthAttachment) renderInfo.setPDepthAttachment(&depthAttachment.value());


                {
                    vk::MemoryBarrier2 membar(vk::PipelineStageFlagBits2::eBottomOfPipe, {}, 
                        vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite);
                    vk::DependencyInfo di({}, membar, {}, {});
                    cmd.pipelineBarrier2(di);
                }

                {
                    std::vector<vk::WriteDescriptorSet> writes;

                    std::unique_ptr<Internal::WriteGroup> wg0;
                    auto wg1 = textures.getArrayUpdate(*dset, 1);

                    writes.push_back(wg1->write);

                    if (!DEPTH_PASS) {
                        wg0 = shadowAtlas.getWrite(*dset, 0);

                        writes.push_back(wg0->write);
                    }
                    else {
                        wg0 = dummyShadowAtlas.getWrite(*dset, 0);

                        writes.push_back(wg0->write);
                    }

                    device.updateDescriptorSets(writes, {});

                    //capture group holds onto shared ptr until frame is known to be done processing
                    ///TODO: reconsider
                    cleanCallback([dset, volShadowDset] () {});
                }

                cmd.beginRendering(renderInfo);


                //OpenGL -> Vulkan: invert Y
                viewport.y = viewport.height;
                viewport.height = -viewport.height;

                cmd.setViewport(0, viewport);
                cmd.setScissor(0, drawArea);

                //cmd.pushConstants<Internal::PushV2>(layout, vk::ShaderStageFlagBits::eAllGraphics, 0, push);
            }
        };
    }

    class GPUSceneGraph;

    class RenderWorld {
        glist<RenderEntity> entities;
        std::function<void(size_t materialID, size_t materialIdx)> uniformDeleteCallback;

        public:
        RenderWorld(Core& core, vk::CommandBuffer cmd, GPUSceneGraph& graph);

        //called via MaterialSet, which sets uniform state itself
        RenderEntityID add(Internal::RenderEntityInit init) {

            RenderEntity r{
                0,
                init.mesh.attributeAddress.address,
                init.mesh.positionAddress.address, //<- position stream addr

                init.pos.pos.toGlmVec3(),

                init.pos.dir.toGlmVec4(),

                (glm::float32) init.mesh.collider.sphereRad,  //<- get bounding sphere rad from mesh
                
                (u32) init.materialID,
                (u32) init.materialUniformIdx,

                (u32) init.mesh.totalVertices, //mesh size
                1, //instances
                //first index goes here
                0, //vertex off
                0 //first instance
            };

            size_t rid = entities.add(r);

            return RenderEntityID{rid};
        }

        void setPos(RenderEntityID rid, Placement pos) {
            RenderEntity& e = entities.atMut(rid.ID);

            e.pos = pos.pos.toGlmVec3();
            e.rot = pos.dir.toGlmVec4();
        }


        void remove(RenderEntityID rid) {
            RenderEntity& e = entities.atMut(rid.ID);

            size_t mid = e.materialID;
            size_t muidx = e.materialUniformIdx;
            e.meshSize = 0; //<- signals to broadphase cull that this is a zombie entry

            entities.remove(rid.ID);

            uniformDeleteCallback(e.materialID, e.materialUniformIdx);
            //materialSets.at(e.materialID).get().removeUniform(e.materialUniformIdx);
        }

        u32 getMaterialUIdx(RenderEntityID rid) const {
            return entities.at(rid.ID).materialUniformIdx;
        }

        friend class GPUSceneGraph;
    };


    ///NOTE: Still gotta think through alota stuff here.
    class GPUSceneGraph {
        public:
        using V2F = Internal::GSGV2F;
        using FOut = Internal::GSGFOut;
        
        private:
        //glist<RenderEntity> entities;

        std::vector<std::reference_wrapper<IMaterialSet>> materialSets;

        AllocatedBuffer froxelArray;
        std::vector<AllocatedImage> volumetricShadows;
        std::vector<vk::raii::ImageView> volShadowViews;
        std::vector<vk::raii::Sampler> volShadowSamplers;

        ComputeShader<Internal::CullCSPush> broadphaseCullShader;
        ComputeShader<Internal::FroxelPush> clusterLightShader;
        ComputeShader<Internal::ShadowTransmittancePush> shadowTransmittanceShader;
        ComputeShader<Internal::ScatteringPush> volScatteringShader;
        ComputeShader<Internal::ScatteringPush> volAccumulateShader;

        DescriptorAllocator shadowTransmittanceArrayAllocator;

        BindlessTextureArray& textures;
        
        AllocatedImage volLightingImage;
        vk::raii::Sampler volLightingSampler;

        std::unordered_map<size_t, size_t> ridToUniformIdx;
        
        //std::unique_ptr<Internal::SceneGraphState> renderState;
        //std::vector<

        std::unique_ptr<Internal::GSGBindlessShader<V2F, FOut>> megashader = nullptr;


        public:

        gvector<LightDef> lights;

        GPUSceneGraph(Core& core, vk::CommandBuffer cmd, BindlessTextureArray& texRef);

        void compileMaterialSets(Core& core) {
            megashader = std::remove_reference<decltype(*megashader)>::type::make(core, materialSets, textures);
        }

        void render(Core& core, vk::CommandBuffer cmd, CleanupJobQueueCallback cleanup, glm::mat4 camView, glm::mat4 camProj,
                    RenderWorld& world,
                    vk::Viewport viewport,
                    const std::vector<AllocatedImage2Ref>& color, std::optional<AllocatedImage2Ref> depth, std::optional<vk::CompareOp> depthOp,
                    double currentTime);

        size_t registerMaterialSet(IMaterialSet& mset) {
            assert(megashader.get() == nullptr);

            materialSets.push_back(mset);

            return materialSets.size()-1;
        }

        void deleteUniform(size_t materialID, size_t materialIdx) {
            materialSets.at(materialID).get().removeUniform(materialIdx);
        }

        template<typename U, typename VIn>
        friend class MaterialSet;
    };

    template<typename Uniform, typename VIn>
    MaterialSet<Uniform, VIn>::MaterialSet(GPUSceneGraph& base_, Core& core, vk::CommandBuffer cmd, std::string_view vtxSrc, std::string_view fragSrc)
        : base(base_), backing(core.allocator, core.device, cmd), vertexShaderPath(vtxSrc), fragmentShaderPath(fragSrc), thisID(base_.registerMaterialSet(*this)) {}



    template<typename Uniform, typename VIn>
    RenderEntityID MaterialSet<Uniform, VIn>::add(RenderWorld& world, Uniform u, FullMesh<VIn>& mesh, Placement pos) {
        size_t uidx = backing.add(u);

        Internal::RenderEntityInit init{
            Internal::MeshPtr(mesh),
            pos,
            thisID,
            uidx
        };

        RenderEntityID rid = world.add(init);

        return rid;
    }

    template<typename Uniform, typename VIn>
    Uniform& MaterialSet<Uniform, VIn>::getMut(const Medea::RenderWorld& world, RenderEntityID rid) {
        return backing.atMut(world.getMaterialUIdx(rid));
    }

    template<typename Uniform, typename VIn>
    const Uniform& MaterialSet<Uniform, VIn>::get(const Medea::RenderWorld& world, RenderEntityID rid) const {
        return backing.at(world.getMaterialUIdx(rid));
    }
}