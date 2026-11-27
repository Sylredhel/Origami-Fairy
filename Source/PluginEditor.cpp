#include "PluginEditor.h"

OrigamiFairyAudioProcessorEditor::OrigamiFairyAudioProcessorEditor(OrigamiFairyAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), scopeView(p)
{
    setLookAndFeel(&laf);
    setSize(1020, 636);

    addAndMakeVisible(scopeView);
    addAndMakeVisible(transferView);
    addAndMakeVisible(mascotView);

    // Header: Factory Preset Browser + Save/Load + Oversampling
    addAndMakeVisible(prevPresetBtn);
    addAndMakeVisible(presetBox);
    addAndMakeVisible(nextPresetBtn);
    addAndMakeVisible(savePresetBtn);
    addAndMakeVisible(loadPresetBtn);

    const auto& factories = OrigamiPresetManager::getFactoryPresets();
    for (int i = 0; i < static_cast<int>(factories.size()); ++i)
        presetBox.addItem(factories[static_cast<size_t>(i)].name, i + 1);
    presetBox.setSelectedId(1, juce::dontSendNotification);

    presetBox.onChange = [this]()
    {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0)
            loadFactoryPresetByIdx(idx);
    };
    prevPresetBtn.onClick = [this]()
    {
        const int n = static_cast<int>(OrigamiPresetManager::getFactoryPresets().size());
        loadFactoryPresetByIdx((currentFactoryPresetIdx - 1 + n) % n);
    };
    nextPresetBtn.onClick = [this]()
    {
        const int n = static_cast<int>(OrigamiPresetManager::getFactoryPresets().size());
        loadFactoryPresetByIdx((currentFactoryPresetIdx + 1) % n);
    };
    savePresetBtn.onClick = [this]() { openSavePresetDialog(); };
    loadPresetBtn.onClick = [this]() { openLoadPresetDialog(); };

    osBox.addItemList({ "OS: OFF (1x)", "OS: 2x FIR", "OS: 4x FIR", "OS: 8x FIR", "OS: 16x FIR" }, 1);
    addAndMakeVisible(osBox);

    // Card 1: Lost-Waveform Detail Topology & Crossover
    engineModeBox.addItemList({ "NEST (Clean Origami)", "FOLD (Origami Crease)", "DPC (Classic Ring)" }, 1);
    phaseModeBox.addItemList({ "LINEAR PHASE (FIR)", "MIN PHASE (LR4)" }, 1);
    addAndMakeVisible(engineModeBox);
    addAndMakeVisible(phaseModeBox);

    setupKnob(detailFreqKnob,     " Hz", 0xff2f8578,
              "DETAIL FREQ: Only clipped/saturated waveform ABOVE this frequency is rescued & folded back.");
    setupKnob(detailPreserveKnob, " %",  0xff2f8578,
              "PRESERVE: Amount of lost high-freq detail rescued when clipping occurs (0 dB change when unclipped!).");

    // Card 2: Saturation, Origami Crease & Dynamic Drive Envelope Follower
    satCurveBox.addItemList({ "TAPE (Warm)", "TUBE (Asym)", "SOFT CLIP", "HARD CLIP" }, 1);
    foldShapeBox.addItemList({ "SMOOTH FOLD", "CRISP ORIGAMI", "SOFT CEILING" }, 1);
    addAndMakeVisible(satCurveBox);
    addAndMakeVisible(foldShapeBox);

    setupKnob(driveKnob,     " dB", 0xffc0842c,
              "INPUT DRIVE: Maximum drive applied to loud hits. When Env Follow > 0%, red dot shows live dynamic drive!");
    setupKnob(ceilingKnob,   " dB", 0xffc6523c,
              "FOLD CEILING: Maximum output limit (+/- C). All rescued high-freq detail stays safely inside this line.");
    setupKnob(foldDepthKnob, " %",  0xff2f8578,
              "CREASE DEPTH: Blends between transparent ceiling guard (0%) and full continuous origami wavefolding (100%).");

    setupKnob(envFollowKnob,  " %",  0xff7b4d7c,
              "ENV FOLLOW: Dynamically lowers Drive on quiet tails/decays so drums & reverb never sound elongated or muddy.");
    setupKnob(envReleaseKnob, " ms", 0xff7b4d7c,
              "ENV RELEASE: How fast Drive backs off after a peak (20-50ms = tight drums; 80-150ms = smooth vocals/bus).");

    // Card 3: Monitor & Output
    auditionBox.addItemList({ "NORMAL", "FOLDED DELTA", "BODY ONLY" }, 1);
    addAndMakeVisible(auditionBox);

    setupKnob(mixKnob,     " %",  0xff466987,
              "PARALLEL MIX: Phase-aligned blend between dry input and Origami Fairy output.");
    setupKnob(outGainKnob, " dB", 0xff466987,
              "OUTPUT GAIN: Final makeup trim after ceiling folding.");
    addAndMakeVisible(autoGainBtn);
    addAndMakeVisible(bypassBtn);

    registerTip(presetBox,     "FACTORY PRESETS: 12 curated starting points including tight Env Follow drum & vocal presets.");
    registerTip(prevPresetBtn, "Previous factory preset.");
    registerTip(nextPresetBtn, "Next factory preset.");
    registerTip(savePresetBtn, "Save your current settings as a reusable .xml user preset.");
    registerTip(loadPresetBtn, "Load a saved .xml user preset from disk.");
    registerTip(osBox,         "OVERSAMPLING: Linear-phase equiripple anti-aliasing FIR (OFF / 2x / 4x / 8x / 16x). 8x recommended.");

    registerTip(engineModeBox, "NEST: Clean un-rectified detail pocket | FOLD: Bright inward wavefold | DPC: Classic Au5 ring-mod edge.");
    registerTip(phaseModeBox,  "LINEAR FIR: Peak-aligned zero-phase detail extraction | MIN LR4: Zero pre-ringing for punchy drums.");
    registerTip(satCurveBox,   "SAT CURVE: Choose warm Tape, asymmetric Tube, transparent Soft Clip, or punchy Hard Clip.");
    registerTip(foldShapeBox,  "CREASE SHAPE: Infinite C^2 Smooth Fold, Crisp geometric triangle fold, or pure Soft Ceiling guard.");
    registerTip(auditionBox,   "NORMAL: Full output | FOLDED DELTA: Solo rescued HF detail only | BODY ONLY: Hear base saturation.");
    registerTip(autoGainBtn,   "AUTO GAIN: Compensates loudness as you push Drive so you can judge tone fairly.");
    registerTip(bypassBtn,     "BYPASS: Exact latency-compensated A/B bypass with zero timing shift.");
    registerTip(mascotView,    "Origami Fairy (Sylredhel): Quietly tucks clipped high-frequency details back inside your ceiling.");
    registerTip(scopeView,     "LIVE SCOPE: Ochre = dynamically driven input; Sage-Teal = folded output safely inside ceiling.");
    registerTip(transferView,  "TRANSFER CURVE: Continuous periodic C^2 Smooth Fold & Crisp Origami folding across +/- Ceiling.");

    auto& a = processorRef.apvts;
    attEngineMode     = std::make_unique<CAtt>(a, "engineMode",     engineModeBox);
    attPhaseMode      = std::make_unique<CAtt>(a, "phaseMode",      phaseModeBox);
    attOversampling   = std::make_unique<CAtt>(a, "oversampling",   osBox);
    attDetailFreq     = std::make_unique<SAtt>(a, "detailFreq",     detailFreqKnob);
    attDetailPreserve = std::make_unique<SAtt>(a, "detailPreserve", detailPreserveKnob);

    attSatCurve       = std::make_unique<CAtt>(a, "satCurve",       satCurveBox);
    attFoldShape      = std::make_unique<CAtt>(a, "foldShape",      foldShapeBox);
    attDrive          = std::make_unique<SAtt>(a, "driveDb",        driveKnob);
    attCeiling        = std::make_unique<SAtt>(a, "ceilingDb",      ceilingKnob);
    attFoldDepth      = std::make_unique<SAtt>(a, "foldDepth",      foldDepthKnob);
    attEnvFollow      = std::make_unique<SAtt>(a, "envFollow",      envFollowKnob);
    attEnvRelease     = std::make_unique<SAtt>(a, "envReleaseMs",   envReleaseKnob);

    attAudition       = std::make_unique<CAtt>(a, "audition",       auditionBox);
    attMix            = std::make_unique<SAtt>(a, "mixPct",         mixKnob);
    attOutGain        = std::make_unique<SAtt>(a, "outGainDb",      outGainKnob);
    attAutoGain       = std::make_unique<BAtt>(a, "autoGain",       autoGainBtn);
    attBypass         = std::make_unique<BAtt>(a, "bypass",         bypassBtn);

    addMouseListener(this, true);
    startTimerHz(30);
}

OrigamiFairyAudioProcessorEditor::~OrigamiFairyAudioProcessorEditor()
{
    stopTimer();
    removeMouseListener(this);
    setLookAndFeel(nullptr);
}

void OrigamiFairyAudioProcessorEditor::setupKnob(juce::Slider& s, const juce::String& suffix,
                                                 uint32_t accentHex, const juce::String& tip)
{
    using namespace sylredhel::sketch;
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
    s.setTextValueSuffix(suffix);
    s.setColour(juce::Slider::textBoxTextColourId,       kGraphite);
    s.setColour(juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxBackgroundColourId, kPaperBot.withAlpha(0.65f));
    s.getProperties().set(LookAndFeelSketchbook::kAccentProp, static_cast<int>(accentHex));
    s.setRotaryParameters(juce::degreesToRadians(220.0f), juce::degreesToRadians(500.0f), true);
    addAndMakeVisible(s);
    registerTip(s, tip);
}

void OrigamiFairyAudioProcessorEditor::registerTip(juce::Component& c, const juce::String& tip)
{
    c.getProperties().set("hoverTip", tip);
}

void OrigamiFairyAudioProcessorEditor::mouseEnter(const juce::MouseEvent& e)
{
    auto* comp = e.eventComponent;
    while (comp != nullptr && comp != this)
    {
        if (comp->getProperties().contains("hoverTip"))
        {
            hoverTipText = comp->getProperties()["hoverTip"].toString();
            repaint(0, getHeight() - 32, getWidth(), 32);
            return;
        }
        comp = comp->getParentComponent();
    }
}

void OrigamiFairyAudioProcessorEditor::mouseExit(const juce::MouseEvent&)
{
    hoverTipText = "Hover over any control for a quick guide  |  Tip: Raise ENV FOLLOW to keep drum tails & quiet passages clean!";
    repaint(0, getHeight() - 32, getWidth(), 32);
}

void OrigamiFairyAudioProcessorEditor::loadFactoryPresetByIdx(int idx)
{
    const auto& list = OrigamiPresetManager::getFactoryPresets();
    if (idx < 0 || idx >= static_cast<int>(list.size()))
        return;

    currentFactoryPresetIdx = idx;
    presetBox.setSelectedId(idx + 1, juce::dontSendNotification);
    OrigamiPresetManager::applyFactoryPreset(processorRef.apvts, idx);
    hoverTipText = "Preset: " + list[static_cast<size_t>(idx)].tip;
    repaint(0, getHeight() - 32, getWidth(), 32);
}

void OrigamiFairyAudioProcessorEditor::openSavePresetDialog()
{
    auto startDir = OrigamiPresetManager::getUserPresetDirectory();
    fileChooser = std::make_unique<juce::FileChooser>(
        "Save Origami Fairy Preset", startDir.getChildFile("MyOrigamiPreset.xml"), "*.xml");

    constexpr int flags = juce::FileBrowserComponent::saveMode
                        | juce::FileBrowserComponent::canSelectFiles
                        | juce::FileBrowserComponent::warnAboutOverwriting;

    fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f != juce::File {})
        {
            auto target = f.hasFileExtension(".xml") ? f : f.withFileExtension(".xml");
            if (OrigamiPresetManager::saveUserPreset(processorRef.apvts, target))
            {
                hoverTipText = "Saved preset: " + target.getFileName();
                repaint(0, getHeight() - 32, getWidth(), 32);
            }
        }
    });
}

void OrigamiFairyAudioProcessorEditor::openLoadPresetDialog()
{
    auto startDir = OrigamiPresetManager::getUserPresetDirectory();
    fileChooser = std::make_unique<juce::FileChooser>(
        "Load Origami Fairy Preset", startDir, "*.xml");

    constexpr int flags = juce::FileBrowserComponent::openMode
                        | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f.existsAsFile())
        {
            if (OrigamiPresetManager::loadUserPreset(processorRef.apvts, f))
            {
                presetBox.setText(f.getFileNameWithoutExtension(), juce::dontSendNotification);
                hoverTipText = "Loaded user preset: " + f.getFileName();
                repaint(0, getHeight() - 32, getWidth(), 32);
            }
        }
    });
}

void OrigamiFairyAudioProcessorEditor::timerCallback()
{
    const float ceilDb    = static_cast<float>(ceilingKnob.getValue());
    const float envFollow = static_cast<float>(envFollowKnob.getValue());
    const float liveDrvDb = processorRef.getLiveDriveDb();

    // Update live terracotta indicator arc on INPUT DRIVE knob when ENV FOLLOW > 0%
    if (envFollow > 0.5f)
    {
        const double liveNorm = juce::jlimit(0.0, 1.0, static_cast<double>(liveDrvDb) / 24.0);
        driveKnob.getProperties().set(LookAndFeelSketchbook::kLivePosProp, liveNorm);
        driveKnob.repaint();
    }
    else if (static_cast<double>(driveKnob.getProperties().getWithDefault(LookAndFeelSketchbook::kLivePosProp, -1.0)) >= 0.0)
    {
        driveKnob.getProperties().set(LookAndFeelSketchbook::kLivePosProp, -1.0);
        driveKnob.repaint();
    }

    scopeView.updateFrame(ceilDb, engineModeBox.getSelectedItemIndex(),
                          phaseModeBox.getSelectedItemIndex(), osBox.getSelectedItemIndex(), envFollow);
    transferView.updateParams(satCurveBox.getSelectedItemIndex(),
                              foldShapeBox.getSelectedItemIndex(),
                              static_cast<float>(foldDepthKnob.getValue()),
                              ceilDb);
    mascotView.updateActivity(processorRef.getFoldActivityDb(),
                              processorRef.getDetailSavedLevel());
}

void OrigamiFairyAudioProcessorEditor::paint(juce::Graphics& g)
{
    using namespace sylredhel::sketch;
    const float w = static_cast<float>(getWidth());
    const float h = static_cast<float>(getHeight());

    // 1. Warm Cream Sketchbook Paper Backdrop
    juce::ColourGradient paper(kPaperTop, 0.0f, 0.0f, kPaperBot, 0.0f, h, false);
    g.setGradientFill(paper);
    g.fillAll();

    g.setColour(kGridLine.withAlpha(0.35f));
    for (float y = 58.0f; y < h - 30.0f; y += 26.0f)
        g.drawHorizontalLine(static_cast<int>(y), 24.0f, w - 12.0f);

    // Sketchbook spiral binding holes
    g.setColour(kGraphiteMid.withAlpha(0.20f));
    g.drawVerticalLine(20, 0.0f, h);
    for (float y = 26.0f; y < h - 18.0f; y += 34.0f)
    {
        g.setColour(kGraphite.withAlpha(0.18f));
        g.fillEllipse(6.0f, y, 8.0f, 8.0f);
        g.setColour(kGraphiteMid.withAlpha(0.45f));
        g.drawEllipse(6.0f, y, 8.0f, 8.0f, 1.0f);
        g.drawLine(0.0f, y + 4.0f, 9.0f, y + 4.0f, 1.4f);
    }

    // 2. Top Sketchbook Header Strip
    g.setColour(kCardFill.withAlpha(0.85f));
    g.fillRect(21.0f, 0.0f, w - 21.0f, 52.0f);
    g.setColour(kGraphiteMid.withAlpha(0.55f));
    g.drawHorizontalLine(52, 21.0f, w);
    g.setColour(kGraphiteLt.withAlpha(0.30f));
    g.drawHorizontalLine(54, 21.0f, w);

    juce::Path gem;
    gem.startNewSubPath(42.0f, 12.0f);
    gem.lineTo(54.0f, 25.0f);
    gem.lineTo(42.0f, 38.0f);
    gem.lineTo(30.0f, 25.0f);
    gem.closeSubPath();
    g.setColour(kSageTeal.withAlpha(0.26f));
    g.fillPath(gem);
    g.setColour(kGraphite);
    g.strokePath(gem, juce::PathStrokeType(1.5f));
    g.drawLine(42.0f, 12.0f, 42.0f, 38.0f, 1.0f);
    g.drawLine(30.0f, 25.0f, 54.0f, 25.0f, 0.9f);

    g.setColour(kGraphiteMid);
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::italic)));
    g.drawText("Sylredhel", 64, 7, 150, 14, juce::Justification::centredLeft);

    g.setColour(kGraphite);
    g.setFont(juce::Font(juce::FontOptions(19.5f, juce::Font::bold)));
    g.drawText("ORIGAMI FAIRY", 63, 21, 195, 24, juce::Justification::centredLeft);

    g.setColour(kSageTeal);
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.drawText("v2.1", 220, 26, 36, 16, juce::Justification::centredLeft);

    // 3. Three Sketchbook Control Cards
    auto drawCard = [&g](juce::Rectangle<float> r, const juce::String& title, juce::Colour accent)
    {
        g.setColour(kCardFill.withAlpha(0.92f));
        g.fillRoundedRectangle(r, 6.0f);
        drawSketchBox(g, r, 6.0f, kGraphiteMid);

        g.setColour(accent.withAlpha(0.32f));
        g.fillRoundedRectangle(r.getX() + 12.0f, r.getY() + 21.0f, 152.0f, 5.0f, 2.5f);

        g.setColour(kGraphite);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText(title, static_cast<int>(r.getX() + 14), static_cast<int>(r.getY() + 8),
                   static_cast<int>(r.getWidth() - 28), 16, juce::Justification::centredLeft);
    };

    drawCard({ 32.0f,  284.0f, 260.0f, 308.0f }, "1. LOST-DETAIL RESCUE",        kSageTeal);
    drawCard({ 304.0f, 284.0f, 468.0f, 308.0f }, "2. SATURATION, CREASE & ENV",  kOchre);
    drawCard({ 784.0f, 284.0f, 204.0f, 308.0f }, "3. MONITOR & OUTPUT",          kTerracotta);

    auto lbl = [&g](const juce::String& txt, int x, int y, int width, juce::Colour col = kGraphiteMid)
    {
        g.setColour(col);
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.drawText(txt, x, y, width, 13, juce::Justification::centred);
    };

    // Card 1 Labels
    lbl("RESCUE TOPOLOGY",  46, 318, 232, kSageTeal.darker(0.15f));
    lbl("CROSSOVER PHASE",  46, 372, 232, kSlateBlue);
    lbl("DETAIL FREQ",      48, 438, 106);
    lbl("DETAIL PRESERVE", 164, 438, 114);

    // Card 2 Labels
    lbl("SATURATION CURVE", 322, 318, 206, kOchre.darker(0.18f));
    lbl("ORIGAMI CREASE",   544, 318, 206, kSageTeal.darker(0.15f));

    // Faint vertical dashed pencil divider between Saturation/Crease and Env Follower
    const float dashes[2] = { 4.0f, 3.0f };
    g.setColour(kGraphiteLt.withAlpha(0.55f));
    g.drawDashedLine(juce::Line<float>(580.0f, 392.0f, 580.0f, 518.0f), dashes, 2, 1.1f);

    lbl("DRIVE",      316, 396, 84, kOchre.darker(0.20f));
    lbl("CEILING",    402, 396, 84, kTerracotta);
    lbl("CREASE",     488, 396, 84, kSageTeal.darker(0.15f));
    lbl("ENV FOLLOW", 588, 396, 84, kPlum);
    lbl("ENV REL",    674, 396, 84, kPlum);

    g.setColour(kGraphiteMid);
    g.setFont(juce::Font(juce::FontOptions(8.7f, juce::Font::italic)));
    g.drawFittedText("Env Follow automatically pulls Drive back on quiet tails so drums & reverb stay tight.\nSmooth Fold now folds continuously across +/- Ceiling with exact C^2 rounded creases.",
                     318, 536, 440, 38, juce::Justification::centred, 2);

    // Card 3 Labels
    lbl("AUDITION",  798, 318, 176, kTerracotta);
    lbl("MIX",       796, 382,  86);
    lbl("OUT GAIN",  888, 382,  86);

    // 4. Bottom 1-Line Hover Guide Bar
    const auto bar = juce::Rectangle<float>(32.0f, h - 32.0f, w - 64.0f, 24.0f);
    g.setColour(kCardFill.withAlpha(0.90f));
    g.fillRoundedRectangle(bar, 4.0f);
    drawSketchBox(g, bar, 4.0f, kGraphiteLt);

    g.setColour(kSageTeal);
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.drawText("GUIDE:", static_cast<int>(bar.getX() + 10), static_cast<int>(bar.getY()), 48, 24,
               juce::Justification::centredLeft);

    g.setColour(kGraphite);
    g.setFont(juce::Font(juce::FontOptions(10.2f, juce::Font::plain)));
    g.drawText(hoverTipText, static_cast<int>(bar.getX() + 58), static_cast<int>(bar.getY()),
               static_cast<int>(bar.getWidth() - 68), 24, juce::Justification::centredLeft, true);
}

void OrigamiFairyAudioProcessorEditor::resized()
{
    // Header: Preset Bar + Oversampling Selector
    prevPresetBtn.setBounds(284, 13,  26, 26);
    presetBox.setBounds(314,     13, 260, 26);
    nextPresetBtn.setBounds(578, 13,  26, 26);
    savePresetBtn.setBounds(612, 13,  56, 26);
    loadPresetBtn.setBounds(672, 13,  56, 26);
    osBox.setBounds(842,         13, 146, 26);

    // Upper Visual Row: Scope + Transfer Curve + Origami Fairy Mascot
    scopeView.setBounds(32,     64, 534, 208);
    transferView.setBounds(578, 64, 230, 208);
    mascotView.setBounds(820,   64, 168, 208);

    // Card 1: Lost-Detail Rescue
    engineModeBox.setBounds(46,     334, 232, 28);
    phaseModeBox.setBounds(46,      388, 232, 28);
    detailFreqKnob.setBounds(52,    456,  98, 108);
    detailPreserveKnob.setBounds(168, 456, 98, 108);

    // Card 2: Saturation, Origami Crease & Dynamic Drive Env Follower
    satCurveBox.setBounds(322,  334, 206, 28);
    foldShapeBox.setBounds(544, 334, 206, 28);

    driveKnob.setBounds(316,      414, 84, 102);
    ceilingKnob.setBounds(402,    414, 84, 102);
    foldDepthKnob.setBounds(488,  414, 84, 102);
    envFollowKnob.setBounds(588,  414, 84, 102);
    envReleaseKnob.setBounds(674, 414, 84, 102);

    // Card 3: Monitor & Output
    auditionBox.setBounds(798, 334, 176, 28);
    mixKnob.setBounds(796,     400,  86, 100);
    outGainKnob.setBounds(888, 400,  86, 100);
    autoGainBtn.setBounds(796, 526,  86,  34);
    bypassBtn.setBounds(888,   526,  86,  34);
}
