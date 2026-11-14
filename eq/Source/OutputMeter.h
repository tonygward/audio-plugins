#pragma once

#include "../../shared/Source/SongsmithOutputMeter.h"

// The EQ's output meter now lives in the shared library (shared/Source/SongsmithOutputMeter.h) so every plugin can use it.
using OutputMeterDisplay = Songsmith::UI::OutputMeterDisplay;
