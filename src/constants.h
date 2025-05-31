#pragma once

#include "intdef.h"

namespace Medea {
    namespace Froxel{
        constexpr u64 FROXELS_W = 24;
        constexpr u64 FROXELS_H = 12;
        constexpr u64 FROXELS_Z = 8;
    }

    namespace VolShadow {
        const glm::ivec3 VOL_SHADOW_RES = glm::ivec3(32, 32, 32);
    }

    namespace VolLighting {
        const glm::ivec3 VOL_LIGHTING_RES = glm::ivec3(192*2,96*2,64/2);
    }

    namespace RenderConstants {
        constexpr uint64_t maxLights = 2048;
        
        constexpr uint32_t maxLightsPerTile = 255;
        //const uint32_t maxDecalsPerTile = ...

        constexpr Coord shadowAtlasResolution = Coord(1024*8, 1024*8);
        constexpr uint shadowAtlasBlockSize = 64;

        static_assert(shadowAtlasResolution.x / shadowAtlasBlockSize <= 256, "Need to be able to pack shadow atlas coords into bytes");
        static_assert(shadowAtlasResolution.y / shadowAtlasBlockSize <= 256, "Need to be able to pack shadow atlas coords into bytes");

        constexpr Coord shadowAtlasBlockResolution = Coord(shadowAtlasResolution.x / shadowAtlasBlockSize, shadowAtlasResolution.y / shadowAtlasBlockSize);

        static_assert(shadowAtlasResolution.x % shadowAtlasBlockSize == 0 && shadowAtlasResolution.y % shadowAtlasBlockSize == 0, "Shadow atlas resolution must be an integer multiple of shadow atlas block size");
        //TODO: power of 2 check

        constexpr size_t arrayHeaderSize = 4*4;

        constexpr double lightZNear = 0.5; 
    }
}