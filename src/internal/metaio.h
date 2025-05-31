#pragma once

#include <string>
#include "medea/primitives.h"

namespace Medea::Internal {
    struct VMaterialFragment {
        std::string src;
        std::string uStructName;
        std::string uArrayName;
        std::string entryFuncName;
        uint32_t materialID;
    };

    struct VMaterialVertex {
        std::string src;
        std::string uStructName;
        std::string uArrayName;
        std::string vStructName;
        std::string vArrayName;
        std::string entryFuncName;
        uint32_t materialID;
    };
}
