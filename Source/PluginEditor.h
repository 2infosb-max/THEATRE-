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
    const juce::Colour panel2  { 0xff0d0f15 };
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
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return theme::font (12.0f, true); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return theme::font (13.0f); }
};

// Everything the views need to draw one frame. Owned by the editor.
struct ViewState
{
    hall::Model model;
    int format = hall::F714;          // effective monitoring format
    bool object = true, trackSource = false;
    float px = 0, py = 0.5f, pz = 0, size = 0.3f;
    hall::Vec3 srcM;                  // source position in metres
    hall::Gains gains;
    hall::Coverage cov;
    hall::SeatResult seatRes;
    int seat = 0;
    int show = 0;                     // 0 spheres, 1 spheres + seats, 2 seat heat
    int tool = 0;                     // 0 move sound, 1 pick seat
    float yaw = 0, pitch = 1.05f, zoom = 1.0f;
    float tYaw = 0, tPitch = 1.05f, tZoom = 1.0f;
    bool orbit = false, fly = false, signal = false;

    std::vector<int> meterChannels;
    std::array<float, hall::NumCh> levelDb = filled (-140.0f), holdDb = filled (-140.0f), holdTime {};
    static std::array<float, hall::NumCh> filled (float v) { std::array<float, hall::NumCh> a; a.fill (v); return a; }
    std::array<bool, hall::NumCh> onBus {};
    float lufsM = -70, lufsS = -70, lufsI = -70, maxPeak = -70;

    // Live, meter-driven animation state
    std::vector<float> spkLive, spkFlash, wavePhase;
    std::array<float, hall::NumCh> chFlash {}, renderFlash {};
    std::array<float, hall::NumCh> renderDb = filled (-140.0f), maxTpDb = filled (-140.0f);
    std::vector<hall::Vec3> trail;
    float srcLevel = 0;

    float spkIntensity (size_t i) const
    {
        if (i < spkLive.size()) return spkLive[i];
        const float g = gains.spk.empty() ? 0.0f : gains.spk[i];
        if (g <= 0.0f) return 0.0f;
        return juce::jlimit (0.0f, 1.0f, 1.0f + 20.0f * std::log10 (g) / 30.0f);
    }
    juce::Colour seatColour (size_t i) const
    {
        if (show == 0 || cov.rel.empty()) return theme::seatOff;
        return theme::heat ((cov.rel[i] + 12.0f) / 18.0f);
    }
};

// ----------------------------------------------------------------------------------
class PlanView : public juce::Component
{
public:
    explicit PlanView (ViewState&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void (float px, float py, int phase)> onPan;     // phase 0 start, 1 move, 2 end
    std::function<void (float py, float pz, int phase)> onSection;
    std::function<void (int seat)> onPickSeat;
    std::function<void()> onToolChanged;

    juce::TextButton moveBtn { "Move sound" }, pickBtn { "Pick seat" };

private:
    ViewState& vs;
    enum class Drag { None, Plan, Section } drag = Drag::None;
    juce::Rectangle<float> planRect, secRect;
    float S = 24.0f, S2 = 13.8f;
    juce::Point<float> P (float x, float y) const { return { planRect.getX() + (x + 10.0f) * S, planRect.getY() + y * S }; }
    juce::Point<float> Q (float y, float z) const { return { secRect.getX() + y * S2, secRect.getY() + (hall::roomH - z) * S2 }; }
    void handlePlan (juce::Point<float>, int phase);
    void handleSection (juce::Point<float>, int phase);
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

    juce::TextButton camBooth { "Projection booth" }, camSide { "Side" }, camScreen { "From screen" }, camTop { "Top" };
    juce::TextButton showSpheres { "Spheres" }, showBoth { "Spheres + seats" }, showSeats { "Seat heat" };
    juce::TextButton orbitBtn { "Auto-orbit" };
    void setCamera (int preset);

private:
    struct Proj { juce::Point<float> p; float s; float d; };
    Proj project (float x, float y, float z) const;
    void fillQuad (juce::Graphics&, std::array<hall::Vec3, 4>, juce::Colour fill, juce::Colour edge = {}) const;

    ViewState& vs;
    float dragYaw = 0, dragPitch = 0;
    juce::Point<float> dragStart;
    float cosY = 1, sinY = 0, cosP = 1, sinP = 0, focal = 900.0f, camDist = 50.0f;
    juce::Point<float> centre;
};

// ----------------------------------------------------------------------------------
class MeterPanel : public juce::Component
{
public:
    explicit MeterPanel (ViewState& v) : vs (v) { addAndMakeVisible (resetBtn); }
    void paint (juce::Graphics&) override;
    void resized() override { resetBtn.setBounds (getWidth() - 66, 8, 54, 22); }
    juce::TextButton resetBtn { "Reset" };
private:
    ViewState& vs;
};

class SoundPanel : public juce::Component
{
public:
    SoundPanel();
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::TextButton srcPanner { "Panner" }, srcTrack { "Track signal" };
    juce::TextButton modeBed { "Bed" }, modeObject { "Object" };
    juce::Slider sx, sy, sz, ssize;
    juce::TextButton flyBtn { "Fly-over test" };
};

class SeatPanel : public juce::Component
{
public:
    explicit SeatPanel (ViewState&);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::ComboBox rowBox, seatBox;
    juce::TextButton mixBtn { "Mix seat" };
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

private:
    class Root : public juce::Component
    {
    public:
        explicit Root (TheatreEditor& e) : ed (e) {}
        void paint (juce::Graphics&) override;
        TheatreEditor& ed;
    };

    void timerCallback() override;
    void setTab (int t);
    void setChoice (const char* id, int index);
    void setParam (const char* id, float value);
    void beginGesture (const char* id);
    void endGesture (const char* id);
    void readMeters (float dt);
    void updateLoudness();
    void refreshButtons();
    void saveUi();

    TheatreProcessor& proc;
    TheatreLook look;
    ViewState vs;
    Root root { *this };

    juce::TextButton tabPlan { "Plan view" }, tab3D { "3D Hall" };
    std::array<juce::TextButton, 4> fmtBtns;
    PlanView plan { vs };
    Hall3DView hall3d { vs };
    MeterPanel meters { vs };
    SoundPanel sound;
    SeatPanel seatPanel { vs };

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SA> ax, ay, az, asize;

    int tab = 1;
    juce::String covKey;
    int tick = 0;
    double lastTime = 0;
    std::array<float, hall::NumCh> energy {}, fastEnergy {};
    int lastCovTick = 0;
    std::vector<float> loud100;        // recent 100 ms blocks
    std::vector<float> gated400;       // 400 ms block energies above the absolute gate
    float maxPeakLin = 0;
    juce::ComponentBoundsConstrainer constrainer;

    static constexpr int W = 1360, H = 860;   // minimum logical size
    int LW = W;                                // logical width, follows the screen shape
    friend class Root;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TheatreEditor)
};
