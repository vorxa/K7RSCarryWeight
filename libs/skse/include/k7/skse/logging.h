#pragma once

#include "k7/common/diagnostics.h"
#include "k7/common/log_level.h"

namespace k7::skse::logging
{
    // Opens <Documents>/My Games/<game>/SKSE/<pluginName>.log at info level (the VR folder under VR).
    // Called before the config is read, so config errors can be logged.
    void Init(const char* pluginName);

    // Re-applies the level once [diagnostics] log_level is known.
    void SetLevel(common::LogLevel level);

    // "config: file:line: message" at the level that fits the severity: errors and warnings are shown,
    // notes only at debug.
    void LogDiagnostic(const common::Diagnostic& diagnostic);
}
