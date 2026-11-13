#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "HallModel.h"
#include <functional>

namespace theme
{
    const juce::Colour bg      { 0xff0a0b0f };
    const juce::Colour panel   { 0xff12141b };
    const juce::Colour line    { 0xff232733 };
    const juce::Colour wall    { 0xff39404f };
    const juce::Colour text    { 0xffe8ebf2 };
    const juce::Colour text2   { 0xffb9c0cf };
    const juce::Colour dim     { 0xff8a93a6 };
    const juce::Colour dim2    { 0xff5c6577 };
    const juce::Colour accent  { 0xff4cc9f0 };
    const juce::Colour amber   { 0xffffb347 };
    const juce::Colour cream   { 0xfff4f1ea };
    const juce::Colour green   { 0xff3ddc97 };
    const juce::Colour seatOff { 0xff3b4150 };

    juce::Colour heat (float t);                 // coverage colour scale, t 0..1
    juce::Colour groupColour (hall::SpkType t);
    juce::Font font (float h, bool bold = false);
    juce::Font mono (float h, bool bold = false);
}

class TheatreLook : public juce::LookAndFeel_V4
{
public:
    TheatreLook();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return theme::font (12.0f, true); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return theme::font (12.5f); }
};

// Everything the views need to draw one frame. Owned by the editor.
struct ViewState
{
    static std::array<float, hall::NumCh> filled (float v) { std::array<float, hall::NumCh> a; a.fill (v); return a; }

    hall::Model model;
    int format = hall::F714;          // monitoring format used for the hall
    int show = 0;                     // 0 spheres, 1 spheres + seats, 2 seat heat
    float yaw = 0, pitch = 1.05f, zoom = 1.0f;
    float tYaw = 0, tPitch = 1.05f, tZoom = 1.0f;
    bool orbit = false;

    // Meters: the real channels of the track
    std::vector<int> meterChannels;
    std::array<float, hall::NumCh> levelDb = filled (-140.0f), holdDb = filled (-140.0f), holdTime {}, maxTpDb = filled (-140.0f);
    std::array<bool, hall::NumCh> onBus {};
    float lufsM = -70, lufsS = -70, lufsI = -70, maxPeak = -140;

    // The hall: what each monitored channel plays right now (same ballistics as the meters)
    std::array<float, hall::NumCh> hallDb = filled (-140.0f), hallFlash {};
    hall::Gains gains;                // live speaker gains (relative)
    hall::Coverage cov;
    std::vector<float> spkLive, spkFlash, wavePhase;
    hall::Vec3 image { 0, 0.3f, 4.5f };    // where the sound appears
    std::vector<hall::Vec3> trail;
    hall::SeatResult mix;             // what the mix position hears
    float pulse = 0;                  // travelling pulses from speakers to the mix position
    bool hasSignal = false;
    int covVersion = 0;

    float spkIntensity (size_t i) const { return i < spkLive.size() ? spkLive[i] : 0.0f; }
    juce::Colour seatColour (size_t i, bool planView = false) const
    {
        if ((! planView && show == 0) || ! hasSignal || cov.rel.empty()) return theme::seatOff;
        return theme::heat ((cov.rel[i] + 12.0f) / 18.0f);
    }
};

// ----------------------------------------------------------------------------------
class PlanView : public juce::Component
{
public:
    explicit PlanView (ViewState& v) : vs (v) { setOpaque (true); }
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    ViewState& vs;
    juce::Rectangle<float> planRect, secRect;
    float S = 24.0f, S2 = 13.8f;
    juce::Point<float> P (float x, float y) const { return { planRect.getX() + (x + 10.0f) * S, planRect.getY() + y * S }; }
    juce::Point<float> Q (float y, float z) const { return { secRect.getX() + y * S2, secRect.getY() + (hall::roomH - z) * S2 }; }
};

// ----------------------------------------------------------------------------------
class Hall3DView : public juce::Component
{
public:
    explicit Hall3DView (ViewState&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    juce::TextButton camBooth { "Projection booth" }, camSide { "Side" }, camScreen { "From screen" }, camTop { "Top" }, camMix { "Mix position" };
    juce::TextButton showSpheres { "Spheres" }, showBoth { "Spheres + seats" }, showSeats { "Seat heat" };
    juce::TextButton orbitBtn { "Auto-orbit" };
    void setCamera (int preset);

private:
    struct Proj { juce::Point<float> p; float s; float d; };
    Proj project (float x, float y, float z) const;
    void fillQuad (juce::Graphics&, std::array<hall::Vec3, 4>, juce::Colour fill, juce::Colour edge = {}) const;
    void paintStatic (juce::Graphics&);

    ViewState& vs;
    juce::Image staticLayer;
    juce::String staticKey;
    float dragYaw = 0, dragPitch = 0;
    juce::Point<float> dragStart;
    float cosY = 1, sinY = 0, cosP = 1, sinP = 0, focal = 900.0f, camDist = 50.0f;
    juce::Point<float> centre;
};

// ----------------------------------------------------------------------------------
class MeterPanel : public juce::Component
{
public:
    explicit MeterPanel (ViewState& v) : vs (v) { setOpaque (true); addAndMakeVisible (resetBtn); }
    void paint (juce::Graphics&) override;
    void resized() override { resetBtn.setBounds (getWidth() - 66, 8, 54, 22); }
    juce::TextButton resetBtn { "Reset" };
private:
    ViewState& vs;
};

class MixPanel : public juce::Component
{
public:
    explicit MixPanel (ViewState& v) : vs (v) { setOpaque (true); }
    void paint (juce::Graphics&) override;
private:
    ViewState& vs;
};

// ----------------------------------------------------------------------------------
class TheatreEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TheatreEditor (TheatreProcessor&);
    ~TheatreEditor() override;
    void paint (juce::Graphics&) override {}
    void resized() override;

    static constexpr int W = 1360, H = 860;   // minimum logical size
    int LW = W;                                // logical width, follows the screen shape

private:
    class Root : public juce::Component
    {
    public:
        explicit Root (TheatreEditor& e) : ed (e) { setOpaque (true); }
        void paint (juce::Graphics&) override;
        TheatreEditor& ed;
    };

    void timerCallback() override;
    void setTab (int t);
    void setChoice (const char* id, int index);
    void readMeters (float dt);
    void updateLoudness();
    void refreshButtons();

    TheatreProcessor& proc;
    TheatreLook look;
    ViewState vs;
    Root root { *this };

    juce::TextButton tabPlan { "Plan view" }, tab3D { "3D Hall" };
    std::array<juce::TextButton, 5> monBtns;
    juce::ComboBox orderBox;
    PlanView plan { vs };
    Hall3DView hall3d { vs };
    MeterPanel meters { vs };
    MixPanel mixPanel { vs };

    int tab = 1;
    int tick = 0;
    double lastTime = 0;
    std::array<float, hall::NumCh> energy {};   // slow mean-square per track channel (seat map)
    std::vector<float> loud100;
    std::vector<float> gated400;
    float maxPeakLin = 0;
    juce::ComponentBoundsConstrainer constrainer;

    friend class Root;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TheatreEditor)
};
