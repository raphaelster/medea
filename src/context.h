#pragma once

#include "3dmath.h"

struct CameraRenderContext {
    glm::mat4 view, projection;

    CameraRenderContext(const glm::mat4& v, const glm::mat4& proj)
        : view(v), projection(proj) {}
};