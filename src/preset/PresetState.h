#pragma once

#include "Preset.h"

class OpenRiffBoxProcessor;

namespace PresetState
{
    Preset capture(OpenRiffBoxProcessor& processor);
    void   apply(const Preset& preset, OpenRiffBoxProcessor& processor);
}
