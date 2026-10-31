/*
  ==============================================================================

   BEGIN_JUCE_MODULE_DECLARATION

    ID:                 ms_core
    vendor:             nova
    version:            0.1.0
    name:               NOVA Core
    description:        Gemeinsamer Analyse-Kern der Suite: Band-Spektrum, Lautheit, Zielkurven, Look & Feel
    license:            Proprietary
    minimumCppStandard: 20

    dependencies:       juce_audio_formats juce_dsp juce_gui_basics

   END_JUCE_MODULE_DECLARATION

  ==============================================================================
*/

#pragma once
#define MS_CORE_H_INCLUDED

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "analysis/BandLayout.h"
#include "analysis/SpectrumEngine.h"
#include "analysis/SpectrumSnapshot.h"
#include "analysis/TargetCurve.h"
#include "analysis/TargetBuilder.h"
#include "analysis/ReferenceAnalysis.h"
#include "analysis/LiveAnalyser.h"
#include "ui/SuiteLookAndFeel.h"
