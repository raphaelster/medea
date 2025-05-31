#pragma once

#define GLM_FORCE_SIMD_AVX2
#define GLM_FORCE_ALIGNED_GENTYPES
#include <glm/glm.hpp>

namespace glm {
    struct alignas(4*4) avec4 : public glm::vec4 {
        using glm::vec4::vec4;


        avec4(const glm::vec4& other) 
            : glm::vec4(other) {}
    };

    struct alignas(4*4) avec3 : public glm::vec3 {
        using glm::vec3::vec3;

        avec3(const glm::vec3& other) 
            : glm::vec3(other) {}
    };

    struct alignas(4*2) avec2 : public glm::vec2 {
        using glm::vec2::vec2;

        avec2(const glm::vec2& other) 
            : glm::vec2(other) {}
    };

};
