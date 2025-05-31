#pragma once

#include "primitives.h"

namespace Medea {
    struct VertexPosition {
        glm::avec3 pos;
        glm::avec3 normal;
    };


    template<typename T>
    struct MVertex {
        VertexPosition position;
        T attributes;

        template<typename... Args>
        MVertex(glm::vec3 pos, glm::vec3 normal, Args&&... args)
            : position(pos, normal), attributes(args...) {}
        
        template<typename... Args>
        MVertex(VertexPosition vp, Args&&... args)
            : position(vp), attributes(args...) {}
    };
}