// Headless test harness for Theatre (not shipped). Checks pass-through, metering,
// loudness, and renders the editor to PNG files for a visual check.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
#include <sstream>
#define private public
#define protected public
#include "../Source/PluginEditor.h"
#undef private
#undef protected
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>

using namespace juce;
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

static int failures = 0;
static void check (bool ok, const String& what)
{
    std::cout << (ok ? "PASS  " : "FAIL  ") << what << std::endl;
    if (! ok) ++failures;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const File outDir (argc > 1 ? String (argv[1]) : File::getCurrentWorkingDirectory().getFullPathName());

    std::unique_ptr<TheatreProcessor> p (static_cast<TheatreProcessor*> (createPluginFilter()));

    // Layouts
    AudioProcessor::BusesLayout l714;
    l714.inputBuses.add (AudioChannelSet::create7point1point4());
    l714.outputBuses.add (AudioChannelSet::create7point1point4());
    check (p->checkBusesLayoutSupported (l714), "7.1.4 layout supported");
    AudioProcessor::BusesLayout bad;
    bad.inputBuses.add (AudioChannelSet::mono());
    bad.outputBuses.add (AudioChannelSet::stereo());
    check (! p->checkBusesLayoutSupported (bad), "mono->stereo rejected");
    check (p->setBusesLayout (l714), "set 7.1.4");
    check (p->busFormat.load() == hall::F714, "bus format detected as 7.1.4");

    const double sr = 48000.0;
    const int bs = 512;
    p->prepareToPlay (sr, bs);
    check (p->getLatencySamples() == 0, "zero latency reported");

    // Find bus indices for L and Ls
    const auto set = AudioChannelSet::create7point1point4();
    const int iL = set.getChannelIndexForType (AudioChannelSet::left);
    const int iLss = set.getChannelIndexForType (AudioChannelSet::leftSurroundSide);
    check (iL >= 0 && iLss >= 0, "found L and Lss channel indices");

    Random rng (1234);
    AudioBuffer<float> buf (12, bs), ref (12, bs);
    MidiBuffer midi;
    bool identical = true;
    for (int b = 0; b < (int) (sr * 3.0) / bs; ++b)   // 3 seconds
    {
        buf.clear();
        for (int n = 0; n < bs; ++n)
        {
            const double t = (double) (b * bs + n) / sr;
            buf.setSample (iL, n, (float) (0.5 * std::sin (2.0 * MathConstants<double>::pi * 1000.0 * t)));   // -6 dBFS peak
            buf.setSample (iLss, n, (rng.nextFloat() * 2.0f - 1.0f) * 0.1f);
        }
        ref.makeCopyOf (buf);
        p->processBlock (buf, midi);
        for (int c = 0; c < 12 && identical; ++c)
            for (int n = 0; n < bs; ++n)
                if (buf.getSample (c, n) != ref.getSample (c, n)) { identical = false; break; }
    }
    check (identical, "audio passes through bit-identical");

    const float pkL = p->meters[hall::L].peak.load();
    const float pkLs = p->meters[hall::Ls].peak.load();
    const float pkR = p->meters[hall::R].peak.load();
    check (std::abs (pkL - 0.5f) < 0.01f, "L peak ~0.5 (" + String (pkL) + ")");
    check (pkLs > 0.05f && pkLs <= 0.1f, "Ls peak in noise range (" + String (pkLs) + ")");
    check (pkR == 0.0f, "R silent");

    float blocks[64];
    const int nb = p->readLoudnessBlocks (blocks, 64);
    check (nb == (int) ((sr * 3.0) / bs) * bs / (int) (sr * 0.1), "one loudness block per 100 ms (" + String (nb) + ")");
    // A 1 kHz sine at 0.5 amplitude on one front channel: K-weighting ~ +0.69 dB at 1 kHz
    // => mean square 0.125 -> -9.03 dB, plus ~0.69 => ~-8.3 LUFS before the -0.691 offset... check plausibility
    double e = 0; for (int i = 10; i < nb; ++i) e += blocks[i]; e /= (nb - 10);
    const double lufs = -0.691 + 10.0 * std::log10 (e);
    std::cout << "      measured loudness " << lufs << " LUFS (L sine + Lss noise)" << std::endl;
    check (lufs > -12.0 && lufs < -7.0, "loudness plausible");

    // Pure 1 kHz sine, 0 dBFS peak... BS.1770 reference: -3.01 LUFS for a 0 dBFS 1 kHz sine on one channel
    {
        std::unique_ptr<TheatreProcessor> q (static_cast<TheatreProcessor*> (createPluginFilter()));
        q->prepareToPlay (sr, bs);
        AudioBuffer<float> sb (2, bs);
        for (int b = 0; b < (int) (sr * 2.0) / bs; ++b)
        {
            sb.clear();
            for (int n = 0; n < bs; ++n)
                sb.setSample (0, n, (float) std::sin (2.0 * MathConstants<double>::pi * 997.0 * (double) (b * bs + n) / sr));
            q->processBlock (sb, midi);
        }
        float bl[64]; const int k = q->readLoudnessBlocks (bl, 64);
        double ee = 0; for (int i = 5; i < k; ++i) ee += bl[i]; ee /= (k - 5);
        const double L = -0.691 + 10.0 * std::log10 (ee);
        std::cout << "      0 dBFS 997 Hz sine, one channel: " << L << " LUFS (reference -3.01)" << std::endl;
        check (std::abs (L + 3.01) < 0.1, "BS.1770 calibration within 0.1 LU");
    }

    // Model sanity
    {
        hall::Model m;
        check (m.seats.size() == 308, "308 seats");
        check (m.speakers.size() == 33, "33 speakers");
        auto g = m.panner (hall::F714, true, 0.0f, 0.0f, 0.0f, 0.0f);
        int loudest = 0;
        for (size_t i = 0; i < g.spk.size(); ++i) if (g.spk[i] > g.spk[(size_t) loudest]) loudest = (int) i;
        check (m.speakers[(size_t) loudest].id == "C", "object at screen centre -> C speaker loudest");
        auto gs = m.panner (hall::FStereo, false, -1.0f, 0.0f, 0.0f, 0.3f);
        bool onlyLR = true;
        for (size_t i = 0; i < gs.spk.size(); ++i)
            if (gs.spk[i] > 0 && m.speakers[i].id != "L" && m.speakers[i].id != "R") onlyLR = false;
        check (onlyLR, "stereo uses only L and R");
        auto g51 = m.panner (hall::F51, false, -1.0f, 1.0f, 0.0f, 0.2f);
        float lrs = 0; for (size_t i = 0; i < g51.spk.size(); ++i) if (m.speakers[i].ch == hall::Lrs) lrs += g51.spk[i];
        check (lrs > 0, "5.1: rear-left position plays on the Lrs speakers (joined to Ls)");
        auto g71 = m.panner (hall::F71, false, 0.0f, 0.3f, 1.0f, 0.3f);
        float tops = 0; for (size_t i = 0; i < g71.spk.size(); ++i) if (m.speakers[i].type == hall::SpkType::Top) tops += g71.spk[i];
        check (tops == 0, "7.1: ceiling speakers silent");
        auto cv = m.coverage (g, hall::panToMetres (0, 0, 0));
        check (std::abs (cv.rel[(size_t) m.mixSeat]) < 1.0e-3f, "mix seat is the 0 dB reference");
    }

    // Editor rendering
    {
        std::unique_ptr<TheatreEditor> ed (static_cast<TheatreEditor*> (p->createEditor()));
        ed->setSize (TheatreEditor::W, TheatreEditor::H);
        auto snap = [&] (const String& name)
        {
            for (int i = 0; i < 6; ++i) ed->timerCallback();
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            File f = outDir.getChildFile (name);
            f.deleteFile();
            FileOutputStream os (f);
            PNGImageFormat().writeImageToStream (img, os);
            std::cout << "      wrote " << f.getFullPathName() << std::endl;
        };
        ed->setTab (1);
        snap ("theatre_3d_714.png");
        ed->vs.show = 1;
        ed->hall3d.setCamera (1); ed->vs.yaw = ed->vs.tYaw; ed->vs.pitch = ed->vs.tPitch;
        snap ("theatre_3d_side_both.png");
        ed->vs.show = 0;
        ed->setChoice ("monitor", 2);
        ed->hall3d.setCamera (0); ed->vs.yaw = ed->vs.tYaw; ed->vs.pitch = ed->vs.tPitch;
        snap ("theatre_3d_51.png");
        ed->setChoice ("monitor", 0);
        ed->setChoice ("mode", 0);
        ed->setTab (0);
        ed->vs.show = 1;
        snap ("theatre_plan_bed.png");
        check (true, "editor rendered");
    }

    std::cout << (failures == 0 ? "ALL PASSED" : (String (failures) + " FAILED").toStdString()) << std::endl;
    return failures == 0 ? 0 : 1;
}
