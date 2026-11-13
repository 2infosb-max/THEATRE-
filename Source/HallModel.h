#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

// Theatre — hall geometry and coverage model.
// World units are metres. x: left(-) / right(+), y: screen (0) -> rear wall (26), z: floor (0) -> ceiling (9).

namespace hall
{
    constexpr float roomW = 20.0f, roomL = 26.0f, roomH = 9.0f;
    constexpr int numRows = 14, seatsPerRow = 22;

    enum Ch { L = 0, R, C, LFE, Ls, Rs, Lrs, Rrs, Ltf, Rtf, Ltr, Rtr, NumCh };
    inline const char* chName (int c)
    {
        static const char* n[] = { "L", "R", "C", "LFE", "Ls", "Rs", "Lrs", "Rrs", "Ltf", "Rtf", "Ltr", "Rtr" };
        return (c >= 0 && c < NumCh) ? n[c] : "?";
    }

    enum Format { F714 = 0, F71, F51, FStereo, NumFormats };
    inline const char* formatName (int f)
    {
        static const char* n[] = { "7.1.4", "7.1", "5.1", "Stereo" };
        return (f >= 0 && f < NumFormats) ? n[f] : "?";
    }

    // Channels shown on the meters for each monitoring format
    inline std::vector<int> formatChannels (int f)
    {
        switch (f)
        {
            case F714:    return { L, R, C, LFE, Ls, Rs, Lrs, Rrs, Ltf, Rtf, Ltr, Rtr };
            case F71:     return { L, R, C, LFE, Ls, Rs, Lrs, Rrs };
            case F51:     return { L, R, C, LFE, Ls, Rs };
            default:      return { L, R };
        }
    }

    // Which format channel feeds a speaker that belongs to 7.1.4 channel `ch` (-1 = not used in this format)
    inline int mapChannel (int f, int ch)
    {
        switch (f)
        {
            case F714: return ch;
            case F71:  return (ch >= Ltf) ? -1 : ch;
            case F51:
                if (ch >= Ltf) return -1;
                if (ch == Lrs) return Ls;
                if (ch == Rrs) return Rs;
                return ch;
            default:   return (ch == L || ch == R) ? ch : -1;
        }
    }

    struct Vec3 { float x = 0, y = 0, z = 0; };

    // Normalised virtual channel positions used by the channel (bed) panner:
    // x -1..1, y 0 (screen)..1 (rear), z 0 (ear)..1 (ceiling)
    inline bool channelPanPos (int f, int ch, Vec3& out)
    {
        switch (ch)
        {
            case L:   out = { -1, 0, 0 }; return true;
            case R:   out = { 1, 0, 0 };  return true;
            case C:   if (f == FStereo) return false; out = { 0, 0, 0 }; return true;
            case Ls:  if (f == FStereo) return false; out = { -1, f == F51 ? 0.7f : 0.45f, 0 }; return true;
            case Rs:  if (f == FStereo) return false; out = { 1, f == F51 ? 0.7f : 0.45f, 0 }; return true;
            case Lrs: if (f == FStereo || f == F51) return false; out = { -1, 1, 0 }; return true;
            case Rrs: if (f == FStereo || f == F51) return false; out = { 1, 1, 0 }; return true;
            case Ltf: if (f != F714) return false; out = { -0.5f, 0.25f, 1 }; return true;
            case Rtf: if (f != F714) return false; out = { 0.5f, 0.25f, 1 }; return true;
            case Ltr: if (f != F714) return false; out = { -0.5f, 0.8f, 1 }; return true;
            case Rtr: if (f != F714) return false; out = { 0.5f, 0.8f, 1 }; return true;
            default:  return false;
        }
    }

    // Fold a track's per-channel energies into a (smaller) monitoring format.
    // 7.1: heights fold into the wall surrounds (-3 dB); 5.1: rears join the sides; stereo: centre and
    // surrounds split -3 dB to their side, LFE dropped. Same or bigger format: unchanged.
    inline std::array<float, NumCh> foldEnergy (const std::array<float, NumCh>& e, int f)
    {
        std::array<float, NumCh> o = e;
        if (f == F714) return o;
        // heights -> wall surrounds
        o[Ls]  += 0.5f * o[Ltf]; o[Rs]  += 0.5f * o[Rtf];
        o[Lrs] += 0.5f * o[Ltr]; o[Rrs] += 0.5f * o[Rtr];
        o[Ltf] = o[Rtf] = o[Ltr] = o[Rtr] = 0.0f;
        if (f == F71) return o;
        o[Ls] += o[Lrs]; o[Rs] += o[Rrs];
        o[Lrs] = o[Rrs] = 0.0f;
        if (f == F51) return o;
        o[L] += 0.5f * o[C] + 0.5f * o[Ls];
        o[R] += 0.5f * o[C] + 0.5f * o[Rs];
        o[C] = o[Ls] = o[Rs] = o[LFE] = 0.0f;
        return o;
    }

    enum class SpkType { Screen, Sub, Wall, Rear, Top };

    struct Speaker
    {
        juce::String id;
        int ch;
        float x, y, z;
        SpkType type;
    };

    struct Seat
    {
        int row, num;
        float x, y, floor, ear;
        juce::String label;
    };

    // Map the normalised panner position to metres in the hall
    inline Vec3 panToMetres (float px, float py, float pz)
    {
        return { px * 9.6f, 0.3f + py * 25.4f, 1.2f + pz * 7.4f };
    }

    struct Gains
    {
        std::vector<float> spk;                 // per speaker amplitude gain (0 = silent)
        std::array<float, NumCh> chEnergy {};   // per channel energy (sum of g^2 of its speakers)
    };

    struct SeatResult
    {
        float db = -120.0f;     // absolute level (relative units)
        float perceived = 0;    // perceived direction (radians, plan coordinates)
        float intended = 0;     // direction toward the panned position
        float offsetDeg = 0;    // perceived - intended, wrapped
        int dominant = -1;      // speaker index that pulls the image most
        int first = -1;         // first-arriving significant speaker
        float firstMs = 0;
        struct Contribution { int spk; float relDb; float ms; };
        std::vector<Contribution> top;   // strongest contributors
    };

    struct Coverage
    {
        std::vector<float> rel;        // per seat dB relative to the mix seat
        float hot = 0, cold = 0;
        int hotIdx = 0, coldIdx = 0;
        float within3 = 0, within6 = 0, shifted = 0; // fractions 0..1
    };

    class Model
    {
    public:
        Model()
        {
            static const char* rowNames = "ABCDEFGHIJKLMN";
            for (int r = 0; r < numRows; ++r)
            {
                const float y = 7.0f + (float) r * 1.27f, fl = 0.3f + (float) r * 0.3f;
                std::vector<float> xs;
                for (int j = 0; j < 5; ++j)  xs.push_back (-8.0f + (float) j * 0.62f);
                for (int i = 0; i < 12; ++i) xs.push_back (((float) i - 5.5f) * 0.62f);
                for (int j = 0; j < 5; ++j)  xs.push_back (5.52f + (float) j * 0.62f);
                for (int n = 0; n < (int) xs.size(); ++n)
                    seats.push_back ({ r, n, xs[(size_t) n], y, fl, fl + 1.15f,
                                       juce::String::charToString ((juce::juce_wchar) rowNames[r]) + juce::String (n + 1) });
            }
            mixSeat = 8 * seatsPerRow + 10; // row I, centre — about two-thirds back

            auto add = [this] (juce::String id, int ch, float x, float y, float z, SpkType t)
            { speakers.push_back ({ id, ch, x, y, z, t }); };

            add ("L", L, -6.0f, 0.3f, 4.5f, SpkType::Screen);
            add ("C", C, 0.0f, 0.3f, 4.5f, SpkType::Screen);
            add ("R", R, 6.0f, 0.3f, 4.5f, SpkType::Screen);
            add ("Sub1", LFE, -2.5f, 0.7f, 0.6f, SpkType::Sub);
            add ("Sub2", LFE, 2.5f, 0.7f, 0.6f, SpkType::Sub);
            const float sideY[] = { 7.0f, 10.5f, 14.0f, 17.5f };
            for (int i = 0; i < 4; ++i)
            {
                add ("Ls" + juce::String (i + 1), Ls, -9.8f, sideY[i], 5.5f, SpkType::Wall);
                add ("Rs" + juce::String (i + 1), Rs, 9.8f, sideY[i], 5.5f, SpkType::Wall);
            }
            const float rearSideY[] = { 21.0f, 24.5f };
            for (int i = 0; i < 2; ++i)
            {
                add ("Lrs" + juce::String (i + 1), Lrs, -9.8f, rearSideY[i], 5.5f, SpkType::Wall);
                add ("Rrs" + juce::String (i + 1), Rrs, 9.8f, rearSideY[i], 5.5f, SpkType::Wall);
            }
            add ("Lrs3", Lrs, -6.0f, 25.8f, 5.5f, SpkType::Rear);
            add ("Lrs4", Lrs, -2.5f, 25.8f, 5.5f, SpkType::Rear);
            add ("Rrs3", Rrs, 2.5f, 25.8f, 5.5f, SpkType::Rear);
            add ("Rrs4", Rrs, 6.0f, 25.8f, 5.5f, SpkType::Rear);
            const float tf[] = { 5.0f, 9.0f, 13.0f }, tr[] = { 17.0f, 20.5f, 24.0f };
            for (int i = 0; i < 3; ++i)
            {
                add ("Ltf" + juce::String (i + 1), Ltf, -4.5f, tf[i], 8.6f, SpkType::Top);
                add ("Rtf" + juce::String (i + 1), Rtf, 4.5f, tf[i], 8.6f, SpkType::Top);
            }
            for (int i = 0; i < 3; ++i)
            {
                add ("Ltr" + juce::String (i + 1), Ltr, -4.5f, tr[i], 8.6f, SpkType::Top);
                add ("Rtr" + juce::String (i + 1), Rtr, 4.5f, tr[i], 8.6f, SpkType::Top);
            }
        }

        bool speakerUsed (int f, int s) const
        {
            const auto& sp = speakers[(size_t) s];
            if (sp.ch == LFE) return f != FStereo;
            return mapChannel (f, sp.ch) >= 0;
        }

        // Built-in panner: bed (arrays) or object (individual speakers, 7.1.4 only)
        Gains panner (int f, bool object, float px, float py, float pz, float size) const
        {
            Gains G;
            G.spk.assign (speakers.size(), 0.0f);

            if (object && f == F714)
            {
                // Layered object rendering, like the Dolby Atmos Renderer: the object is panned
                // across the ear-level layer (screen + wall speakers) and across the ceiling layer
                // separately, then the two layers are cross-faded by the object's height.
                const auto S = panToMetres (px, py, pz);
                const float a = 3.2f - 2.6f * size;
                const float layerGain[2] = { std::cos (pz * juce::MathConstants<float>::halfPi),
                                             std::sin (pz * juce::MathConstants<float>::halfPi) };
                for (int layer = 0; layer < 2; ++layer)
                {
                    double tot = 0;
                    std::vector<float> w (speakers.size(), 0.0f);
                    for (size_t i = 0; i < speakers.size(); ++i)
                    {
                        const auto& s = speakers[i];
                        if (s.ch == LFE || (s.type == SpkType::Top) != (layer == 1)) continue;
                        const float d2 = sq (S.x - s.x) + sq (S.y - s.y);
                        w[i] = 1.0f / std::pow (d2 + 1.0f, a);
                        tot += (double) w[i] * w[i];
                    }
                    const float n = (float) std::sqrt (tot);
                    if (n > 0)
                        for (size_t i = 0; i < speakers.size(); ++i)
                            G.spk[i] += w[i] / n * layerGain[layer];
                }
            }
            else
            {
                const float a = 2.6f - 2.0f * size;
                std::array<float, NumCh> cg {};
                double tot = 0;
                for (int ch = 0; ch < NumCh; ++ch)
                {
                    Vec3 p;
                    if (ch == LFE || ! channelPanPos (f, ch, p)) continue;
                    const float d2 = sq (px - p.x) + sq (py - p.y) + 1.2f * sq (pz - p.z);
                    const float w = 1.0f / std::pow (d2 + 0.003f, a);
                    cg[(size_t) ch] = w; tot += (double) w * w;
                }
                const float n = (float) std::sqrt (tot);
                if (n > 0) for (auto& g : cg) g /= n;
                distributeChannels (f, cg, G);
            }
            computeChannelEnergy (f, G);
            return G;
        }

        // Track signal: speaker gains from measured per-channel energies (index = Ch)
        Gains fromChannelEnergy (int f, const std::array<float, NumCh>& energy) const
        {
            Gains G;
            G.spk.assign (speakers.size(), 0.0f);
            std::array<float, NumCh> cg {};
            double tot = 0;
            for (int ch = 0; ch < NumCh; ++ch)
            {
                if (ch == LFE) continue;
                const int m = mapChannel (f, ch);
                if (m != ch) continue;
                tot += energy[(size_t) ch];
            }
            if (tot > 1.0e-12)
                for (int ch = 0; ch < NumCh; ++ch)
                    if (ch != LFE && mapChannel (f, ch) == ch)
                        cg[(size_t) ch] = std::sqrt ((float) (energy[(size_t) ch] / tot));
            distributeChannels (f, cg, G);
            if (tot > 1.0e-12 && energy[LFE] > 0 && f != FStereo)
                for (size_t i = 0; i < speakers.size(); ++i)
                    if (speakers[i].ch == LFE)
                        G.spk[i] = std::sqrt ((float) (energy[LFE] / tot) * 0.5f);
            computeChannelEnergy (f, G);
            return G;
        }

        // Where the sound appears: energy-weighted centre of the speakers that play it (LFE excluded)
        Vec3 phantomImage (const Gains& G, bool& ok) const
        {
            Vec3 acc; double tot = 0;
            for (size_t i = 0; i < speakers.size(); ++i)
            {
                const auto& s = speakers[i];
                if (s.ch == LFE) continue;
                const double w = (double) G.spk[i] * G.spk[i];
                acc.x += (float) (s.x * w); acc.y += (float) (s.y * w); acc.z += (float) (s.z * w);
                tot += w;
            }
            ok = tot > 1.0e-12;
            if (! ok) return { 0, 0.3f, 4.5f };
            return { (float) (acc.x / tot), (float) (acc.y / tot), (float) (acc.z / tot) };
        }

        // Estimated panned position from channel energies (for the "Track signal" source)
        Vec3 estimatePan (int f, const std::array<float, NumCh>& energy) const
        {
            Vec3 acc; float tot = 0;
            for (int ch = 0; ch < NumCh; ++ch)
            {
                Vec3 p;
                if (ch == LFE || mapChannel (f, ch) != ch || ! channelPanPos (f, ch, p)) continue;
                const float e = energy[(size_t) ch];
                acc.x += p.x * e; acc.y += p.y * e; acc.z += p.z * e; tot += e;
            }
            if (tot <= 1.0e-12f) return { 0, 0.5f, 0 };
            return { acc.x / tot, acc.y / tot, acc.z / tot };
        }

        SeatResult analyseSeat (const Gains& G, const Seat& se, Vec3 source, bool withTop = false) const
        {
            SeatResult R;
            struct C { int i; float a, t, dx, dy; };
            std::vector<C> cont;
            float amax = 0;
            for (size_t i = 0; i < speakers.size(); ++i)
            {
                const auto& s = speakers[i];
                const float g = G.spk[i];
                if (s.ch == LFE || g < 1.0e-4f) continue;
                const float dx = s.x - se.x, dy = s.y - se.y, dz = s.z - se.ear;
                const float d = std::sqrt (dx * dx + dy * dy + dz * dz);
                const float a = g / std::max (d, 1.0f);
                cont.push_back ({ (int) i, a, d / 343.0f * 1000.0f, dx, dy });
                amax = std::max (amax, a);
            }
            if (cont.empty()) return R;

            double p = 0; float tmin = 1.0e9f;
            for (auto& c : cont)
            {
                p += (double) c.a * c.a;
                if (c.a * c.a > 0.02f * amax * amax && c.t < tmin) { tmin = c.t; R.first = c.i; }
            }
            float vx = 0, vy = 0, wm = 0;
            for (auto& c : cont)
            {
                const float w = c.a * c.a * (1.0f + 3.0f * std::exp (-(c.t - tmin) / 1.5f));
                const float h = std::max (std::sqrt (c.dx * c.dx + c.dy * c.dy), 1.0e-3f);
                vx += w * c.dx / h; vy += w * c.dy / h;
                if (w > wm) { wm = w; R.dominant = c.i; }
            }
            R.db = 10.0f * std::log10 ((float) p + 1.0e-12f);
            R.firstMs = tmin;
            R.perceived = std::atan2 (vy, vx);
            R.intended = std::atan2 (source.y - se.y, source.x - se.x);
            float off = (R.perceived - R.intended) * 180.0f / juce::MathConstants<float>::pi;
            while (off > 180.0f) off -= 360.0f;
            while (off < -180.0f) off += 360.0f;
            R.offsetDeg = off;

            if (withTop)
            {
                std::sort (cont.begin(), cont.end(), [] (const C& a, const C& b) { return a.a > b.a; });
                for (size_t k = 0; k < std::min<size_t> (5, cont.size()); ++k)
                    R.top.push_back ({ cont[k].i, 20.0f * std::log10 (cont[k].a / amax), cont[k].t });
            }
            return R;
        }

        Coverage coverage (const Gains& G, Vec3 source) const
        {
            Coverage cv;
            cv.rel.resize (seats.size());
            const float ref = analyseSeat (G, seats[(size_t) mixSeat], source).db;
            cv.hot = -999; cv.cold = 999;
            int w3 = 0, w6 = 0, sh = 0;
            for (size_t i = 0; i < seats.size(); ++i)
            {
                const auto r = analyseSeat (G, seats[i], source);
                const float rel = r.db - ref;
                cv.rel[i] = rel;
                if (rel > cv.hot) { cv.hot = rel; cv.hotIdx = (int) i; }
                if (rel < cv.cold) { cv.cold = rel; cv.coldIdx = (int) i; }
                if (std::abs (rel) <= 3.0f) ++w3;
                if (std::abs (rel) <= 6.0f) ++w6;
                if (std::abs (r.offsetDeg) > 15.0f) ++sh;
            }
            const float n = (float) seats.size();
            cv.within3 = (float) w3 / n; cv.within6 = (float) w6 / n; cv.shifted = (float) sh / n;
            return cv;
        }

        std::vector<Speaker> speakers;
        std::vector<Seat> seats;
        int mixSeat = 0;

    private:
        static float sq (float v) { return v * v; }

        void distributeChannels (int f, const std::array<float, NumCh>& cg, Gains& G) const
        {
            std::array<int, NumCh> count {};
            for (auto& s : speakers)
            {
                if (s.ch == LFE) continue;
                const int m = mapChannel (f, s.ch);
                if (m >= 0) ++count[(size_t) m];
            }
            for (size_t i = 0; i < speakers.size(); ++i)
            {
                const auto& s = speakers[i];
                if (s.ch == LFE) continue;
                const int m = mapChannel (f, s.ch);
                if (m >= 0 && count[(size_t) m] > 0)
                    G.spk[i] = cg[(size_t) m] / std::sqrt ((float) count[(size_t) m]);
            }
        }

        void computeChannelEnergy (int f, Gains& G) const
        {
            G.chEnergy.fill (0.0f);
            for (size_t i = 0; i < speakers.size(); ++i)
            {
                const auto& s = speakers[i];
                const int m = (s.ch == LFE) ? (f == FStereo ? -1 : (int) LFE) : mapChannel (f, s.ch);
                if (m >= 0) G.chEnergy[(size_t) m] += G.spk[i] * G.spk[i];
            }
        }
    };
}
