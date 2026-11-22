#pragma once

#include "../../shared/Source/SongsmithAnalogModels.h"

// ACOMP-10: the selectable compressor models. The table (the voicings, the stable IDs, the model-by-ID lookup) lives in
// shared/Source/SongsmithAnalogModels.h since songsmithstudio#14, because the Songsmith Compressor's Analog Warmth uses the same
// models. The names below are the same things under the Analog Compressor's own namespace, so nothing here changed.
namespace FET
{
    using namespace Songsmith::AnalogModels;
}
