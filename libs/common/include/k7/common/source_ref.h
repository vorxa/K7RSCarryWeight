#pragma once

#include <string>

namespace k7::common
{
    // Where a configuration entry came from: the file and line of the layer that last defined it.
    struct SourceRef
    {
        std::string file;
        int         line{};
    };
}
