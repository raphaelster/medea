#pragma once

#include "primitives.h"

#include "aabb.h"

inline const Vec3 perspectiveDivide(const glm::vec4& in) {
    return Vec3::GlmXYZ(in) / in.w;
}
inline const Vec3 transformPoint(const glm::mat4& m, const Vec3& v) {
    return perspectiveDivide(m * v.toGlmVec4Pos());
}

namespace Cull {

    struct Cone {
        Vec3 pos;
        Vec3 fwd;
        double length;
        double angle; //<- in radians; half-angle

        Cone(const Placement& p, double len, double ang)
            : pos(p.pos), fwd(p.dir.rotate(Vec3(0,0,-1))), length(len), angle(ang) {}
    };

    struct Sphere {
        Vec3 pos;
        double rad;

        Sphere(const Placement& p, double r)
            : pos(p.pos), rad(r) {}

        Sphere(const Vec3& p, double r)
            : pos(p), rad(r) {}

        Sphere()
            : pos(0), rad(0) {}
    };

    extern bool frustrumInFrustrum(glm::mat4 projToCull0, glm::mat4 projToCull1);

    extern bool testConeVsSphere(Vec3 origin, Vec3 fwd, double size, double angle, Vec3 testSpherePos, double testSphereRad);

    inline bool testConeVsSphere(const Cull::Cone& cone, const Sphere& sphere) {
        return testConeVsSphere(cone.pos, cone.fwd, cone.length, cone.angle, sphere.pos, sphere.rad);
    }
}
