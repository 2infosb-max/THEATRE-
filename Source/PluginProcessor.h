#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "HallModel.h"
#include <atomic>
#include <array>

// Theatre — a zero-latency analysis plugin. Audio passes through untouched;
// the audio thread only measures per-channel sample peak, true peak, RMS and K-weighted loudness.
class TheatreProcessor : public juce::AudioProcessor
{
public:
    static constexpr int maxCh = 16;

    TheatreProcessor();
    ~TheatreProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processorLayoutsChanged() override { updateChannelMap(); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Theatre"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // ---- metering (written on the audio thread, read and reset by the editor) ----
    struct ChannelMeter
    {
        std::atomic<float> peak { 0.0f };     // max |sample| since last read
        std::atomic<float> truePeak { 0.0f }; // max |4x oversampled| since last read
        std::atomic<float> sumSq { 0.0f };    // sum of squares since last read
        std::atomic<int>   count { 0 };
    };
    std::array<ChannelMeter, hall::NumCh> meters;   // indexed by hall::Ch
    std::array<std::atomic<bool>, hall::NumCh> channelPresent {};

    // Format implied by the track's channels (hall::Format)
    std::atomic<int> busFormat { hall::FStereo };
    juce::String getLayoutDescription() const;

    // 100 ms K-weighted energy blocks for loudness (single producer / single consumer)
    juce::AbstractFifo loudFifo { 1024 };
    std::array<float, 1024> loudBuf {};
    int readLoudnessBlocks (float* dest, int maxNum);

    // Editor-only UI state persisted with the session
    juce::ValueTree uiState { "UI" };

    // Exposed for tests
    static int mapDiscrete (int numChannels, int index);

private:
    void updateChannelMap();
    void setupKWeighting (double sr);
    void setupTruePeak (double sr);

    std::array<int, maxCh> chMap {};   // bus channel index -> hall::Ch (-1 = not metered)
    int numMapped = 0;
    juce::String layoutName;
    mutable juce::SpinLock layoutLock;

    struct Biquad { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    Biquad shelf, highpass;
    struct KState { double x1 = 0, x2 = 0, y1 = 0, y2 = 0, u1 = 0, u2 = 0, v1 = 0, v2 = 0; };
    std::array<KState, maxCh> kState {};
    std::array<double, maxCh> kAcc {};
    int blockLen = 4800, blockPos = 0;

    // True peak: polyphase interpolator (ITU-R BS.1770-4 Annex 2 method)
    static constexpr int tpTaps = 12;            // taps per phase
    int tpPhases = 4;                            // oversampling factor (4x below 96 kHz, 2x above)
    std::array<float, 4 * tpTaps> tpCoef {};     // [phase * tpTaps + k]
    std::array<std::array<float, tpTaps>, maxCh> tpHist {};
    std::array<int, maxCh> tpPos {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TheatreProcessor)
};
