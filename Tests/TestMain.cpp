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

    p->apvts.getParameter ("order")->setValueNotifyingHost (p->apvts.getParameter ("order")->convertTo0to1 (2.0f));
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

    // LFE and layouts: unlabelled 12-channel track (as some DAWs provide) and labelled 5.1
    {
        std::unique_ptr<TheatreProcessor> q (static_cast<TheatreProcessor*> (createPluginFilter()));
        AudioProcessor::BusesLayout d12;
        d12.inputBuses.add (AudioChannelSet::discreteChannels (12));
        d12.outputBuses.add (AudioChannelSet::discreteChannels (12));
        check (q->checkBusesLayoutSupported (d12), "unlabelled 12-channel track accepted");
        q->apvts.getParameter ("order")->setValueNotifyingHost (q->apvts.getParameter ("order")->convertTo0to1 (1.0f));
        q->setBusesLayout (d12);
        q->prepareToPlay (sr, bs);
        check (q->busFormat.load() == hall::F714, "unlabelled 12 ch read as 7.1.4");
        AudioBuffer<float> b12 (12, bs);
        b12.clear();
        for (int n = 0; n < bs; ++n) b12.setSample (3, n, 0.25f * (float) std::sin (2.0 * MathConstants<double>::pi * 60.0 * n / sr));
        q->processBlock (b12, midi);
        check (q->meters[hall::LFE].peak.load() > 0.2f, "LFE (channel 4) detected on unlabelled 12 ch");
        check (q->meters[hall::L].peak.load() == 0.0f && q->meters[hall::Ls].peak.load() == 0.0f, "no leakage into other meters");

        std::unique_ptr<TheatreProcessor> r (static_cast<TheatreProcessor*> (createPluginFilter()));
        AudioProcessor::BusesLayout l51;
        l51.inputBuses.add (AudioChannelSet::create5point1());
        l51.outputBuses.add (AudioChannelSet::create5point1());
        r->apvts.getParameter ("order")->setValueNotifyingHost (r->apvts.getParameter ("order")->convertTo0to1 (2.0f));
        r->setBusesLayout (l51);
        r->prepareToPlay (sr, bs);
        const int iLfe = AudioChannelSet::create5point1().getChannelIndexForType (AudioChannelSet::LFE);
        AudioBuffer<float> b6 (6, bs);
        b6.clear();
        for (int n = 0; n < bs; ++n) b6.setSample (iLfe, n, 0.5f);
        r->processBlock (b6, midi);
        check (std::abs (r->meters[hall::LFE].peak.load() - 0.5f) < 1.0e-4f, "LFE detected on labelled 5.1");

        AudioProcessor::BusesLayout l712;
        l712.inputBuses.add (AudioChannelSet::create7point1point2());
        l712.outputBuses.add (AudioChannelSet::create7point1point2());
        check (q->checkBusesLayoutSupported (l712), "7.1.2 accepted");
    }

    // True peak: a sine at fs/4 sampled 45 degrees off its crest has sample peaks of 0.707 but a true peak of 1.0
    {
        std::unique_ptr<TheatreProcessor> q (static_cast<TheatreProcessor*> (createPluginFilter()));
        q->prepareToPlay (sr, bs);
        AudioBuffer<float> sb (2, bs);
        for (int b = 0; b < 20; ++b)
        {
            for (int n = 0; n < bs; ++n)
                sb.setSample (0, n, (float) std::sin (MathConstants<double>::halfPi * (double) (b * bs + n) + MathConstants<double>::pi / 4.0));
            sb.clear (1, 0, bs);
            q->processBlock (sb, midi);
        }
        const float spk = 20.0f * std::log10 (q->meters[hall::L].peak.load());
        const float tpk = 20.0f * std::log10 (q->meters[hall::L].truePeak.load());
        std::cout << "      fs/4 sine: sample peak " << spk << " dBFS, true peak " << tpk << " dBTP (expected -3.01 / 0.00)" << std::endl;
        check (std::abs (spk + 3.01f) < 0.05f, "sample peak reads -3.01 dBFS");
        check (std::abs (tpk) < 0.3f, "true peak reads 0 dBTP within 0.3 dB");
    }
    // True peak never over-reads a plain 1 kHz sine at -6 dBFS
    {
        std::unique_ptr<TheatreProcessor> q (static_cast<TheatreProcessor*> (createPluginFilter()));
        q->prepareToPlay (sr, bs);
        AudioBuffer<float> sb (2, bs);
        for (int b = 0; b < 20; ++b)
        {
            for (int n = 0; n < bs; ++n)
                sb.setSample (0, n, 0.5f * (float) std::sin (2.0 * MathConstants<double>::pi * 1000.0 * (double) (b * bs + n) / sr));
            sb.clear (1, 0, bs);
            q->processBlock (sb, midi);
        }
        const float tpk = 20.0f * std::log10 (q->meters[hall::L].truePeak.load());
        std::cout << "      1 kHz -6 dBFS sine: true peak " << tpk << " dBTP" << std::endl;
        check (std::abs (tpk + 6.02f) < 0.1f, "1 kHz sine at -6 dBFS reads -6.0 dBTP");
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

    // ---- Channel order: Pro Tools / film (your case: 7.1 track, signal on L, R and LFE) ----
    {
        std::unique_ptr<TheatreProcessor> q (static_cast<TheatreProcessor*> (createPluginFilter()));
        AudioProcessor::BusesLayout l71;
        l71.inputBuses.add (AudioChannelSet::create7point1());
        l71.outputBuses.add (AudioChannelSet::create7point1());
        q->setBusesLayout (l71);
        q->prepareToPlay (sr, bs);
        check ((int) q->apvts.getRawParameterValue ("order")->load() == 0, "default channel order is Pro Tools / Film");
        // Pro Tools 7.1 order: 1 L, 2 C, 3 R, 4 Lss, 5 Rss, 6 Lsr, 7 Rsr, 8 LFE
        AudioBuffer<float> b8 (8, bs);
        b8.clear();
        for (int n = 0; n < bs; ++n) { b8.setSample (0, n, 0.5f); b8.setSample (2, n, 0.25f); b8.setSample (7, n, 0.125f); }
        q->processBlock (b8, midi);
        auto pk = [&] (int c) { return q->meters[(size_t) c].peak.load(); };
        check (std::abs (pk (hall::L) - 0.5f) < 1e-4f, "film order: track ch 1 -> L meter");
        check (std::abs (pk (hall::R) - 0.25f) < 1e-4f, "film order: track ch 3 -> R meter");
        check (std::abs (pk (hall::LFE) - 0.125f) < 1e-4f, "film order: track ch 8 -> LFE meter");
        check (pk (hall::C) == 0.0f && pk (hall::Rrs) == 0.0f, "film order: nothing on C or Rrs");
        std::cout << "      track: " << q->getLayoutDescription() << std::endl;

        // Switch to SMPTE and back
        q->apvts.getParameter ("order")->setValueNotifyingHost (q->apvts.getParameter ("order")->convertTo0to1 (1.0f));
        for (auto& m : q->meters) m.peak.store (0.0f);
        q->processBlock (b8, midi);
        check (std::abs (pk (hall::C) - 0.25f) < 1e-4f && std::abs (pk (hall::Rrs) - 0.125f) < 1e-4f, "SMPTE order reads ch 3 as C and ch 8 as Rrs");

        // Pro Tools 7.1.4: L C R Lss Rss Lsr Rsr LFE Ltf Rtf Ltr Rtr
        std::unique_ptr<TheatreProcessor> r (static_cast<TheatreProcessor*> (createPluginFilter()));
        r->setBusesLayout (l714);
        r->prepareToPlay (sr, bs);
        AudioBuffer<float> b12 (12, bs);
        for (int ch = 0; ch < 12; ++ch)
        {
            b12.clear();
            for (int n = 0; n < bs; ++n) b12.setSample (ch, n, 0.5f);
            for (auto& m : r->meters) m.peak.store (0.0f);
            r->processBlock (b12, midi);
            static const int expect[] = { hall::L, hall::C, hall::R, hall::Ls, hall::Rs, hall::Lrs, hall::Rrs, hall::LFE,
                                          hall::Ltf, hall::Rtf, hall::Ltr, hall::Rtr };
            int lit = -1; int count = 0;
            for (int c = 0; c < hall::NumCh; ++c) if (r->meters[(size_t) c].peak.load() > 0.0f) { lit = c; ++count; }
            check (count == 1 && lit == expect[ch], String ("Pro Tools 7.1.4: track ch ") + String (ch + 1) + " -> " + hall::chName (expect[ch]));
        }
    }

    // ---- Editor ----
    {
        std::unique_ptr<TheatreProcessor> fresh (static_cast<TheatreProcessor*> (createPluginFilter()));
        fresh->setBusesLayout (l714);
        fresh->prepareToPlay (sr, bs);
        std::unique_ptr<TheatreEditor> ed (static_cast<TheatreEditor*> (fresh->createEditor()));
        ed->setSize (ed->LW, TheatreEditor::H);
        for (int i = 0; i < 5; ++i) ed->timerCallback();
        bool allZero = true;
        for (auto k : ed->vs.spkLive) if (k > 0.0f) allZero = false;
        check (allZero, "on opening: no speaker or sphere is lit");
        check (! ed->vs.hasSignal, "on opening: mix position shows no values");
        check (ed->vs.meterChannels.size() == 12, "12 meters for a 7.1.4 track");
        auto snap = [&] (const String& name)
        {
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            File f = outDir.getChildFile (name);
            f.deleteFile();
            FileOutputStream os (f);
            PNGImageFormat().writeImageToStream (img, os);
        };
        snap ("v6_open.png");

        // Pro Tools 7.1.4 track, signal on L and R (equal) + LFE: the sound must sit at the screen centre
        AudioBuffer<float> b (12, bs);
        for (int fr = 0; fr < 60; ++fr)
        {
            Thread::sleep (16);
            for (int k = 0; k < 2; ++k)
            {
                b.clear();
                for (int n = 0; n < bs; ++n)
                {
                    const float v = (rng.nextFloat() * 2.0f - 1.0f) * 0.4f;
                    b.setSample (0, n, v); b.setSample (2, n, v); b.setSample (7, n, 0.2f * v);
                }
                fresh->processBlock (b, midi);
            }
            ed->timerCallback();
        }
        auto& V = ed->vs;
        std::cout << "      sound image at x=" << V.image.x << " y=" << V.image.y << " z=" << V.image.z << std::endl;
        check (std::abs (V.image.x) < 0.3f && V.image.y < 1.0f, "L+R equal -> sound at the screen centre");
        check (std::abs (V.levelDb[hall::L] - V.levelDb[hall::R]) < 2.0f && V.levelDb[hall::C] < -100.0f, "meters: L and R lit, C dark");
        check (V.levelDb[hall::LFE] > -40.0f, "meters: LFE lit");
        float subK = 0; for (size_t i = 0; i < V.model.speakers.size(); ++i) if (V.model.speakers[i].ch == hall::LFE) subK = jmax (subK, V.spkLive[i]);
        check (subK > 0.1f, "LFE spheres lit at the subs");
        check (V.mix.dominant >= 0 && std::abs (std::atan2 (V.image.x - V.model.seats[(size_t) V.model.mixSeat].x,
                                                           -(V.image.y - V.model.seats[(size_t) V.model.mixSeat].y))) < 0.1f,
               "mix position hears it straight ahead");
        ed->setTab (1);
        ed->hall3d.setCamera (0); V.yaw = V.tYaw; V.pitch = V.tPitch; V.zoom = V.tZoom;
        snap ("v6_LR_3d.png");
        ed->setTab (0);
        snap ("v6_LR_plan.png");

        // Moving to the rear left (Lsr = track ch 6)
        for (int fr = 0; fr < 120; ++fr)
        {
            Thread::sleep (16);
            for (int k = 0; k < 2; ++k)
            {
                b.clear();
                for (int n = 0; n < bs; ++n) b.setSample (5, n, (rng.nextFloat() * 2.0f - 1.0f) * 0.4f);
                fresh->processBlock (b, midi);
            }
            ed->timerCallback();
        }
        int loudest = 0;
        for (size_t i = 0; i < V.spkLive.size(); ++i) if (V.spkLive[i] > V.spkLive[(size_t) loudest]) loudest = (int) i;
        check (V.model.speakers[(size_t) loudest].ch == hall::Lrs, "track ch 6 (Lsr) lights the Lrs speakers (" + V.model.speakers[(size_t) loudest].id + ")");
        ed->setTab (1);
        ed->hall3d.setCamera (1); V.yaw = V.tYaw; V.pitch = V.tPitch; V.zoom = V.tZoom;
        snap ("v6_Lrs_side.png");

        // Monitoring fold-down: 7.1.4 content heard on 5.1 -> rear goes to the side surrounds
        ed->setChoice ("monitor", 3);
        ed->timerCallback();
        check (V.format == hall::F51, "monitoring 5.1 selected");
        float lsK = 0; for (size_t i = 0; i < V.model.speakers.size(); ++i) if (V.model.speakers[i].ch == hall::Ls) lsK = jmax (lsK, V.spkLive[i]);
        check (lsK > 0.1f, "5.1 monitoring: Lrs content plays on the left surround array");
    }

    std::cout << (failures == 0 ? "ALL PASSED" : (String (failures) + " FAILED").toStdString()) << std::endl;
    return failures == 0 ? 0 : 1;
}
