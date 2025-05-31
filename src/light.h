#pragma once

#include "renderentity.h"
#include "3dmath.h"

#include <compare>

#include "context.h"
#include "intdef.h"
#include "cull.h"

namespace Medea {

    struct LightPriority {
        double priority;
        uint desiredAtlasRes; //<- in blocksize, not pixel span
    };

    //point lights are 6 spotlights
    class Spotlight {
        Placement pos;
        Placement offset;
        double depth;
        double fov; //<- in degrees
        Vec3 color;
        Vec2 innerAngleOuterAngle;
        u32 minResPower;
        u32 maxResPower;

        Cull::Sphere sphereCollider;

        glm::mat4 getProj() const;

        glm::mat4 getViewProj() const {
            Placement curPos = pos;

            curPos.pos = curPos.pos + offset.pos;
            curPos.dir = offset.dir * curPos.dir;

            return getProj() * curPos.toInvMat4();
        }

        void updateCollider();

        public:
        static const u32 stdMinResolution = 0;
        static const u32 stdMaxResolution = 4;

        Spotlight(Placement p, Placement off, double _depth, double angleDegrees, Vec3 _color, 
                  u32 minShadowResolutionPower = stdMinResolution, u32 maxShadowResolutionPower = stdMaxResolution, 
                  double falloffInnerAngle = 180.0, double falloffOuterAngle = 180.0)
            : pos(p), offset(off), depth(_depth), fov(glm::radians(angleDegrees)), color(_color), minResPower(minShadowResolutionPower), maxResPower(maxShadowResolutionPower),
              innerAngleOuterAngle(glm::radians(falloffInnerAngle/2.0), glm::radians(falloffOuterAngle/2.0)) {
            updateCollider();
        }

        void setPos(const Placement& p) {
            if (pos == p) return;

            pos = p;
            updateCollider();
        }

        //gonna combine this with culling; if <0, the light should be culled from view
        LightPriority getPriority(const Cull::Cone& cameraCone, const glm::mat4& cameraViewProj) const;
        //const LightDef toLightDef();

        static void filterLights(std::vector<LightDef>& out, const std::vector<Spotlight>& inLights, const CameraRenderContext& context, 
                                 const Cull::Cone& cameraPos, Coord shadowAtlasBlockRes);
    };
};