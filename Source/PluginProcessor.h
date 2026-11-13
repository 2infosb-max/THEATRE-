#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "HallModel.h"
#include <atomic>
#include <array>

// Theatre — a zero-latency analysis plugin. Audio passes through untouched;
// the audio thread only measures per-channel levels and K-weighted loudness.
class TheatreProcessor : public juce::AudioProcessor
{
public:
    TheatreProcessor();
    ~TheatreProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return false; }
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

    // ---- metering (written on the audio thread, read by the editor) ----
    struct ChannelMeter
    {
        std::atomic<float> peak { 0.0f };     // max |sample| since last read
        std::atomic<float> sumSq { 0.0f };    // sum of squares since last read
        std::atomic<int>   count { 0 };
    };
    std::array<ChannelMeter, hall::NumCh> meters;   // indexed by hall::Ch
    std::array<std::atomic<bool>, hall::NumCh> channelPresent {};

    // Bus format of the track the plugin is inserted on (hall::Format), -1 if unsupported
    std::atomic<int> busFormat { hall::FStereo };

    // 100 ms K-weighted energy blocks for loudness (single producer / single consumer)
    juce::AbstractFifo loudFifo { 1024 };
    std::array<float, 1024> loudBuf {};
    int readLoudnessBlocks (float* dest, int maxNum);

    // Editor-only UI state persisted with the session
    juce::ValueTree uiState { "UI" };

private:
    void updateChannelMap();
    void setupKWeighting (double sr);

    std::array<int, 16> chMap {};   // bus channel index -> hall::Ch (-1 = ignored)
    int numMapped = 0;

    struct Biquad { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    Biquad shelf, highpass;
    struct KState { double x1 = 0, x2 = 0, y1 = 0, y2 = 0, u1 = 0, u2 = 0, v1 = 0, v2 = 0; };
    std::array<KState, 16> kState {};
    std::array<double, 16> kAcc {};
    int blockLen = 4800, blockPos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TheatreProcessor)
};
