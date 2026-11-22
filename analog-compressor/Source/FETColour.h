#pragma once

#include "../../shared/Source/SongsmithAnalogModels.h"

// Analogue colour stages (the FET gain cell and the Class A output stage with its transformer). They live in
// shared/Source/SongsmithAnalogModels.h since songsmithstudio#14, because the Songsmith Compressor's Analog Warmth uses the same code.
// The calibration and the published figures they follow are in analog-compressor/docs/colour.md. These are the same types under the
// Analog Compressor's own namespace, so nothing here changed.
namespace FET
{
    using namespace Songsmith::AnalogModels;
}
