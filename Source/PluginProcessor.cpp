#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

OrigamiFairyAudioProcessor::OrigamiFairyAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "OrigamiFairyV2Params", createParameterLayout())
{
    for (size_t i = 0; i < 4; ++i)
    {
        const size_t order = i + 1; // 1->2x, 2->4x, 3->8x, 4->16x
        osSatBank[i]  = std::make_unique<juce::dsp::Oversampling<float>>(
            4, order, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true);
        osFoldBank[i] = std::make_unique<juce::dsp::Oversampling<float>>(
            4, order, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true);
    }

    pEngineMode     = apvts.getRawParameterValue("engineMode");
    pPhaseMode      = apvts.getRawParameterValue("phaseMode");
    pSatCurve       = apvts.getRawParameterValue("satCurve");
    pFoldShape      = apvts.getRawParameterValue("foldShape");
    pOversampling   = apvts.getRawParameterValue("oversampling");
    pDriveDb        = apvts.getRawParameterValue("driveDb");
    pCeilingDb      = apvts.getRawParameterValue("ceilingDb");
    pDetailFreq     = apvts.getRawParameterValue("detailFreq");
    pDetailPreserve = apvts.getRawParameterValue("detailPreserve");
    pFoldDepth      = apvts.getRawParameterValue("foldDepth");
    pEnvFollow      = apvts.getRawParameterValue("envFollow");
    pEnvReleaseMs   = apvts.getRawParameterValue("envReleaseMs");
    pMix            = apvts.getRawParameterValue("mixPct");
    pOutGainDb      = apvts.getRawParameterValue("outGainDb");
    pAutoGain       = apvts.getRawParameterValue("autoGain");
    pAudition       = apvts.getRawParameterValue("audition");
    pBypass         = apvts.getRawParameterValue("bypass");
}

OrigamiFairyAudioProcessor::~OrigamiFairyAudioProcessor()
{
    cancelPendingUpdate();
}

juce::AudioProcessorValueTreeState::ParameterLayout OrigamiFairyAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 1. Lost-Waveform Detail Topology & Crossover
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "engineMode", 1 }, "Detail Topology",
        juce::StringArray { "NEST (Clean Origami)", "FOLD (Origami Crease)", "DPC (Classic Ring)" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "phaseMode", 1 }, "Crossover Phase",
        juce::StringArray { "LINEAR PHASE (FIR)", "MIN PHASE (LR4)" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "oversampling", 1 }, "Oversampling",
        juce::StringArray { "OFF (1x)", "2x FIR", "4x FIR", "8x FIR", "16x FIR" }, 3));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "detailFreq", 1 }, "Detail Frequency",
        juce::NormalisableRange<float>(150.0f, 8000.0f, 1.0f, 0.35f), 1400.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "detailPreserve", 1 }, "Detail Preserve",
        juce::NormalisableRange<float>(0.0f, 150.0f, 0.5f), 100.0f));

    // 2. Saturation, Origami Crease & Dynamic Drive Envelope Follower
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "satCurve", 1 }, "Saturation Curve",
        juce::StringArray { "TAPE (Warm)", "TUBE (Asym)", "SOFT CLIP", "HARD CLIP" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "foldShape", 1 }, "Origami Crease",
        juce::StringArray { "SMOOTH FOLD", "CRISP ORIGAMI", "SOFT CEILING" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "driveDb", 1 }, "Input Drive",
        juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 4.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ceilingDb", 1 }, "Fold Ceiling",
        juce::NormalisableRange<float>(-12.0f, 0.0f, 0.1f), -0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "foldDepth", 1 }, "Crease Depth",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f), 100.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "envFollow", 1 }, "Env Follow",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "envReleaseMs", 1 }, "Env Release",
        juce::NormalisableRange<float>(15.0f, 350.0f, 1.0f, 0.45f), 75.0f));

    // 3. Output, Mix & Monitor
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "mixPct", 1 }, "Parallel Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f), 100.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "outGainDb", 1 }, "Output Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "autoGain", 1 }, "Auto Gain", false));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "audition", 1 }, "Audition",
        juce::StringArray { "NORMAL", "FOLDED DELTA", "BODY ONLY" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return { params.begin(), params.end() };
}

double OrigamiFairyAudioProcessor::computeTotalLatencyForOs(int osIdx) const noexcept
{
    double osLat = 0.0;
    if (osIdx >= 1 && osIdx <= 4)
    {
        const size_t bankIdx = static_cast<size_t>(osIdx - 1);
        osLat = static_cast<double>(osSatBank[bankIdx]->getLatencyInSamples())
              + static_cast<double>(osFoldBank[bankIdx]->getLatencyInSamples());
    }
    return static_cast<double>(firLatencyK) + osLat;
}

void OrigamiFairyAudioProcessor::handleAsyncUpdate()
{
    setLatencySamples(pendingLatencyAtomic.load(std::memory_order_relaxed));
}

void OrigamiFairyAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    hostFs = std::max(sampleRate, 8000.0);
    currentFsAtomic.store(hostFs, std::memory_order_relaxed);

    const int maxBlock = std::max(4096, samplesPerBlock);
    stage4Buf.setSize(4, maxBlock, false, true, true);
    dryAlignedBuf.setSize(2, maxBlock, false, true, true);
    driveGainBuf.setSize(1, maxBlock, false, true, true);

    for (size_t i = 0; i < 4; ++i)
    {
        osSatBank[i]->initProcessing(static_cast<size_t>(maxBlock));
        osFoldBank[i]->initProcessing(static_cast<size_t>(maxBlock));
        osSatBank[i]->reset();
        osFoldBank[i]->reset();
    }

    firXover.prepare(hostFs);
    lr4Xover.prepare(hostFs);
    firLatencyK = firXover.getLatencySamples();

    activeOsIdx = juce::jlimit(0, 4, static_cast<int>(std::round(pOversampling->load())));
    const double factor = (activeOsIdx == 0) ? 1.0 : static_cast<double>(1 << activeOsIdx);
    waveshaper.prepareForRate(hostFs * factor);
    waveshaper.reset();
    driveEnv.prepareForRate(hostFs * factor);
    driveEnv.reset();

    exactTotalLatency = computeTotalLatencyForOs(activeOsIdx);
    const int reportedLat = static_cast<int>(std::round(exactTotalLatency));
    latencyAtomic.store(reportedLat, std::memory_order_relaxed);
    pendingLatencyAtomic.store(reportedLat, std::memory_order_relaxed);
    setLatencySamples(reportedLat);

    const int maxLat = static_cast<int>(std::ceil(computeTotalLatencyForOs(4))) + 64;
    dryAlignDelay.prepare(maxLat);
    driveTrackDelay.prepare(maxLat);
    minPhasePreDelay.prepare(firLatencyK + 32);
    satAlignDelay.prepare(firLatencyK + 32);

    waveScope.reset();

    const double rampSec = 0.015;
    smDriveDb.reset(hostFs, rampSec);
    smDriveLin.reset(hostFs, rampSec);
    smCeilingSat.reset(hostFs, rampSec);
    smCeilingFold.reset(hostFs, rampSec);
    smDetailFreq.reset(hostFs, rampSec);
    smDetailPreserve.reset(hostFs, rampSec);
    smFoldDepth.reset(hostFs, rampSec);
    smEnvFollow.reset(hostFs, rampSec);
    smEnvReleaseMs.reset(hostFs, rampSec);
    smMix.reset(hostFs, rampSec);
    smOutGainLin.reset(hostFs, rampSec);

    const double initDrvDb = pDriveDb->load();
    const double initCeil  = std::pow(10.0, pCeilingDb->load() * 0.05);
    smDriveDb.setCurrentAndTargetValue(initDrvDb);
    smDriveLin.setCurrentAndTargetValue(std::pow(10.0, initDrvDb * 0.05));
    smCeilingSat.setCurrentAndTargetValue(initCeil);
    smCeilingFold.setCurrentAndTargetValue(initCeil);
    smDetailFreq.setCurrentAndTargetValue(pDetailFreq->load());
    smDetailPreserve.setCurrentAndTargetValue(pDetailPreserve->load() * 0.01);
    smFoldDepth.setCurrentAndTargetValue(pFoldDepth->load() * 0.01);
    smEnvFollow.setCurrentAndTargetValue(pEnvFollow->load() * 0.01);
    smEnvReleaseMs.setCurrentAndTargetValue(pEnvReleaseMs->load());
    smMix.setCurrentAndTargetValue(pMix->load() * 0.01);
    smOutGainLin.setCurrentAndTargetValue(std::pow(10.0, pOutGainDb->load() * 0.05));
    liveDriveDbAtomic.store(static_cast<float>(initDrvDb), std::memory_order_relaxed);
}

void OrigamiFairyAudioProcessor::releaseResources()
{
    cancelPendingUpdate();
    for (size_t i = 0; i < 4; ++i)
    {
        osSatBank[i]->reset();
        osFoldBank[i]->reset();
    }
    firXover.reset();
    lr4Xover.reset();
    waveshaper.reset();
    driveEnv.reset();
    dryAlignDelay.reset();
    driveTrackDelay.reset();
    minPhasePreDelay.reset();
    satAlignDelay.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool OrigamiFairyAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto outSet = layouts.getMainOutputChannelSet();
    const auto inSet  = layouts.getMainInputChannelSet();
    if (outSet != juce::AudioChannelSet::mono() && outSet != juce::AudioChannelSet::stereo())
        return false;
    return outSet == inSet;
}
#endif

void OrigamiFairyAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numInCh    = getTotalNumInputChannels();
    const int numOutCh   = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = numInCh; i < numOutCh; ++i)
        buffer.clear(i, 0, numSamples);

    if (numSamples <= 0 || numInCh <= 0)
        return;

    if (stage4Buf.getNumSamples() < numSamples)
    {
        stage4Buf.setSize(4, numSamples, false, true, true);
        dryAlignedBuf.setSize(2, numSamples, false, true, true);
        driveGainBuf.setSize(1, numSamples, false, true, true);
    }

    // Handle live Oversampling Mode change (OFF / 2x / 4x / 8x / 16x) safely without RT allocations
    const int reqOsIdx = juce::jlimit(0, 4, static_cast<int>(std::round(pOversampling->load())));
    if (reqOsIdx != activeOsIdx)
    {
        activeOsIdx = reqOsIdx;
        if (activeOsIdx >= 1)
        {
            osSatBank[static_cast<size_t>(activeOsIdx - 1)]->reset();
            osFoldBank[static_cast<size_t>(activeOsIdx - 1)]->reset();
        }
        const double factor = (activeOsIdx == 0) ? 1.0 : static_cast<double>(1 << activeOsIdx);
        waveshaper.prepareForRate(hostFs * factor);
        driveEnv.prepareForRate(hostFs * factor);

        exactTotalLatency = computeTotalLatencyForOs(activeOsIdx);
        const int newLat = static_cast<int>(std::round(exactTotalLatency));
        latencyAtomic.store(newLat, std::memory_order_relaxed);
        pendingLatencyAtomic.store(newLat, std::memory_order_relaxed);
        triggerAsyncUpdate();
    }

    const auto engineMode = static_cast<sylredhel::origami::EngineMode>(static_cast<int>(std::round(pEngineMode->load())));
    const auto phaseMode  = static_cast<sylredhel::origami::PhaseMode>(static_cast<int>(std::round(pPhaseMode->load())));
    const auto satCurve   = static_cast<sylredhel::origami::SatCurve>(static_cast<int>(std::round(pSatCurve->load())));
    const auto foldShape  = static_cast<sylredhel::origami::FoldShape>(static_cast<int>(std::round(pFoldShape->load())));
    const auto audition   = static_cast<sylredhel::origami::AuditionMode>(static_cast<int>(std::round(pAudition->load())));
    const bool autoGain   = pAutoGain->load() > 0.5f;
    const bool bypassed   = pBypass->load() > 0.5f;

    const double driveDb   = pDriveDb->load();
    const double ceilDb    = pCeilingDb->load();
    const double envAmt    = pEnvFollow->load() * 0.01;
    // When Env Follow is high, average loudness boost is gentler, so scale static Auto Gain smoothly
    const double autoTrim  = autoGain ? (-0.55 * driveDb * (1.0 - 0.35 * envAmt)) : 0.0;
    const double outDb     = pOutGainDb->load() + autoTrim;
    const double ceilLin   = std::pow(10.0, ceilDb * 0.05);

    smDriveDb.setTargetValue(driveDb);
    smDriveLin.setTargetValue(std::pow(10.0, driveDb * 0.05));
    smCeilingSat.setTargetValue(ceilLin);
    smCeilingFold.setTargetValue(ceilLin);
    smDetailFreq.setTargetValue(pDetailFreq->load());
    smDetailPreserve.setTargetValue(pDetailPreserve->load() * 0.01);
    smFoldDepth.setTargetValue(pFoldDepth->load() * 0.01);
    smEnvFollow.setTargetValue(envAmt);
    smEnvReleaseMs.setTargetValue(pEnvReleaseMs->load());
    smMix.setTargetValue(pMix->load() * 0.01);
    smOutGainLin.setTargetValue(std::pow(10.0, outDb * 0.05));

    firXover.setCutoffHz(pDetailFreq->load());

    const bool isMono = (numInCh == 1);
    float* ioL = buffer.getWritePointer(0);
    float* ioR = isMono ? nullptr : buffer.getWritePointer(1);

    float* dryL = dryAlignedBuf.getWritePointer(0);
    float* dryR = dryAlignedBuf.getWritePointer(1);
    float* drvTrack = driveGainBuf.getWritePointer(0);

    // 1. Exact Latency Alignment for Dry Parallel Mix & Click-Free Bypass
    for (int i = 0; i < numSamples; ++i)
    {
        const double inL = static_cast<double>(ioL[i]);
        const double inR = isMono ? inL : static_cast<double>(ioR[i]);
        double aL = 0.0, aR = 0.0;
        dryAlignDelay.pushAndRead(inL, inR, exactTotalLatency, aL, aR);
        dryL[i] = static_cast<float>(aL);
        dryR[i] = static_cast<float>(aR);
    }

    if (bypassed)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            ioL[i] = dryL[i];
            if (!isMono) ioR[i] = dryR[i];
            const float m = 0.5f * (dryL[i] + dryR[i]);
            waveScope.pushPair(m, m);
        }
        foldActivityDbAtomic.store(0.0f, std::memory_order_relaxed);
        detailSavedAtomic.store(0.0f, std::memory_order_relaxed);
        liveDriveDbAtomic.store(0.0f, std::memory_order_relaxed);
        return;
    }

    float* s0 = stage4Buf.getWritePointer(0);
    float* s1 = stage4Buf.getWritePointer(1);
    float* s2 = stage4Buf.getWritePointer(2);
    float* s3 = stage4Buf.getWritePointer(3);

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = ioL[i];
        const float inR = isMono ? inL : ioR[i];
        s0[i] = inL;
        s1[i] = inR;
        s2[i] = 0.0f;
        s3[i] = 0.0f;
    }

    // -------------------------------------------------------------------------
    // STAGE 1: Envelope-Followed Dynamic Drive + Saturation S(x) + Lost Delta
    // -------------------------------------------------------------------------
    double blockEffDriveSum = 0.0;
    size_t blockEffDriveCnt = 0;

    auto runStage1Loop = [&](float* b0, float* b1, float* b2, float* b3, size_t count, size_t stepDiv)
    {
        for (size_t k = 0; k < count; ++k)
        {
            const bool hostTick = (k % stepDiv == 0);
            const double dDb  = hostTick ? smDriveDb.getNextValue()      : smDriveDb.getCurrentValue();
            const double dLin = hostTick ? smDriveLin.getNextValue()     : smDriveLin.getCurrentValue();
            const double C    = hostTick ? smCeilingSat.getNextValue()   : smCeilingSat.getCurrentValue();
            const double eAmt = hostTick ? smEnvFollow.getNextValue()    : smEnvFollow.getCurrentValue();
            const double eRel = hostTick ? smEnvReleaseMs.getNextValue() : smEnvReleaseMs.getCurrentValue();

            if (hostTick)
                driveEnv.setReleaseMs(eRel);

            const double rawL = static_cast<double>(b0[k]);
            const double rawR = static_cast<double>(b1[k]);

            double effDrvDb = dDb;
            const double effDrvLin = driveEnv.processAndGetDriveLin(rawL, rawR, dDb, dLin, C, eAmt, effDrvDb);

            if (hostTick)
            {
                const size_t hostIdx = k / stepDiv;
                if (hostIdx < static_cast<size_t>(numSamples))
                    drvTrack[hostIdx] = static_cast<float>(effDrvLin);
                blockEffDriveSum += effDrvDb;
                ++blockEffDriveCnt;
            }

            const double xL = rawL * effDrvLin;
            const double xR = rawR * effDrvLin;
            const double satL = waveshaper.processSaturation(xL, C, satCurve, 0);
            const double satR = waveshaper.processSaturation(xR, C, satCurve, 1);

            b0[k] = static_cast<float>(satL);
            b1[k] = static_cast<float>(satR);
            b2[k] = static_cast<float>(xL - satL);
            b3[k] = static_cast<float>(xR - satR);
        }
    };

    auto block4 = juce::dsp::AudioBlock<float>(stage4Buf).getSubBlock(0, static_cast<size_t>(numSamples));

    if (activeOsIdx == 0)
    {
        runStage1Loop(s0, s1, s2, s3, static_cast<size_t>(numSamples), 1);
    }
    else
    {
        auto& os1 = *osSatBank[static_cast<size_t>(activeOsIdx - 1)];
        auto osBlock1 = os1.processSamplesUp(block4);
        runStage1Loop(osBlock1.getChannelPointer(0), osBlock1.getChannelPointer(1),
                      osBlock1.getChannelPointer(2), osBlock1.getChannelPointer(3),
                      osBlock1.getNumSamples(), os1.getOversamplingFactor());
        os1.processSamplesDown(block4);
    }

    // -------------------------------------------------------------------------
    // STAGE 2 (Host Rate, exact K latency): Extract Lost High-Freq Detail dHF
    // -------------------------------------------------------------------------
    double maxSavedDetail = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const double satL = static_cast<double>(s0[i]);
        const double satR = static_cast<double>(s1[i]);
        const double delL = static_cast<double>(s2[i]);
        const double delR = static_cast<double>(s3[i]);
        const double fc   = smDetailFreq.getNextValue();
        const double prs  = smDetailPreserve.getNextValue();

        double satAlL = 0.0, satAlR = 0.0;
        satAlignDelay.pushAndRead(satL, satR, static_cast<double>(firLatencyK), satAlL, satAlR);

        double dummyLoL = 0.0, dummyLoR = 0.0, dhfL = 0.0, dhfR = 0.0;
        if (phaseMode == sylredhel::origami::PhaseMode::LinearPhase)
        {
            firXover.processStereoSample(delL, delR, dummyLoL, dummyLoR, dhfL, dhfR);
        }
        else
        {
            double preL = 0.0, preR = 0.0;
            minPhasePreDelay.pushAndRead(delL, delR, static_cast<double>(firLatencyK), preL, preR);
            lr4Xover.processStereoSample(preL, preR, fc, dummyLoL, dummyLoR, dhfL, dhfR);
        }

        const double scaledDhfL = prs * dhfL;
        const double scaledDhfR = prs * dhfR;
        maxSavedDetail = std::max(maxSavedDetail, 0.5 * (std::abs(scaledDhfL) + std::abs(scaledDhfR)));

        s0[i] = static_cast<float>(satAlL);
        s1[i] = static_cast<float>(satAlR);
        s2[i] = static_cast<float>(scaledDhfL);
        s3[i] = static_cast<float>(scaledDhfR);
    }

    // -------------------------------------------------------------------------
    // STAGE 3: Origami Detail Reconstruction (NEST / FOLD / DPC) inside [-C, +C]
    // -------------------------------------------------------------------------
    auto runStage3Loop = [&](float* b0, float* b1, float* b2, float* b3, size_t count, size_t stepDiv)
    {
        for (size_t k = 0; k < count; ++k)
        {
            const double C     = (k % stepDiv == 0) ? smCeilingFold.getNextValue() : smCeilingFold.getCurrentValue();
            const double depth = (k % stepDiv == 0) ? smFoldDepth.getNextValue()   : smFoldDepth.getCurrentValue();

            const double satL = static_cast<double>(b0[k]);
            const double satR = static_cast<double>(b1[k]);
            const double dL   = static_cast<double>(b2[k]);
            const double dR   = static_cast<double>(b3[k]);

            const double outL = waveshaper.processLostDetailReconstruct(satL, dL, C, engineMode, foldShape, depth, 0);
            const double outR = waveshaper.processLostDetailReconstruct(satR, dR, C, engineMode, foldShape, depth, 1);

            b0[k] = static_cast<float>(outL);
            b1[k] = static_cast<float>(outR);
            b2[k] = static_cast<float>(sylredhel::origami::OrigamiWaveshaper::evalSoftClamp(satL, C));
            b3[k] = static_cast<float>(sylredhel::origami::OrigamiWaveshaper::evalSoftClamp(satR, C));
        }
    };

    if (activeOsIdx == 0)
    {
        runStage3Loop(s0, s1, s2, s3, static_cast<size_t>(numSamples), 1);
    }
    else
    {
        auto& os2 = *osFoldBank[static_cast<size_t>(activeOsIdx - 1)];
        auto osBlock2 = os2.processSamplesUp(block4);
        runStage3Loop(osBlock2.getChannelPointer(0), osBlock2.getChannelPointer(1),
                      osBlock2.getChannelPointer(2), osBlock2.getChannelPointer(3),
                      osBlock2.getNumSamples(), os2.getOversamplingFactor());
        os2.processSamplesDown(block4);
    }

    // 4. Final Audition Routing, Parallel Mix, Output Gain & Live Scope Telemetry
    const double curCeil = smCeilingFold.getTargetValue();
    double maxOvershootRatio = 1.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const double mix  = smMix.getNextValue();
        const double outG = smOutGainLin.getNextValue();

        // Latency-align dynamic drive multiplier so Scope's driven input matches output sample-for-sample!
        double alignedDrv = 1.0, dummyDrv = 1.0;
        driveTrackDelay.pushAndRead(static_cast<double>(drvTrack[i]), 0.0, exactTotalLatency, alignedDrv, dummyDrv);

        const double dL = static_cast<double>(dryL[i]);
        const double dR = static_cast<double>(dryR[i]);
        const double wL = static_cast<double>(s0[i]);
        const double wR = static_cast<double>(s1[i]);
        const double bL = static_cast<double>(s2[i]);
        const double bR = static_cast<double>(s3[i]);

        const double drvMono = 0.5 * (dL + dR) * alignedDrv;
        const double ratio = std::abs(drvMono) / std::max(curCeil, 0.05);
        if (ratio > maxOvershootRatio)
            maxOvershootRatio = ratio;

        double yL = 0.0, yR = 0.0;
        if (audition == sylredhel::origami::AuditionMode::FoldedDelta)
        {
            yL = wL - bL;
            yR = wR - bR;
        }
        else if (audition == sylredhel::origami::AuditionMode::BodyOnly)
        {
            yL = bL;
            yR = bR;
        }
        else
        {
            yL = (1.0 - mix) * dL + mix * wL;
            yR = (1.0 - mix) * dR + mix * wR;
        }

        ioL[i] = static_cast<float>(yL * outG);
        if (!isMono)
            ioR[i] = static_cast<float>(yR * outG);

        waveScope.pushPair(static_cast<float>(drvMono), static_cast<float>(0.5 * (wL + wR)));
    }

    const float foldDb = static_cast<float>(20.0 * std::log10(std::max(maxOvershootRatio, 1.0)));
    const float prevDb = foldActivityDbAtomic.load(std::memory_order_relaxed);
    foldActivityDbAtomic.store((foldDb > prevDb) ? foldDb : (prevDb * 0.87f + foldDb * 0.13f),
                               std::memory_order_relaxed);

    const float savedNorm = static_cast<float>(std::clamp(maxSavedDetail / std::max(curCeil, 0.05) * 2.5, 0.0, 1.0));
    const float prevSv = detailSavedAtomic.load(std::memory_order_relaxed);
    detailSavedAtomic.store((savedNorm > prevSv) ? savedNorm : (prevSv * 0.88f + savedNorm * 0.12f),
                            std::memory_order_relaxed);

    if (blockEffDriveCnt > 0)
    {
        const float avgEffDb = static_cast<float>(blockEffDriveSum / static_cast<double>(blockEffDriveCnt));
        const float prevLive = liveDriveDbAtomic.load(std::memory_order_relaxed);
        liveDriveDbAtomic.store(prevLive * 0.65f + avgEffDb * 0.35f, std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* OrigamiFairyAudioProcessor::createEditor()
{
    return new OrigamiFairyAudioProcessorEditor(*this);
}

void OrigamiFairyAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void OrigamiFairyAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OrigamiFairyAudioProcessor();
}
