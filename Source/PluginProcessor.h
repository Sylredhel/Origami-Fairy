#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/LinearPhaseFirXover.h"
#include "DSP/TptLr4Xover.h"
#include "DSP/OrigamiWaveshaper.h"
#include <array>
#include <atomic>
#include <memory>

template <int Capacity>
class LockFreeWaveScope
{
public:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");

    void reset() noexcept
    {
        inBuf.fill(0.0f);
        outBuf.fill(0.0f);
        writePos.store(0, std::memory_order_relaxed);
    }

    inline void pushPair(float drivenIn, float foldedOut) noexcept
    {
        const int idx = writePos.load(std::memory_order_relaxed);
        inBuf[static_cast<size_t>(idx)]  = drivenIn;
        outBuf[static_cast<size_t>(idx)] = foldedOut;
        writePos.store((idx + 1) & (Capacity - 1), std::memory_order_release);
    }

    void copyLatest(float* destIn, float* destOut, int count) const noexcept
    {
        const int end = writePos.load(std::memory_order_acquire);
        int pos = (end - count) & (Capacity - 1);
        for (int i = 0; i < count; ++i)
        {
            destIn[i]  = inBuf[static_cast<size_t>(pos)];
            destOut[i] = outBuf[static_cast<size_t>(pos)];
            pos = (pos + 1) & (Capacity - 1);
        }
    }

private:
    std::array<float, Capacity> inBuf {};
    std::array<float, Capacity> outBuf {};
    std::atomic<int> writePos { 0 };
};

class OrigamiFairyAudioProcessor : public juce::AudioProcessor,
                                   private juce::AsyncUpdater
{
public:
    static constexpr int kScopeCapacity = 4096;

    OrigamiFairyAudioProcessor();
    ~OrigamiFairyAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
   #endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Lock-free telemetry for UI oscilloscope, live dynamic drive indicator & mascot
    double getCurrentSampleRate() const noexcept    { return currentFsAtomic.load(std::memory_order_relaxed); }
    int    getEngineLatencySamples() const noexcept { return latencyAtomic.load(std::memory_order_relaxed); }
    float  getFoldActivityDb() const noexcept       { return foldActivityDbAtomic.load(std::memory_order_relaxed); }
    float  getDetailSavedLevel() const noexcept     { return detailSavedAtomic.load(std::memory_order_relaxed); }
    float  getLiveDriveDb() const noexcept          { return liveDriveDbAtomic.load(std::memory_order_relaxed); }
    const LockFreeWaveScope<kScopeCapacity>& getWaveScope() const noexcept { return waveScope; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void handleAsyncUpdate() override;
    double computeTotalLatencyForOs(int osIdx) const noexcept;

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 4> osSatBank;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 4> osFoldBank;

    sylredhel::origami::LinearPhaseFirXover firXover;
    sylredhel::origami::TptLr4Xover         lr4Xover;
    sylredhel::origami::OrigamiWaveshaper   waveshaper;
    sylredhel::origami::DriveEnvFollower    driveEnv;

    sylredhel::origami::FractionalDelayLine dryAlignDelay;
    sylredhel::origami::FractionalDelayLine minPhasePreDelay;
    sylredhel::origami::FractionalDelayLine satAlignDelay;
    sylredhel::origami::FractionalDelayLine driveTrackDelay;

    juce::AudioBuffer<float> stage4Buf;
    juce::AudioBuffer<float> dryAlignedBuf;
    juce::AudioBuffer<float> driveGainBuf;

    // Smoothed continuous parameters
    juce::SmoothedValue<double> smDriveDb, smDriveLin, smCeilingSat, smCeilingFold;
    juce::SmoothedValue<double> smDetailFreq, smDetailPreserve, smFoldDepth;
    juce::SmoothedValue<double> smEnvFollow, smEnvReleaseMs;
    juce::SmoothedValue<double> smMix, smOutGainLin;

    // Cached atomic parameter pointers
    std::atomic<float>* pEngineMode     = nullptr;
    std::atomic<float>* pPhaseMode      = nullptr;
    std::atomic<float>* pSatCurve       = nullptr;
    std::atomic<float>* pFoldShape      = nullptr;
    std::atomic<float>* pOversampling   = nullptr;
    std::atomic<float>* pDriveDb        = nullptr;
    std::atomic<float>* pCeilingDb      = nullptr;
    std::atomic<float>* pDetailFreq     = nullptr;
    std::atomic<float>* pDetailPreserve = nullptr;
    std::atomic<float>* pFoldDepth      = nullptr;
    std::atomic<float>* pEnvFollow      = nullptr;
    std::atomic<float>* pEnvReleaseMs   = nullptr;
    std::atomic<float>* pMix            = nullptr;
    std::atomic<float>* pOutGainDb      = nullptr;
    std::atomic<float>* pAutoGain       = nullptr;
    std::atomic<float>* pAudition       = nullptr;
    std::atomic<float>* pBypass         = nullptr;

    double hostFs            = 48000.0;
    double exactTotalLatency = 310.0;
    int    firLatencyK       = 256;
    int    activeOsIdx       = 3;

    std::atomic<double> currentFsAtomic      { 48000.0 };
    std::atomic<int>    latencyAtomic        { 310 };
    std::atomic<int>    pendingLatencyAtomic { 310 };
    std::atomic<float>  foldActivityDbAtomic { 0.0f };
    std::atomic<float>  detailSavedAtomic    { 0.0f };
    std::atomic<float>  liveDriveDbAtomic    { 4.0f };
    LockFreeWaveScope<kScopeCapacity> waveScope;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrigamiFairyAudioProcessor)
};
