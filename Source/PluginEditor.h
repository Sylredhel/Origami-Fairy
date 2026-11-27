#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PresetManager.h"
#include "LookAndFeelSketchbook.h"

class OrigamiFairyAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit OrigamiFairyAudioProcessorEditor(OrigamiFairyAudioProcessor&);
    ~OrigamiFairyAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    void setupKnob(juce::Slider& s, const juce::String& suffix, uint32_t accentHex, const juce::String& tip);
    void registerTip(juce::Component& c, const juce::String& tip);
    void loadFactoryPresetByIdx(int idx);
    void openSavePresetDialog();
    void openLoadPresetDialog();

    OrigamiFairyAudioProcessor& processorRef;
    LookAndFeelSketchbook laf;
    OrigamiScopeView      scopeView;
    OrigamiTransferView   transferView;
    OrigamiFairyMascot    mascotView;

    // Header Preset Bar & Oversampling Selector
    juce::TextButton prevPresetBtn { "<" };
    juce::ComboBox   presetBox;
    juce::TextButton nextPresetBtn { ">" };
    juce::TextButton savePresetBtn { "SAVE" };
    juce::TextButton loadPresetBtn { "LOAD" };
    juce::ComboBox   osBox;

    // Card 1: Lost-Waveform Detail Topology & Crossover
    juce::ComboBox engineModeBox, phaseModeBox;
    juce::Slider   detailFreqKnob, detailPreserveKnob;

    // Card 2: Saturation, Origami Crease & Dynamic Drive Envelope Follower
    juce::ComboBox satCurveBox, foldShapeBox;
    juce::Slider   driveKnob, ceilingKnob, foldDepthKnob;
    juce::Slider   envFollowKnob, envReleaseKnob;

    // Card 3: Monitor & Output
    juce::ComboBox     auditionBox;
    juce::Slider       mixKnob, outGainKnob;
    juce::ToggleButton autoGainBtn { "AUTO GAIN" };
    juce::ToggleButton bypassBtn   { "BYPASS" };

    juce::String hoverTipText { "Hover over any control for a quick guide, or pick a factory preset above." };
    int currentFactoryPresetIdx = 0;
    std::unique_ptr<juce::FileChooser> fileChooser;

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<CAtt> attEngineMode, attPhaseMode, attOversampling, attSatCurve, attFoldShape, attAudition;
    std::unique_ptr<SAtt> attDetailFreq, attDetailPreserve, attDrive, attCeiling, attFoldDepth;
    std::unique_ptr<SAtt> attEnvFollow, attEnvRelease, attMix, attOutGain;
    std::unique_ptr<BAtt> attAutoGain, attBypass;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrigamiFairyAudioProcessorEditor)
};
