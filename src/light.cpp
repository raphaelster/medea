#include "light.h"

#include "cull.h"

#include "constants.h"

#include <sstream>
#include <iomanip>


#include <optional>

//#include "glm/gtc/type_aligned.hpp"

namespace Medea {

    std::optional<Vec3> planeRayIntersect(const Vec3& planeNormal, const double planeOffset, const Vec3& rayBase, const Vec3& rayDir) {
        double rayEff = planeNormal.dot(rayDir);
        double ray0 = planeNormal.dot(rayBase);

        //want to walk along ray until t * rayEff + ray0 = planeOffset -> t = (planeOffset - ray0) / rayEff

        if (rayEff < 0.001) return std::nullopt;

        double t = (planeOffset - ray0) / rayEff;

        //if (t < -0.001) return std::nullopt;

        return rayBase + rayDir * t;
    }
    

    void Spotlight::updateCollider() {
        Quaternion vdir = offset.dir * pos.dir;
        Vec3 vpos = pos.pos + offset.pos;

        Vec3 fwd = vdir.rotate(Vec3(0,0,-1));
        Vec3 ur = vdir.rotate(Vec3(1,1,0));

        double urDist = depth * std::tan(innerAngleOuterAngle.y / 2.0);

        Vec3 corner = fwd * depth + ur * urDist + vpos;

        //at this point:
        // find equidistant plane between pos.pos and corner
        // find point where fwd intersects with equidistant plane
        // sphere pos = point, sphere rad = distance(pos.pos, point)

        Vec3 planeNormal = (corner - vpos).normalize();

        double planeOffset = (planeNormal.dot(corner) + planeNormal.dot(vpos)) / 2.0;

        Vec3 isctPos = planeRayIntersect(planeNormal, planeOffset, vpos, fwd).value();

        double rad = isctPos.distTo(vpos);

        double rad2 = isctPos.distTo(corner);

        assert(std::abs(rad - rad2) < 0.01);
        

        sphereCollider = Cull::Sphere(Placement(vpos, vdir), depth*1.0);
        //sphereCollider = Cull::Sphere(isctPos, rad);
    }

    glm::mat4 Spotlight::getProj() const {
        return glm::perspective(fov, 1.0, RenderConstants::lightZNear, depth);
    }

    const double Z_HIGH = 5.0;

    Vec3 clampZHigh(const Vec3& in) {
        return Vec3(in.x, in.y, std::min(Z_HIGH, in.z));
    }

    LightPriority Spotlight::getPriority(const Cull::Cone& cameraCone, const glm::mat4& viewProj) const {
        //glm::mat4 lightProjToCamView = context.view * glm::inverse(getViewProj());
        //glm::mat4 camProjToView = glm::inverse(context.projection);
        //glm::mat4 lightProjToCamProj = context.projection * lightProjToCamView;

        if (!Cull::testConeVsSphere(cameraCone, sphereCollider)) return {-1.0, 0};

        AABB3 screenBounds(perspectiveDivide(viewProj * clampZHigh(pos.pos).toGlmVec4Pos()));

        Quaternion dir = offset.dir * pos.dir;

        {
            AABB3 screenClip(Vec3(-1, -1, 0), Vec3(1));


            Vec3 fwd   = dir.rotate(Vec3(0,0,-1));
            Vec3 up    = dir.rotate(Vec3(1,0,0));
            Vec3 right = dir.rotate(Vec3(0,1,0));

            double sideLen = sin(fov / 2.0) * this->depth;

            for (int i=0; i<4; i++) {
                Vec2 mask(i & 1 ? 1 : -1, i & 2 ? 1 : -1);

                Vec3 curOff = up * mask.x * sideLen + right * mask.y * sideLen + fwd * this->depth;

                Vec3 farPos = perspectiveDivide(viewProj * clampZHigh(pos.pos + offset.pos + curOff).toGlmVec4Pos());

                screenBounds.add(farPos);
            }
            
            if (!screenClip.overlap(screenBounds)) return {-1.0, 0};
        }

        //if (!Cull::frustrumInFrustrum(lightProjToCamProj, glm::translate(glm::mat4(1), glm::vec3(0,0,1)))) return {-1.0, 0};

        
        //initial heuristic: screenspace as proj space xy AABB span

        screenBounds.lo = (screenBounds.lo / 2.0 + Vec3(0.5)).piecewiseClamp(Vec3(0.), Vec3(1.));
        screenBounds.hi = (screenBounds.hi / 2.0 + Vec3(0.5)).piecewiseClamp(Vec3(0.), Vec3(1.));

        Vec2 span = screenBounds.hi.xy() - screenBounds.lo.xy();

        double priority = span.x * span.y;

        const double resHeuristicFactor = 2.0;

        const double maxDesiredRes = pow(2.0, log2(RenderConstants::shadowAtlasBlockSize) + maxResPower);

        double rawRes = span.x * span.y * resHeuristicFactor * 512.0; //<- 512 from max light res [512, 512]

        double power = std::ceil(log2(rawRes)) - log2(RenderConstants::shadowAtlasBlockSize);

        power = std::max((double) minResPower, std::min((double) maxResPower, power));

        //round up to next power of 2
        rawRes = pow(2.0, power);
        rawRes = std::max(1.0, std::floor(rawRes));
        rawRes = std::min(rawRes, 512.0 / RenderConstants::shadowAtlasBlockSize);

        return LightPriority{priority, (uint) rawRes};
    }

    u64 coordToZOrder(const Coord& c) {
        u64 out = 0;

        for (size_t i=0; i<32; i++) {
            out |= (c.x & (1 << i)) << i;
            out |= (c.y & (1 << i)) << (i + 1);
        }

        return out;
    }

    Coord zOrderToCoord(u64 in) {
        Coord res(0);

        for (size_t i=0; i<32; i++) {
            res.x |= (in & (1 << (i*2)))     >> i;
            res.y |= (in & (1 << (i*2 + 1))) >> (i + 1);
        }

        return res;
    }

    void printMatrix(std::ostream& trg, const glm::mat4& mat) {
        std::stringstream str;

        str << std::setprecision(2);

        for (u32 x=0; x<4; x++) {
            for (u32 y=0; y<4; y++) {
                str<<std::setw(6)<<mat[y][x]<<" ";
            }
            str<<std::endl;
        }

        trg << str.str();
    }

    void assertMatrixSimilar(const glm::mat4& a, const glm::mat4& b, double epsilon) {

        for (u32 x=0; x<4; x++) for (u32 y=0; y<4; y++) {
            if (std::abs(a[x][y] - b[x][y]) >= epsilon) {
                std::cerr<<"assertMatrixSimilar failed, for A, B:\n";
                
                printMatrix(std::cerr, a);
                std::cerr<<std::endl;
                printMatrix(std::cerr, b);
            }

            assert(std::abs(a[x][y] - b[x][y]) < epsilon);
        }
    }

    void Spotlight::filterLights(std::vector<LightDef>& out, const std::vector<Spotlight>& inLights, const CameraRenderContext& context, 
                const Cull::Cone& cameraCone, Coord shadowAtlasBlockRes) {

        assert(inLights.size() > 0);

        auto priorityCompare = [](auto a, auto b) {
            return a.first.priority > b.first.priority;
        };
        auto resCompare = [](auto a, auto b) {
            return a.first.desiredAtlasRes > b.first.desiredAtlasRes;
        };

        std::vector<std::pair<LightPriority, Spotlight>> cameraCulledLights;

        glm::mat4 viewProj = context.projection * context.view;

        //Placement cameraPos()

        //Cull::Cone cameraCone()

        //cull for LOS
        for (const auto& l : inLights) {
            LightPriority p = l.getPriority(cameraCone, viewProj);

            if (p.priority < 0.0) continue;

            assert(p.desiredAtlasRes >= 1);

            cameraCulledLights.push_back({p, l});
        }

        if (cameraCulledLights.size() == 0) cameraCulledLights.push_back(std::pair<LightPriority, Spotlight>({1, 1}, inLights.at(0)));
        
        std::cout<<"CPU cull: "<<inLights.size()<<" -> "<<cameraCulledLights.size()<<std::endl;

        std::sort(cameraCulledLights.begin(), cameraCulledLights.end(), priorityCompare);


        //make sure lights can fit on the atlas (just culling lowest prio lights for now)
        //TODO: dynamically lower resolution to make them best fit w/o dropping lights?
        std::vector<std::pair<LightPriority, Spotlight>> priorityCulledLights;

        const uint MAX_RES = shadowAtlasBlockRes.x * shadowAtlasBlockRes.y;

        uint totalRes = 0;
        for (auto& p : cameraCulledLights) {
            uint delta = p.first.desiredAtlasRes * p.first.desiredAtlasRes;

            if (totalRes + delta > MAX_RES) {
                std::cerr<<"Total resolution saturated"<<std::endl;
                break;
            }

            totalRes += delta;

            priorityCulledLights.push_back(p);
        }

        std::cout<<"Shadow atlas occupancy: "<<totalRes / (MAX_RES + 0.0)<<std::endl;

        //sort by resolution, for shadow atlas occupancy + to discard lowest res lights
        std::stable_sort(priorityCulledLights.begin(), priorityCulledLights.end(), resCompare);

        //add to out lightdef list
        out.clear();

        uint lightAtlasZOrderIdx = 0;
        for (auto& p : priorityCulledLights) {
            const LightPriority& prio = p.first;
            const Spotlight& light = p.second;

            uint curSize = prio.desiredAtlasRes * prio.desiredAtlasRes;

            assert(lightAtlasZOrderIdx < shadowAtlasBlockRes.x * shadowAtlasBlockRes.y);

            Coord p0 = zOrderToCoord(lightAtlasZOrderIdx);

            lightAtlasZOrderIdx += curSize;

            assert(p0.x >= 0 && p0.y >= 0);
            assert(p0.x + prio.desiredAtlasRes <= shadowAtlasBlockRes.x && p0.y + prio.desiredAtlasRes <= shadowAtlasBlockRes.y);

            Vec2 atlasPos = Vec2(p0) / Vec2(shadowAtlasBlockRes);
            Vec2 atlasExtents = Vec2(prio.desiredAtlasRes) / Vec2(shadowAtlasBlockRes);

            double coneAngle = light.fov / 2.0;

            if (out.size() >= RenderConstants::maxLights) {
                std::cerr<<"Total light count saturated"<<std::endl;
                break;
            }

            assert(p0.x <= 255 && p0.y <= 255 && p0.x >= 0 && p0.y >= 0 && prio.desiredAtlasRes <= 255);

            Vec3 testFwd = light.pos.dir.rotate(Vec3(0,0,-1));

            u8 ldx = (u8) p0.x;
            u8 ldy = (u8) p0.y;
            u8 ldw = (u8) prio.desiredAtlasRes;
            u8 ldh = (u8) prio.desiredAtlasRes;


            out.push_back(LightDef{
                light.getViewProj(),
                glm::vec4((light.pos.pos + light.offset.pos).toGlmVec3(), light.depth),
                (light.offset.dir * light.pos.dir).toGlmVec4(),
                light.color.toGlmVec3(),
                light.innerAngleOuterAngle.toGlmVec2(),
                u32pack(ldx, ldy, ldw, ldh)});

            /*assertMatrixSimilar(p.second.pos.toInvMat4(), out.back().getView(), 0.01);
            assertMatrixSimilar(p.second.getProj(), out.back().getProj(), 0.01);
            assertMatrixSimilar(p.second.getViewProj(), out.back().getViewProj(), 0.01);*/
        }
    }
}