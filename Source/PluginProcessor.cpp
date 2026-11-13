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
                    StringArray { "Built-in panner", "Track signal" }, 1));
    return { p.begin(), p.end() };
}

// Map one channel type to the hall channel it feeds (-1 = not metered)
static int mapType (juce::AudioChannelSet::ChannelType t)
{
    using CT = juce::AudioChannelSet::ChannelType;
    switch (t)
    {
        case CT::left:              return hall::L;
        case CT::right:             return hall::R;
        case CT::centre:            return hall::C;
        case CT::LFE:
        case CT::LFE2:              return hall::LFE;
        case CT::leftSurround:
        case CT::leftSurroundSide:  return hall::Ls;
        case CT::rightSurround:
        case CT::rightSurroundSide: return hall::Rs;
        case CT::leftSurroundRear:  return hall::Lrs;
        case CT::rightSurroundRear: return hall::Rrs;
        case CT::topFrontLeft:
        case CT::topSideLeft:       return hall::Ltf;
        case CT::topFrontRight:
        case CT::topSideRight:      return hall::Rtf;
        case CT::topRearLeft:       return hall::Ltr;
        case CT::topRearRight:      return hall::Rtr;
        default:                    return -1;
    }
}

// Unlabelled ("discrete") tracks: read the channels in the standard film / SMPTE order
int TheatreProcessor::mapDiscrete (int n, int i)
{
    using namespace hall;
    static const int o1[]  = { C };
    static const int o2[]  = { L, R };
    static const int o6[]  = { L, R, C, LFE, Ls, Rs };
    static const int o8[]  = { L, R, C, LFE, Ls, Rs, Lrs, Rrs };
    static const int o10[] = { L, R, C, LFE, Ls, Rs, Ltf, Rtf, Ltr, Rtr };            // 5.1.4
    static const int o12[] = { L, R, C, LFE, Ls, Rs, Lrs, Rrs, Ltf, Rtf, Ltr, Rtr };   // 7.1.4
    if (i < 0 || i >= n) return -1;
    switch (n)
    {
        case 1:  return o1[i];
        case 2:  return o2[i];
        case 6:  return o6[i];
        case 8:  return o8[i];
        case 10: return o10[i];
        case 12: return o12[i];
        default: return -1;
    }
}

static bool layoutUsable (const juce::AudioChannelSet& s)
{
    const int n = s.size();
    if (n < 1 || n > 12) return false;
    if (s.isDiscreteLayout())
        return n == 1 || n == 2 || n == 6 || n == 8 || n == 10 || n == 12;
    std::array<bool, hall::NumCh> used {};
    for (int i = 0; i < n; ++i)
    {
        const int c = mapType (s.getTypeOfChannel (i));
        if (c < 0 || used[(size_t) c]) return false;   // unknown or doubled channel: refuse rather than mis-meter
        used[(size_t) c] = true;
    }
    return true;
}

bool TheatreProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return in == out && layoutUsable (out);
}

void TheatreProcessor::updateChannelMap()
{
    const auto set = getChannelLayoutOfBus (true, 0);
    std::array<int, maxCh> map;
    map.fill (-1);
    std::array<bool, hall::NumCh> present {};
    const int n = std::min (set.size(), maxCh);
    const bool discrete = set.isDiscreteLayout();
    for (int i = 0; i < n; ++i)
    {
        const int c = discrete ? mapDiscrete (n, i) : mapType (set.getTypeOfChannel (i));
        if (c >= 0 && ! present[(size_t) c]) { map[(size_t) i] = c; present[(size_t) c] = true; }
    }

    int f = hall::FStereo;
    if (present[hall::Ltf] || present[hall::Ltr]) f = hall::F714;
    else if (present[hall::Lrs])                  f = hall::F71;
    else if (present[hall::Ls] || present[hall::LFE]) f = hall::F51;

    juce::String desc;
    juce::StringArray names;
    for (int i = 0; i < n; ++i) if (map[(size_t) i] >= 0) names.add (hall::chName (map[(size_t) i]));
    desc << n << " ch " << (discrete ? "(unlabelled, read as " : "(") << names.joinIntoString (" ") << ")";

    {
        const juce::SpinLock::ScopedLockType sl (layoutLock);
        chMap = map;
        numMapped = n;
        layoutName = desc;
    }
    for (int c = 0; c < hall::NumCh; ++c) channelPresent[(size_t) c].store (present[(size_t) c]);
    busFormat.store (f);
}

juce::String TheatreProcessor::getLayoutDescription() const
{
    const juce::SpinLock::ScopedLockType sl (layoutLock);
    return layoutName;
}

static double besselI0 (double x)
{
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 50; ++k)
    {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
        if (term < 1.0e-12 * sum) break;
    }
    return sum;
}

void TheatreProcessor::setupTruePeak (double fs)
{
    // Windowed-sinc polyphase interpolator: 4x below 96 kHz, 2x at 96 kHz and above (BS.1770-4 Annex 2)
    tpPhases = fs < 96000.0 ? 4 : 2;
    const int N = tpPhases * tpTaps;
    const double centre = (N - 1) * 0.5, beta = 7.0;
    std::vector<double> h ((size_t) N);
    for (int i = 0; i < N; ++i)
    {
        const double t = ((double) i - centre) / (double) tpPhases;
        const double sinc = std::abs (t) < 1.0e-12 ? 1.0 : std::sin (juce::MathConstants<double>::pi * t) / (juce::MathConstants<double>::pi * t);
        const double r = ((double) i - centre) / centre;
        const double w = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / besselI0 (beta);
        h[(size_t) i] = sinc * w;
    }
    tpCoef.fill (0.0f);
    for (int p = 0; p < tpPhases; ++p)
    {
        double sum = 0.0;
        for (int k = 0; k < tpTaps; ++k) sum += h[(size_t) (p + k * tpPhases)];
        for (int k = 0; k < tpTaps; ++k)
            tpCoef[(size_t) (p * tpTaps + k)] = (float) (h[(size_t) (p + k * tpPhases)] / sum);
    }
    for (auto& hh : tpHist) hh.fill (0.0f);
    tpPos.fill (0);
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
    setupTruePeak (sampleRate);
    for (auto& k : kState) k = {};
    kAcc.fill (0.0);
    blockLen = std::max (1, (int) std::round (sampleRate * 0.1));
    blockPos = 0;
    updateChannelMap();
}

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
        float pk = 0.0f, ss = 0.0f, tp = 0.0f;
        auto& hist = tpHist[(size_t) i];
        int hp = tpPos[(size_t) i];
        for (int n = 0; n < numSamples; ++n)
        {
            const float v = d[n];
            pk = std::max (pk, std::abs (v));
            ss += v * v;
            hp = (hp + 1) % tpTaps;
            hist[(size_t) hp] = v;
            for (int ph = 0; ph < tpPhases; ++ph)
            {
                const float* co = tpCoef.data() + ph * tpTaps;
                float y = 0.0f;
                int idx = hp;
                for (int k = 0; k < tpTaps; ++k)
                {
                    y += co[k] * hist[(size_t) idx];
                    idx = idx == 0 ? tpTaps - 1 : idx - 1;
                }
                tp = std::max (tp, std::abs (y));
            }
        }
        tpPos[(size_t) i] = hp;
        tp = std::max (tp, pk);   // never report less than the sample peak
        auto& m = meters[(size_t) c];
        float prev = m.peak.load();
        while (pk > prev && ! m.peak.compare_exchange_weak (prev, pk)) {}
        float prevTp = m.truePeak.load();
        while (tp > prevTp && ! m.truePeak.compare_exchange_weak (prevTp, tp)) {}
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
