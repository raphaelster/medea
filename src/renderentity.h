#pragma once

#include "primitives.h"

#include "intdef.h"

#include "3dmath.h"

#include "medea/constants.h"

namespace Medea {

    struct RenderEntityID {
        const size_t ID;
    };

    struct alignas(16) RenderEntity {
        uint64_t materialUniformArrayAddress;   //NOTE: this is null until broadphase cull, then it's replaced with addr (bc resize invalidates ptrs)
        uint64_t meshAddress; //<- if I'm using buffer device address, I can just offset this s.t. v[0] = 0
        uint64_t positionStreamAddress; //gotta go here because uhh alignment. Ptr to vertex position mesh array

        glm::avec3 pos;

        glm::avec4 rot; //<- quaternion

        glm::float32 boundingSphereRad; //TODO: switch to or add better cull primitive later?

        //scale?
        //instance count? Might be useful if I want to do e.g. grass.
        //Add flags for whether or not this thing ought to be rendered in shadowing, etc? Shadowing grass is NO, shadowing large vegetation is probably YES

        uint32_t materialID;    //index of which material this entity uses. material uniform array addr derived from this in broadphase cull
        uint32_t materialUniformIdx; //<- index in material uniform array

        //these entries form an indirect drawcall.
        uint32_t meshSize; //<- numTriangles * 3. indexCount
        uint32_t instanceCount = 1;
        //uint32_t firstIndex = 0; //<- if I copy all (unique) mesh indices into a single buffer, then I can just offset into it fairly simply. 
        // But, ^ is a bit harder if you have to stream in mesh data (for example, for runtime created creatures). Can ignore for a while
        int32_t vertexOffset = 0;
        uint32_t firstInstance = 0;
    };

    
    struct alignas(sizeof(float)*4) LightDef {
        glm::mat4 viewProj; // can reconstruct view from quat, can reconstruct proj from lightDepth + outerAngle + some fixed near value
        //glm::avec4 atlasPosExtents;
        glm::avec4 worldPosLightDepth;
        glm::avec4 dirQuat;
        //glm::avec4 forwardAngle; // <- angle in radians
        glm::avec3 color;   // <- unnormalized
        glm::avec2 innerAngleOuterAngle;
        u32 atlasPosExtentsPacked;  //least significant byte is X, byte 2 is Y, byte 3 is W, most significant byte is H

        glm::mat4 getProj() const {
            return glm::perspective<double>(innerAngleOuterAngle.y, 1.0, RenderConstants::lightZNear, worldPosLightDepth.w);
        }

        glm::mat4 getView() const {
            Quaternion q(dirQuat);

            glm::vec3 pos(-worldPosLightDepth.x, -worldPosLightDepth.y, -worldPosLightDepth.z);

            glm::mat4 tf = glm::translate(glm::mat4(1), pos);

            return q.conjugate().toMat4() * tf;
        }

        glm::mat4 getViewProj() const {
            return viewProj;
            //return getProj() * getView();
        }

        glm::avec4 getOldAtlasPosExtents() {
            auto [x, y, w, h] = u32unpack(atlasPosExtentsPacked);

            const Vec2 dim = Vec2(RenderConstants::shadowAtlasBlockResolution); 

            Vec2 pos = Vec2(x, y) / dim;
            Vec2 span = Vec2(w, h) / dim;

            return glm::avec4(pos.x, pos.y, span.x, span.y);

        }
    };
}