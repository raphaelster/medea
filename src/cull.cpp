#include "cull.h"


bool Cull::frustrumInFrustrum(glm::mat4 projToCull0, glm::mat4 projToCull1) {
    AABB3 a0(transformPoint(projToCull0, Vec3(-1,-1,-1)));
    AABB3 a1(transformPoint(projToCull1, Vec3(-1,-1,-1)));

    for (size_t i=1; i<8; i++) {
        Vec3 p(i & 1 ? 1 : -1, i & 2 ? 1 : -1, i & 4 ? 1 : -1);

        a0.add(transformPoint(projToCull0, p));
        a1.add(transformPoint(projToCull1, p));
    }

    return a0.overlap(a1);
}



bool Cull::testConeVsSphere(Vec3 origin, Vec3 fwd, double size, double angle, Vec3 testSpherePos, double testSphereRad) {
    const Vec3 V = testSpherePos - origin;
    const float VlenSq = V.dot(V);
    const float V1len  = V.dot(fwd);
    const float distanceClosestPoint = cos(angle) * sqrt(VlenSq - V1len*V1len) - V1len * sin(angle);
 
    const bool angleCull = distanceClosestPoint > testSphereRad;
    const bool frontCull = V1len >  testSphereRad + size;
    const bool backCull  = V1len < -testSphereRad;

    return !(angleCull || frontCull || backCull);
}