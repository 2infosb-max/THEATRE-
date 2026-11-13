#include "PluginProcessor.h"
#include "PluginEditor.h"

TheatreProcessor::TheatreProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    chMap.fill (-1);
    for (auto& p : channelPresent) p.store (false);
    updateChannelMap();
}

juce::AudioProcessorValueTreeState::ParameterLayout TheatreProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "x", 1 }, "Left-Right",
                    NormalisableRange<float> (-1.0f, 1.0f, 0.001f), -0.35f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "y", 1 }, "Screen-Rear",
                    NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.55f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "z", 1 }, "Height",
                    NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.30f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "size", 1 }, "Size",
                    NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.30f));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Render as",
                    StringArray { "Bed", "Object" }, 1));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "monitor", 1 }, "Monitoring",
                    StringArray { "7.1.4", "7.1", "5.1", "Stereo" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "source", 1 }, "Source",
                    StringArray { "Built-in panner", "Track signal" }, 0));
    return { p.begin(), p.end() };
}

static bool isSupportedSet (const juce::AudioChannelSet& s)
{
    return s == juce::AudioChannelSet::stereo()
        || s == juce::AudioChannelSet::create5point1()
        || s == juce::AudioChannelSet::create7point1()
        || s == juce::AudioChannelSet::create7point1point4();
}

bool TheatreProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return in == out && isSupportedSet (out);
}

void TheatreProcessor::updateChannelMap()
{
    using CT = juce::AudioChannelSet::ChannelType;
    const auto set = getChannelLayoutOfBus (true, 0);
    chMap.fill (-1);
    for (auto& p : channelPresent) p.store (false);

    const int n = std::min (set.size(), (int) chMap.size());
    for (int i = 0; i < n; ++i)
    {
        int c = -1;
        switch (set.getTypeOfChannel (i))
        {
            case CT::left:              c = hall::L; break;
            case CT::right:             c = hall::R; break;
            case CT::centre:            c = hall::C; break;
            case CT::LFE:               c = hall::LFE; break;
            case CT::leftSurround:
            case CT::leftSurroundSide:  c = hall::Ls; break;
            case CT::rightSurround:
            case CT::rightSurroundSide: c = hall::Rs; break;
            case CT::leftSurroundRear:  c = hall::Lrs; break;
            case CT::rightSurroundRear: c = hall::Rrs; break;
            case CT::topFrontLeft:      c = hall::Ltf; break;
            case CT::topFrontRight:     c = hall::Rtf; break;
            case CT::topRearLeft:       c = hall::Ltr; break;
            case CT::topRearRight:      c = hall::Rtr; break;
            default: break;
        }
        chMap[(size_t) i] = c;
        if (c >= 0) channelPresent[(size_t) c].store (true);
    }
    numMapped = n;

    int f = hall::FStereo;
    if (set == juce::AudioChannelSet::create7point1point4()) f = hall::F714;
    else if (set == juce::AudioChannelSet::create7point1())  f = hall::F71;
    else if (set == juce::AudioChannelSet::create5point1())  f = hall::F51;
    busFormat.store (f);
}

void TheatreProcessor::setupKWeighting (double fs)
{
    // ITU-R BS.1770 K-weighting, coefficients derived for any sample rate
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs);
        const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        shelf.b0 = (Vh + Vb * K / Q + K * K) / a0;
        shelf.b1 = 2.0 * (K * K - Vh) / a0;
        shelf.b2 = (Vh - Vb * K / Q + K * K) / a0;
        shelf.a1 = 2.0 * (K * K - 1.0) / a0;
        shelf.a2 = (1.0 - K / Q + K * K) / a0;
    }
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs);
        const double a0 = 1.0 + K / Q + K * K;
        highpass.b0 = 1.0; highpass.b1 = -2.0; highpass.b2 = 1.0;
        highpass.a1 = 2.0 * (K * K - 1.0) / a0;
        highpass.a2 = (1.0 - K / Q + K * K) / a0;
    }
}

void TheatreProcessor::prepareToPlay (double sampleRate, int)
{
    setLatencySamples (0);
    setupKWeighting (sampleRate);
    for (auto& k : kState) k = {};
    kAcc.fill (0.0);
    blockLen = std::max (1, (int) std::round (sampleRate * 0.1));
    blockPos = 0;
    updateChannelMap();
}

void TheatreProcessor::processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) {}

void TheatreProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    // Pass-through: the audio is never modified. Only measurement happens here.
    const int numCh = std::min (buffer.getNumChannels(), numMapped);
    const int numSamples = buffer.getNumSamples();

    // Level meters
    for (int i = 0; i < numCh; ++i)
    {
        const int c = chMap[(size_t) i];
        if (c < 0) continue;
        const float* d = buffer.getReadPointer (i);
        float pk = 0.0f, ss = 0.0f;
        for (int n = 0; n < numSamples; ++n)
        {
            const float v = d[n];
            pk = std::max (pk, std::abs (v));
            ss += v * v;
        }
        auto& m = meters[(size_t) c];
        float prev = m.peak.load();
        while (pk > prev && ! m.peak.compare_exchange_weak (prev, pk)) {}
        float s = m.sumSq.load();
        while (! m.sumSq.compare_exchange_weak (s, s + ss)) {}
        m.count.fetch_add (numSamples);
    }

    // K-weighted loudness, in 100 ms blocks
    int pos = 0;
    while (pos < numSamples)
    {
        const int len = std::min (numSamples - pos, blockLen - blockPos);
        for (int i = 0; i < numCh; ++i)
        {
            const int c = chMap[(size_t) i];
            if (c < 0 || c == hall::LFE) continue;
            const float* d = buffer.getReadPointer (i) + pos;
            auto& st = kState[(size_t) i];
            double acc = 0.0;
            for (int n = 0; n < len; ++n)
            {
                const double x = d[n];
                const double y = shelf.b0 * x + shelf.b1 * st.x1 + shelf.b2 * st.x2 - shelf.a1 * st.y1 - shelf.a2 * st.y2;
                st.x2 = st.x1; st.x1 = x; st.y2 = st.y1; st.y1 = y;
                const double z = y - 2.0 * st.u1 + st.u2 - highpass.a1 * st.v1 - highpass.a2 * st.v2;
                st.u2 = st.u1; st.u1 = y; st.v2 = st.v1; st.v1 = z;
                acc += z * z;
            }
            kAcc[(size_t) i] += acc;
        }
        blockPos += len;
        pos += len;
        if (blockPos >= blockLen)
        {
            double e = 0.0;
            for (int i = 0; i < numCh; ++i)
            {
                const int c = chMap[(size_t) i];
                if (c < 0 || c == hall::LFE) continue;
                const double w = (c == hall::Ls || c == hall::Rs || c == hall::Lrs || c == hall::Rrs) ? 1.41 : 1.0;
                e += w * kAcc[(size_t) i] / (double) blockLen;
                kAcc[(size_t) i] = 0.0;
            }
            int s1, z1, s2, z2;
            loudFifo.prepareToWrite (1, s1, z1, s2, z2);
            if (z1 > 0) loudBuf[(size_t) s1] = (float) e;
            else if (z2 > 0) loudBuf[(size_t) s2] = (float) e;
            loudFifo.finishedWrite (z1 + z2);
            blockPos = 0;
        }
    }
}

int TheatreProcessor::readLoudnessBlocks (float* dest, int maxNum)
{
    int s1, z1, s2, z2;
    loudFifo.prepareToRead (maxNum, s1, z1, s2, z2);
    for (int i = 0; i < z1; ++i) dest[i] = loudBuf[(size_t) (s1 + i)];
    for (int i = 0; i < z2; ++i) dest[z1 + i] = loudBuf[(size_t) (s2 + i)];
    loudFifo.finishedRead (z1 + z2);
    return z1 + z2;
}

juce::AudioProcessorEditor* TheatreProcessor::createEditor()
{
    return new TheatreEditor (*this);
}

void TheatreProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("UI"), nullptr);
    state.appendChild (uiState.createCopy(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TheatreProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (tree.hasType (apvts.state.getType()))
        {
            auto ui = tree.getChildWithName ("UI");
            if (ui.isValid())
            {
                uiState.copyPropertiesFrom (ui, nullptr);
                tree.removeChild (ui, nullptr);
            }
            apvts.replaceState (tree);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TheatreProcessor();
}
