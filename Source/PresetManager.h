#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

struct OrigamiPreset
{
    juce::String name;
    juce::String tip;
    int   engineMode;     // 0=NEST, 1=FOLD, 2=DPC
    int   phaseMode;      // 0=Linear FIR, 1=Min LR4
    int   satCurve;       // 0=Tape, 1=Tube, 2=SoftClip, 3=HardClip
    int   foldShape;      // 0=Smooth, 1=Crisp, 2=SoftCeiling
    int   oversampling;   // 0=OFF, 1=2x, 2=4x, 3=8x, 4=16x
    float driveDb;
    float ceilingDb;
    float detailFreq;
    float detailPreserve; // %
    float foldDepth;      // %
    float envFollow;      // % (0 = static drive, 100 = full dynamic drive)
    float envReleaseMs;   // ms
    float mixPct;
    float outGainDb;
    bool  autoGain;
};

class OrigamiPresetManager
{
public:
    static const std::vector<OrigamiPreset>& getFactoryPresets()
    {
        static const std::vector<OrigamiPreset> presets = {
            { "01. Init (Clean Nest)",
              "Balanced starting point: gentle tape saturation with clean ceiling nesting.",
              0, 0, 0, 0, 3,  4.0f, -0.3f, 1400.0f, 100.0f, 100.0f,   0.0f,  75.0f, 100.0f, 0.0f, false },

            { "02. Master - Transparent Ceiling",
              "Clean mastering clipper: catches peaks transparently while keeping hi-hats & air intact.",
              0, 0, 2, 0, 3,  3.5f, -0.3f, 1500.0f, 100.0f,  60.0f,  25.0f,  90.0f, 100.0f, 0.0f, false },

            { "03. Master - Analog Tape Polish",
              "Warm magnetic tape saturation with zero high-end dulling on loud drops.",
              0, 0, 0, 0, 3,  4.5f, -0.5f, 1800.0f,  95.0f,  75.0f,  20.0f, 110.0f, 100.0f, 0.0f, false },

            { "04. Mix Bus - Tight Dynamic Glue",
              "Tube glue with 55% Env Follow: thickens loud hits while keeping quiet breaks clean & open.",
              0, 0, 1, 0, 3,  6.5f, -0.5f, 1200.0f,  90.0f,  70.0f,  55.0f,  95.0f, 100.0f, 0.0f, true  },

            { "05. Drum Bus - Tight Env Origami",
              "Env Follow (75%) + fast 45ms release keeps drum tails punchy & dry with smooth origami peaks.",
              1, 1, 2, 0, 3,  9.5f, -1.0f, 1600.0f, 110.0f, 100.0f,  75.0f,  45.0f, 100.0f, 0.0f, false },

            { "06. Drum Bus - Punchy Hard DPC",
              "Classic Au5 DPC ring-mod edge on hard-clipped drum transients with Env Follow tail control.",
              2, 0, 3, 1, 3,  9.5f, -0.5f, 2200.0f, 100.0f, 100.0f,  60.0f,  55.0f, 100.0f, 0.0f, false },

            { "07. 808 + Hi-Hat Separator",
              "Hard-clip heavy sub/808 energy without choking the hi-hat shimmer on top.",
              0, 0, 3, 0, 3, 10.0f, -0.3f, 2500.0f, 115.0f,  50.0f,  35.0f,  85.0f, 100.0f, 0.0f, false },

            { "08. Snare - Snappy Transient Bite",
              "High Env Follow (85%) & 35ms release saturates ONLY the snare crack, leaving room decay crisp.",
              2, 1, 3, 0, 3, 12.0f, -1.5f, 1100.0f, 120.0f, 100.0f,  85.0f,  35.0f, 100.0f, 0.0f, false },

            { "09. Vocal - Silk Peak Tamer",
              "Env Follow (65%) keeps quiet breaths untouched while smoothly folding loud vocal belts.",
              0, 0, 1, 0, 3,  6.0f, -1.5f, 2800.0f, 100.0f,  40.0f,  65.0f, 120.0f, 100.0f, 0.0f, true  },

            { "10. Bass - Sub Weight + Fold Grit",
              "Thick tube body with continuous C^2 smooth origami folding above 650 Hz.",
              1, 0, 1, 0, 3, 12.0f, -1.0f,  650.0f,  85.0f, 100.0f,  25.0f, 110.0f, 100.0f, 0.0f, true  },

            { "11. Synth Lead - Infinite Smooth Fold",
              "Multi-cycle C^2 smooth origami wavefolding for liquid bright synth harmonics.",
              1, 0, 0, 0, 4, 13.5f, -3.5f, 1500.0f, 130.0f, 100.0f,  40.0f,  80.0f, 100.0f, 0.0f, true  },

            { "12. Parallel Smash - Air Intact",
              "Heavy parallel hard-clip saturation at 45% mix with clean nested high detail.",
              0, 0, 3, 0, 3, 15.0f, -2.0f, 1800.0f, 110.0f,  50.0f,  50.0f,  60.0f,  45.0f, 0.0f, true  }
        };
        return presets;
    }

    static void applyFactoryPreset(juce::AudioProcessorValueTreeState& apvts, int index)
    {
        const auto& list = getFactoryPresets();
        if (index < 0 || index >= static_cast<int>(list.size()))
            return;

        const auto& p = list[static_cast<size_t>(index)];
        auto setParam = [&](const juce::String& id, float plainVal)
        {
            if (auto* param = apvts.getParameter(id))
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost(param->convertTo0to1(plainVal));
                param->endChangeGesture();
            }
        };

        setParam("engineMode",     static_cast<float>(p.engineMode));
        setParam("phaseMode",      static_cast<float>(p.phaseMode));
        setParam("satCurve",       static_cast<float>(p.satCurve));
        setParam("foldShape",      static_cast<float>(p.foldShape));
        setParam("oversampling",   static_cast<float>(p.oversampling));
        setParam("driveDb",        p.driveDb);
        setParam("ceilingDb",      p.ceilingDb);
        setParam("detailFreq",     p.detailFreq);
        setParam("detailPreserve", p.detailPreserve);
        setParam("foldDepth",      p.foldDepth);
        setParam("envFollow",      p.envFollow);
        setParam("envReleaseMs",   p.envReleaseMs);
        setParam("mixPct",         p.mixPct);
        setParam("outGainDb",      p.outGainDb);
        setParam("autoGain",       p.autoGain ? 1.0f : 0.0f);
        setParam("audition",       0.0f);
        setParam("bypass",         0.0f);
    }

    static juce::File getUserPresetDirectory()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                       .getChildFile("Sylredhel")
                       .getChildFile("Origami Fairy")
                       .getChildFile("Presets");
        if (!dir.exists())
            dir.createDirectory();
        return dir;
    }

    static bool saveUserPreset(juce::AudioProcessorValueTreeState& apvts, const juce::File& file)
    {
        auto state = apvts.copyState();
        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        if (xml == nullptr)
            return false;
        return xml->writeTo(file);
    }

    static bool loadUserPreset(juce::AudioProcessorValueTreeState& apvts, const juce::File& file)
    {
        auto xml = juce::XmlDocument::parse(file);
        if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            return true;
        }
        return false;
    }
};
