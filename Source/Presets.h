#pragma once
#include "Params.h"
#include <vector>

struct PedalSteelPreset {
    const char* name;
    const char* category;
    const char* description;
    PedalSteelParams params;
};

const std::vector<PedalSteelPreset>& getPedalSteelPresets();
