#include "PluginEditor.h"

using namespace juce;

// =====================================================================================
// Theme
// =====================================================================================
Colour theme::heat (float t)
{
    t = jlimit (0.0f, 1.0f, t);
    static const float stops[] = { 0.0f, 0.33f, 0.66f, 1.0f };
    static const uint8 c[4][3] = { { 29, 35, 80 }, { 36, 100, 122 }, { 201, 138, 46 }, { 255, 211, 107 } };
    for (int i = 1; i < 4; ++i)
        if (t <= stops[i])
        {
            const float f = (t - stops[i - 1]) / (stops[i] - stops[i - 1]);
            auto mix = [&] (int k) { return (uint8) roundToInt (c[i - 1][k] + (c[i][k] - c[i - 1][k]) * f); };
            return Colour (mix (0), mix (1), mix (2));
        }
    return Colour (255, 211, 107);
}

Colour theme::groupColour (hall::SpkType t)
{
    switch (t)
    {
        case hall::SpkType::Screen: return Colour (76, 201, 240);
        case hall::SpkType::Wall:
        case hall::SpkType::Rear:   return Colour (61, 220, 151);
        case hall::SpkType::Top:    return Colour (255, 179, 71);
        case hall::SpkType::Sub:    return Colour (201, 167, 255);
    }
    return Colour (76, 201, 240);
}

Font theme::font (float h, bool bold)
{
    return Font (FontOptions (h, bold ? Font::bold : Font::plain));
}

Font theme::mono (float h, bool bold)
{
    return Font (FontOptions (Font::getDefaultMonospacedFontName(), h, bold ? Font::bold : Font::plain));
}

static String fmtDb (float v, int dp = 1)
{
    if (v <= -69.0f) return String (CharPointer_UTF8 ("\xe2\x80\x94"));
    return String (v, dp);
}

static String signedDb (float v)
{
    return (v >= 0 ? "+" : String (CharPointer_UTF8 ("\xe2\x88\x92"))) + String (std::abs (v), 1);
}

static void sectionTitle (Graphics& g, const String& s, int x, int y)
{
    g.setColour (Colour (0xff9aa3b5));
    g.setFont (theme::font (11.0f, true));
    g.drawText (s.toUpperCase(), x, y, 400, 14, Justification::left);
}

static void drawPanel (Graphics& g, Rectangle<float> r)
{
    g.fillAll (theme::bg);
    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 14.0f);
    g.setColour (theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);
}

// =====================================================================================
// Look and feel
// =====================================================================================
TheatreLook::TheatreLook()
{
    setColour (ComboBox::backgroundColourId, Colour (0xff1b1e27));
    setColour (ComboBox::outlineColourId, Colour (0xff2c3140));
    setColour (ComboBox::textColourId, theme::text);
    setColour (ComboBox::arrowColourId, theme::dim);
    setColour (PopupMenu::backgroundColourId, Colour (0xff1b1e27));
    setColour (PopupMenu::textColourId, theme::text);
    setColour (PopupMenu::highlightedBackgroundColourId, theme::accent);
    setColour (PopupMenu::highlightedTextColourId, Colour (0xff061018));
}

void TheatreLook::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    Colour c = b.getToggleState() ? theme::accent : Colour (0xff1b1e27);
    if (! b.getToggleState() && (over || down)) c = Colour (0xff252a36);
    if (! b.isEnabled()) c = c.withMultipliedAlpha (0.4f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 7.0f);
}

void TheatreLook::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    Colour c = b.getToggleState() ? Colour (0xff061018) : theme::text2;
    if (! b.isEnabled()) c = c.withMultipliedAlpha (0.45f);
    g.setColour (c);
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), Justification::centred, 1);
}

void TheatreLook::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                    Slider::SliderStyle, Slider& s)
{
    const float cy = (float) y + (float) h * 0.5f;
    const auto alpha = s.isEnabled() ? 1.0f : 0.35f;
    g.setColour (Colour (0xff1e222c).withMultipliedAlpha (alpha));
    g.fillRoundedRectangle ((float) x, cy - 3.0f, (float) w, 6.0f, 3.0f);
    const bool bipolar = s.getMinimum() < 0;
    const float zero = bipolar ? (float) x + (float) w * 0.5f : (float) x;
    g.setColour (theme::accent.withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (jmin (zero, pos), cy - 3.0f, std::abs (pos - zero), 6.0f, 3.0f);
    g.setColour (theme::cream.withMultipliedAlpha (alpha));
    g.fillEllipse (pos - 8.0f, cy - 8.0f, 16.0f, 16.0f);
    g.setColour (theme::accent.withMultipliedAlpha (alpha));
    g.drawEllipse (pos - 8.0f, cy - 8.0f, 16.0f, 16.0f, 2.0f);
}

// =====================================================================================
// Plan view
// =====================================================================================
PlanView::PlanView (ViewState& v) : vs (v)
{
    setOpaque (true);
    for (auto* b : { &moveBtn, &pickBtn })
    {
        addAndMakeVisible (*b);
        b->setClickingTogglesState (false);
    }
    moveBtn.onClick = [this] { vs.tool = 0; if (onToolChanged) onToolChanged(); };
    pickBtn.onClick = [this] { vs.tool = 1; if (onToolChanged) onToolChanged(); };
}

void PlanView::resized()
{
    planRect = { 40.0f, 74.0f, hall::roomW * S, hall::roomL * S };
    secRect = { 580.0f, 116.0f, hall::roomL * S2, hall::roomH * S2 };
    moveBtn.setBounds (580, 14, 120, 30);
    pickBtn.setBounds (704, 14, 110, 30);
}

void PlanView::paint (Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    sectionTitle (g, "Hall plan - top view", 22, 20);
    g.setColour (theme::dim);
    g.setFont (theme::font (11.0f));
    g.drawText (vs.tool == 0 ? "Drag on the plan to move the sound" : "Click a seat to check it", 22, 38, 400, 14, Justification::left);

    const auto& M = vs.model;

    // Room, aisles, screen, booth
    g.setColour (Colour (0xff0d0f15));
    g.fillRect (planRect);
    g.setColour (theme::wall);
    g.drawRect (planRect, 1.5f);
    for (float ax : { -5.2f, 3.72f })
    {
        auto a = P (ax, 6.3f);
        g.setColour (Colour (0xff151923));
        g.fillRect (a.x, a.y, 1.48f * S, 17.9f * S);
    }
    {
        auto a = P (-7.0f, 0.12f);
        g.setColour (Colour (0xffe9edf5));
        g.fillRoundedRectangle (a.x, a.y, 14.0f * S, 5.0f, 2.0f);
        g.setColour (Colour (0x55c8dcff));
        g.fillRect (a.x, a.y + 5.0f, 14.0f * S, 10.0f);
        g.setColour (Colour (0xffc9cfdb));
        g.setFont (theme::font (10.0f, true));
        g.drawText ("S C R E E N", (int) a.x, (int) a.y - 18, (int) (14.0f * S), 14, Justification::centred);
        auto b = P (-2.0f, hall::roomL);
        g.setColour (theme::wall);
        g.drawRoundedRectangle (b.x, b.y + 6.0f, 4.0f * S, 20.0f, 3.0f, 1.0f);
        g.setColour (theme::dim2);
        g.setFont (theme::font (9.0f, true));
        g.drawText ("BOOTH", (int) b.x, (int) b.y + 6, (int) (4.0f * S), 20, Justification::centred);
    }

    // Row letters
    g.setFont (theme::mono (9.0f));
    g.setColour (theme::dim2);
    for (int r = 0; r < hall::numRows; ++r)
    {
        auto p = P (-9.55f, 7.0f + (float) r * 1.27f);
        g.drawText (String::charToString ((juce_wchar) ("ABCDEFGHIJKLMN"[r])), (int) p.x, (int) p.y - 6, 12, 12, Justification::centred);
    }

    // Seats
    for (size_t i = 0; i < M.seats.size(); ++i)
    {
        const auto& s = M.seats[i];
        auto p = P (s.x, s.y);
        Rectangle<float> r (p.x - 6.0f, p.y - 6.0f, 12.0f, 9.0f);
        g.setColour (vs.seatColour (i));
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillRect (r.getX(), r.getBottom(), r.getWidth(), 3.0f);
        if ((int) i == vs.seat)
        {
            g.setColour (Colours::white);
            g.drawRoundedRectangle (r.expanded (2.0f), 3.0f, 2.0f);
        }
        else if ((int) i == M.mixSeat)
        {
            g.setColour (theme::accent);
            g.drawRoundedRectangle (r.expanded (1.5f), 3.0f, 1.5f);
        }
    }

    // Speakers
    for (size_t i = 0; i < M.speakers.size(); ++i)
    {
        const auto& s = M.speakers[i];
        const bool used = M.speakerUsed (vs.format, (int) i);
        const float k = vs.spkIntensity (i);
        auto c = theme::groupColour (s.type);
        float w = 12, h = 12;
        if (s.type == hall::SpkType::Screen) { w = 22; h = 8; }
        else if (s.type == hall::SpkType::Sub) { w = 20; h = 10; }
        else if (s.type == hall::SpkType::Wall) { w = 7; h = 14; }
        else if (s.type == hall::SpkType::Rear) { w = 14; h = 7; }
        else { w = 11; h = 11; }
        auto p = P (s.x, s.y);
        Rectangle<float> r (p.x - w * 0.5f, p.y - h * 0.5f, w, h);
        if (k > 0.02f)
        {
            for (int ring = 3; ring >= 1; --ring)
            {
                g.setColour (c.withAlpha (0.10f * k));
                g.fillEllipse (r.expanded ((float) ring * 5.0f * k + 2.0f));
            }
            g.setColour (c.withAlpha (0.3f + 0.7f * k));
        }
        else
            g.setColour (Colour (0xff232733).withMultipliedAlpha (used ? 1.0f : 0.3f));
        if (s.type == hall::SpkType::Top) g.fillEllipse (r); else g.fillRoundedRectangle (r, 2.0f);
        g.setColour ((k > 0.02f ? c : Colour (0xff3a4150)).withMultipliedAlpha (used ? 1.0f : 0.3f));
        if (s.type == hall::SpkType::Top) g.drawEllipse (r, 1.0f); else g.drawRoundedRectangle (r, 2.0f, 1.0f);
        if (s.type == hall::SpkType::Screen)
        {
            g.setFont (theme::font (9.0f, true));
            g.setColour (k > 0.05f ? Colour (0xffbfefff) : theme::dim2);
            g.drawText (s.id, (int) p.x - 15, (int) p.y + 6, 30, 12, Justification::centred);
        }
    }

    // Selected seat: intended (dashed) vs perceived (amber)
    if (vs.hasSignal && vs.seat >= 0 && vs.seat < (int) M.seats.size() && vs.seatRes.dominant >= 0)
    {
        const auto& s = M.seats[(size_t) vs.seat];
        auto p0 = P (s.x, s.y);
        auto p1 = P (vs.srcM.x, vs.srcM.y);
        Path dash;
        dash.startNewSubPath (p0);
        dash.lineTo (p1);
        float dl[] = { 5.0f, 4.0f };
        PathStrokeType (1.5f).createDashedStroke (dash, dash, dl, 2);
        g.setColour (theme::cream.withAlpha (0.6f));
        g.fillPath (dash);
        auto pe = p0 + Point<float> (std::cos (vs.seatRes.perceived), std::sin (vs.seatRes.perceived)) * 70.0f;
        g.setColour (theme::amber.withAlpha (0.35f));
        g.drawLine ({ p0, pe }, 7.0f);
        g.setColour (theme::amber);
        g.drawLine ({ p0, pe }, 3.0f);
        g.fillEllipse (pe.x - 5.0f, pe.y - 5.0f, 10.0f, 10.0f);
    }

    // Trail of where the sound has travelled
    for (size_t i = 1; i < vs.trail.size(); ++i)
    {
        const float t = (float) i / (float) vs.trail.size();
        g.setColour (theme::accent.withAlpha (0.55f * t));
        g.drawLine ({ P (vs.trail[i - 1].x, vs.trail[i - 1].y), P (vs.trail[i].x, vs.trail[i].y) }, 1.0f + 3.0f * t);
    }

    // Source puck (only while there is audio; with the panner, a faint marker shows where it will play)
    if (! vs.hasSignal && ! vs.trackSource)
    {
        auto p = P (vs.srcM.x, vs.srcM.y);
        float dl[] = { 3.0f, 3.0f };
        Path ring, dashed;
        ring.addEllipse (p.x - 10.0f, p.y - 10.0f, 20.0f, 20.0f);
        PathStrokeType (1.2f).createDashedStroke (dashed, ring, dl, 2);
        g.setColour (theme::accent.withAlpha (0.6f));
        g.fillPath (dashed);
        g.setFont (theme::mono (10.0f));
        g.drawText ("pan position - no audio", (int) p.x - 80, (int) p.y + 12, 160, 14, Justification::centred);
    }
    if (vs.hasSignal)
    {
        auto p = P (vs.srcM.x, vs.srcM.y);
        const float d = 20.0f + vs.pz * 16.0f;
        ColourGradient grad (Colours::white, p.x, p.y, theme::accent.withAlpha (0.0f), p.x + d, p.y, true);
        grad.addColour (0.45, Colour (0xffdff6ff));
        grad.addColour (0.7, theme::accent.withAlpha (0.25f));
        g.setGradientFill (grad);
        g.fillEllipse (p.x - d, p.y - d, d * 2.0f, d * 2.0f);
        g.setColour (theme::accent);
        g.drawEllipse (p.x - d * 0.5f, p.y - d * 0.5f, d, d, 2.0f);
        g.setFont (theme::mono (10.0f));
        g.setColour (Colour (0xffbfefff));
        g.drawText (vs.trackSource ? "TRACK (estimated)" : ("height " + String (roundToInt (vs.pz * 100.0f)) + "%"),
                    (int) p.x - 70, (int) (p.y + d * 0.5f + 4.0f), 140, 14, Justification::centred);
    }

    // ---- Side section ----
    sectionTitle (g, "Side section - height", (int) secRect.getX(), (int) secRect.getY() - 40);
    g.setColour (theme::dim);
    g.setFont (theme::font (11.0f));
    g.drawText ("Drag to set depth + height", (int) secRect.getX(), (int) secRect.getY() - 24, 300, 14, Justification::left);
    g.setColour (Colour (0xff0d0f15));
    g.fillRect (secRect);
    g.setColour (theme::wall);
    g.drawRect (secRect, 1.5f);
    {
        auto a = Q (0.1f, 7.5f);
        g.setColour (Colour (0xffe9edf5));
        g.fillRect (a.x + 1.0f, a.y, 4.0f, 6.0f * S2);
    }
    for (int r = 0; r < hall::numRows; ++r)
    {
        const float y = 7.0f + (float) r * 1.27f, fl = 0.3f + (float) r * 0.3f;
        auto a = Q (y - 0.635f, fl);
        g.setColour (Colour (0xff1a1e28));
        g.fillRect (a.x, a.y, 1.27f * S2, fl * S2);
        float acc = 0; int n = 0;
        for (int k = 5; k < 17; ++k)
            if (! vs.cov.rel.empty()) { acc += vs.cov.rel[(size_t) (r * hall::seatsPerRow + k)]; ++n; }
        auto c = (vs.show == 0 || n == 0 || ! vs.hasSignal) ? theme::seatOff : theme::heat ((acc / (float) n + 12.0f) / 18.0f);
        auto sp = Q (y, fl + 1.0f);
        g.setColour (c);
        g.fillRoundedRectangle (sp.x - 4.5f, sp.y, 9.0f, S2, 2.0f);
    }
    for (size_t i = 0; i < M.speakers.size(); ++i)
    {
        const auto& s = M.speakers[i];
        const float k = vs.spkIntensity (i);
        auto p = Q (s.y, s.z);
        auto c = theme::groupColour (s.type);
        if (k > 0.02f)
        {
            g.setColour (c.withAlpha (0.18f * k));
            g.fillEllipse (p.x - 10.0f * k - 4.0f, p.y - 10.0f * k - 4.0f, 20.0f * k + 8.0f, 20.0f * k + 8.0f);
        }
        g.setColour (k > 0.02f ? c.withAlpha (0.35f + 0.65f * k) : Colour (0xff2c3240));
        g.fillRoundedRectangle (p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f, 2.0f);
    }
    if (vs.hasSignal || ! vs.trackSource)
    {
        const float al = vs.hasSignal ? 1.0f : 0.3f;
        auto p = Q (vs.srcM.y, vs.srcM.z);
        g.setOpacity (al);
        g.setColour (theme::accent.withAlpha (0.35f * al));
        g.fillEllipse (p.x - 14.0f, p.y - 14.0f, 28.0f, 28.0f);
        g.setColour (Colours::white);
        g.fillEllipse (p.x - 8.0f, p.y - 8.0f, 16.0f, 16.0f);
        g.setColour (theme::accent);
        g.drawEllipse (p.x - 8.0f, p.y - 8.0f, 16.0f, 16.0f, 2.0f);
    }
    g.setFont (theme::mono (9.0f));
    g.setColour (theme::dim2);
    g.drawText ("SCREEN", (int) secRect.getX(), (int) secRect.getBottom() + 4, 80, 12, Justification::left);
    g.drawText ("REAR WALL", (int) secRect.getRight() - 80, (int) secRect.getBottom() + 4, 80, 12, Justification::right);

    // ---- Coverage summary ----
    const int cx = (int) secRect.getX(), cy0 = (int) secRect.getBottom() + 44;
    sectionTitle (g, "Hall coverage", cx, cy0);
    auto bar = [&] (int y, const String& label, float frac, float alpha)
    {
        g.setFont (theme::font (12.0f));
        g.setColour (theme::text2);
        g.drawText (label, cx, y, 280, 16, Justification::left);
        g.setFont (theme::mono (12.0f, true));
        g.setColour (theme::cream);
        g.drawText (String (roundToInt (frac * 100.0f)) + "%", cx + (int) secRect.getWidth() - 60, y, 60, 16, Justification::right);
        g.setColour (Colour (0xff1e222c));
        g.fillRoundedRectangle ((float) cx, (float) y + 20.0f, secRect.getWidth(), 6.0f, 3.0f);
        g.setColour (theme::accent.withAlpha (alpha));
        g.fillRoundedRectangle ((float) cx, (float) y + 20.0f, secRect.getWidth() * frac, 6.0f, 3.0f);
    };
    if (! vs.hasSignal)
    {
        g.setFont (theme::font (12.0f));
        g.setColour (theme::dim);
        g.drawText (String::fromUTF8 ("\xe2\x80\x94  Waiting for audio. Coverage appears when sound plays."), cx, cy0 + 26, 380, 16, Justification::left);
    }
    else
    {
    bar (cy0 + 26, String::fromUTF8 ("Seats within \xc2\xb1" "3 dB of the mix seat"), vs.cov.within3, 1.0f);
    bar (cy0 + 64, String::fromUTF8 ("Seats within \xc2\xb1" "6 dB"), vs.cov.within6, 0.6f);
    auto row = [&] (int y, const String& l, const String& v, Colour c)
    {
        g.setFont (theme::font (12.0f));
        g.setColour (theme::text2);
        g.drawText (l, cx, y, 260, 16, Justification::left);
        g.setFont (theme::mono (12.0f, true));
        g.setColour (c);
        g.drawText (v, cx + (int) secRect.getWidth() - 160, y, 160, 16, Justification::right);
    };
    if (! vs.cov.rel.empty())
    {
        row (cy0 + 104, "Loudest seat", M.seats[(size_t) vs.cov.hotIdx].label + "  " + signedDb (vs.cov.hot) + " dB", Colour (0xffffd36b));
        row (cy0 + 126, "Quietest seat", M.seats[(size_t) vs.cov.coldIdx].label + "  " + signedDb (vs.cov.cold) + " dB", Colour (0xff7fa3ff));
        row (cy0 + 148, String::fromUTF8 ("Seats where the image shifts >15\xc2\xb0"), String (roundToInt (vs.cov.shifted * 100.0f)) + "%", theme::amber);
    }
    }

    // Legend
    const int ly = getHeight() - 40;
    g.setFont (theme::mono (10.0f));
    g.setColour (theme::dim);
    g.drawText (String::fromUTF8 ("\xe2\x88\x92" "12 dB"), 40, ly, 50, 12, Justification::left);
    ColourGradient lg (theme::heat (0), 92.0f, 0, theme::heat (1), 92.0f + 300.0f, 0, false);
    lg.addColour (0.33, theme::heat (0.33f));
    lg.addColour (0.66, theme::heat (0.66f));
    g.setGradientFill (lg);
    g.fillRoundedRectangle (92.0f, (float) ly + 2.0f, 300.0f, 8.0f, 4.0f);
    g.setColour (theme::dim);
    g.drawText ("+6 dB vs mix seat", 400, ly, 160, 12, Justification::left);
    g.setFont (theme::font (11.0f));
    g.drawText ("Dashed line = where you panned it   |   Amber arrow = where the chosen seat hears it",
                cx, ly - 2, 380, 16, Justification::left);
}

void PlanView::handlePlan (Point<float> p, int phase)
{
    const float x = (p.x - planRect.getX()) / S - 10.0f;
    const float y = (p.y - planRect.getY()) / S;
    if (vs.tool == 1)
    {
        if (phase != 0) return;
        int best = 0; float bd = 1.0e9f;
        for (size_t i = 0; i < vs.model.seats.size(); ++i)
        {
            const auto& s = vs.model.seats[i];
            const float d = (s.x - x) * (s.x - x) + (s.y - y) * (s.y - y);
            if (d < bd) { bd = d; best = (int) i; }
        }
        if (onPickSeat) onPickSeat (best);
        return;
    }
    if (onPan) onPan (jlimit (-1.0f, 1.0f, x / 9.6f), jlimit (0.0f, 1.0f, (y - 0.3f) / 25.4f), phase);
}

void PlanView::handleSection (Point<float> p, int phase)
{
    const float y = (p.x - secRect.getX()) / S2;
    const float z = hall::roomH - (p.y - secRect.getY()) / S2;
    if (onSection) onSection (jlimit (0.0f, 1.0f, (y - 0.3f) / 25.4f), jlimit (0.0f, 1.0f, (z - 1.2f) / 7.4f), phase);
}

void PlanView::mouseDown (const MouseEvent& e)
{
    if (planRect.expanded (6).contains (e.position)) { drag = Drag::Plan; handlePlan (e.position, 0); }
    else if (secRect.expanded (6).contains (e.position)) { drag = Drag::Section; handleSection (e.position, 0); }
    else drag = Drag::None;
}

void PlanView::mouseDrag (const MouseEvent& e)
{
    if (drag == Drag::Plan) handlePlan (e.position, 1);
    else if (drag == Drag::Section) handleSection (e.position, 1);
}

void PlanView::mouseUp (const MouseEvent& e)
{
    if (drag == Drag::Plan) handlePlan (e.position, 2);
    else if (drag == Drag::Section) handleSection (e.position, 2);
    drag = Drag::None;
}

// =====================================================================================
// 3D hall
// =====================================================================================
Hall3DView::Hall3DView (ViewState& v) : vs (v)
{
    setOpaque (true);
    for (auto* b : { &camBooth, &camSide, &camScreen, &camTop, &showSpheres, &showBoth, &showSeats, &orbitBtn })
        addAndMakeVisible (*b);
    camBooth.onClick  = [this] { setCamera (0); };
    camSide.onClick   = [this] { setCamera (1); };
    camScreen.onClick = [this] { setCamera (2); };
    camTop.onClick    = [this] { setCamera (3); };
    showSpheres.onClick = [this] { vs.show = 0; };
    showBoth.onClick    = [this] { vs.show = 1; };
    showSeats.onClick   = [this] { vs.show = 2; };
    orbitBtn.onClick    = [this] { vs.orbit = ! vs.orbit; };
    setMouseCursor (MouseCursor::DraggingHandCursor);
}

void Hall3DView::setCamera (int preset)
{
    const float deg = MathConstants<float>::pi / 180.0f;
    vs.orbit = false;
    switch (preset)
    {
        case 0: vs.tYaw = 0.0f;          vs.tPitch = 60.0f * deg; vs.tZoom = 1.0f;  break;
        case 1: vs.tYaw = -62.0f * deg;  vs.tPitch = 58.0f * deg; vs.tZoom = 1.0f;  break;
        case 2: vs.tYaw = 180.0f * deg;  vs.tPitch = 56.0f * deg; vs.tZoom = 1.0f;  break;
        default: vs.tYaw = 0.0f;         vs.tPitch = 0.0f;        vs.tZoom = 1.05f; break;
    }
    // take the short way round
    while (vs.tYaw - vs.yaw > MathConstants<float>::pi) vs.tYaw -= MathConstants<float>::twoPi;
    while (vs.tYaw - vs.yaw < -MathConstants<float>::pi) vs.tYaw += MathConstants<float>::twoPi;
}

void Hall3DView::resized()
{
    camBooth.setBounds (16, 16, 130, 30);
    camSide.setBounds (150, 16, 60, 30);
    camScreen.setBounds (214, 16, 100, 30);
    camTop.setBounds (318, 16, 54, 30);
    const int r = getWidth() - 16;
    orbitBtn.setBounds (r - 100, 16, 100, 30);
    showSeats.setBounds (r - 196, 16, 90, 30);
    showBoth.setBounds (r - 322, 16, 122, 30);
    showSpheres.setBounds (r - 400, 16, 74, 30);
    centre = { (float) getWidth() * 0.5f, (float) getHeight() * 0.47f };
    focal = (float) getHeight() * 1.6f;
}

Hall3DView::Proj Hall3DView::project (float x, float y, float z) const
{
    x -= 0.0f; y -= hall::roomL * 0.5f; z -= 3.0f;
    const float x1 = x * cosY - y * sinY;
    const float y1 = x * sinY + y * cosY;
    const float v = y1 * cosP - z * sinP;
    const float d = y1 * sinP + z * cosP;
    const float s = focal * vs.zoom / jmax (5.0f, camDist - d);
    return { { centre.x + x1 * s, centre.y + v * s }, s, d };
}

void Hall3DView::fillQuad (Graphics& g, std::array<hall::Vec3, 4> q, Colour fill, Colour edge) const
{
    Path p;
    for (int i = 0; i < 4; ++i)
    {
        auto pr = project (q[(size_t) i].x, q[(size_t) i].y, q[(size_t) i].z).p;
        if (i == 0) p.startNewSubPath (pr); else p.lineTo (pr);
    }
    p.closeSubPath();
    g.setColour (fill);
    g.fillPath (p);
    if (! edge.isTransparent())
    {
        g.setColour (edge);
        g.strokePath (p, PathStrokeType (1.0f));
    }
}

void Hall3DView::paintStatic (Graphics& g)
{

    auto bounds = getLocalBounds().toFloat();
    g.fillAll (theme::bg);
    {
        ColourGradient bgGrad (Colour (0xff141826), bounds.getCentreX(), bounds.getHeight() * 0.35f,
                               Colour (0xff0b0c11), bounds.getCentreX(), bounds.getBottom() + 200.0f, true);
        g.setGradientFill (bgGrad);
        g.fillRoundedRectangle (bounds, 14.0f);
        g.setColour (theme::line);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 14.0f, 1.0f);
    }
    g.reduceClipRegion (getLocalBounds().reduced (1));
    const auto& M = vs.model;
    const float W2 = hall::roomW * 0.5f, Lr = hall::roomL, Hh = hall::roomH;

    // Floor + grid
    fillQuad (g, { hall::Vec3 { -W2, 0, 0 }, { W2, 0, 0 }, { W2, Lr, 0 }, { -W2, Lr, 0 } }, Colour (0xff0f1118), theme::wall);
    g.setColour (Colour (0x12788cb4));
    for (float x = -W2 + 2.0f; x < W2; x += 2.0f) g.drawLine ({ project (x, 0, 0).p, project (x, Lr, 0).p }, 1.0f);
    for (float y = 2.0f; y < Lr; y += 2.0f)       g.drawLine ({ project (-W2, y, 0).p, project (W2, y, 0).p }, 1.0f);

    // Walls (very light so the hall stays readable from every angle)
    const Colour wallFill (0x0e5a6e96), wallEdge (0x2e96aad2);
    fillQuad (g, { hall::Vec3 { -W2, 0, 0 }, { -W2, Lr, 0 }, { -W2, Lr, Hh }, { -W2, 0, Hh } }, wallFill, wallEdge);
    fillQuad (g, { hall::Vec3 { W2, 0, 0 }, { W2, Lr, 0 }, { W2, Lr, Hh }, { W2, 0, Hh } }, wallFill, wallEdge);
    fillQuad (g, { hall::Vec3 { -W2, Lr, 0 }, { W2, Lr, 0 }, { W2, Lr, Hh }, { -W2, Lr, Hh } }, wallFill, wallEdge);
    {
        Path ceiling;
        ceiling.startNewSubPath (project (-W2, 0, Hh).p);
        ceiling.lineTo (project (W2, 0, Hh).p);
        ceiling.lineTo (project (W2, Lr, Hh).p);
        ceiling.lineTo (project (-W2, Lr, Hh).p);
        ceiling.closeSubPath();
        float dl[] = { 6.0f, 6.0f };
        Path dashed;
        PathStrokeType (1.0f).createDashedStroke (dashed, ceiling, dl, 2);
        g.setColour (Colour (0x2e96aad2));
        g.fillPath (dashed);
    }
    // Screen (with a soft glow)
    {
        fillQuad (g, { hall::Vec3 { -7, 0.15f, 1.5f }, { 7, 0.15f, 1.5f }, { 7, 0.15f, 7.5f }, { -7, 0.15f, 7.5f } },
                  Colour (0xffdfe6f2), Colour (0xfff3f6fb));
    }

    // Tiers and seats, back to front
    struct Item { float d; int kind; int idx; };
    std::vector<Item> items;
    items.reserve (330);
    for (int r = 0; r < hall::numRows; ++r)
    {
        const float y = 7.0f + (float) r * 1.27f, fl = 0.3f + (float) r * 0.3f;
        items.push_back ({ project (0, y, fl).d - 0.8f, 0, r });
    }
    for (size_t i = 0; i < M.seats.size(); ++i)
        items.push_back ({ project (M.seats[i].x, M.seats[i].y, M.seats[i].floor + 0.6f).d, 1, (int) i });
    std::sort (items.begin(), items.end(), [] (const Item& a, const Item& b) { return a.d < b.d; });

    for (auto& it : items)
    {
        if (it.kind == 0)
        {
            const float y = 7.0f + (float) it.idx * 1.27f, fl = 0.3f + (float) it.idx * 0.3f;
            fillQuad (g, { hall::Vec3 { -8.5f, y - 0.635f, fl - 0.3f }, { 8.5f, y - 0.635f, fl - 0.3f },
                           { 8.5f, y - 0.635f, fl }, { -8.5f, y - 0.635f, fl } }, Colour (0xff11141c), Colour (0xff2a3040));
            fillQuad (g, { hall::Vec3 { -8.5f, y - 0.635f, fl }, { 8.5f, y - 0.635f, fl },
                           { 8.5f, y + 0.635f, fl }, { -8.5f, y + 0.635f, fl } }, Colour (0xff161a23), Colour (0xff262c3a));
        }
        else
        {
            const auto& s = M.seats[(size_t) it.idx];
            const auto c = vs.seatColour ((size_t) it.idx);
            const float z = s.floor + 0.45f;
            fillQuad (g, { hall::Vec3 { s.x - 0.25f, s.y - 0.2f, z }, { s.x + 0.25f, s.y - 0.2f, z },
                           { s.x + 0.25f, s.y + 0.25f, z }, { s.x - 0.25f, s.y + 0.25f, z } }, c);
            fillQuad (g, { hall::Vec3 { s.x - 0.25f, s.y + 0.25f, z }, { s.x + 0.25f, s.y + 0.25f, z },
                           { s.x + 0.25f, s.y + 0.3f, z + 0.55f }, { s.x - 0.25f, s.y + 0.3f, z + 0.55f } },
                      c.darker (0.35f));
        }
    }
}

Image Hall3DView::sprite (hall::SpkType t)
{
    const int key = (int) t;
    auto& img = sprites[(size_t) key];
    if (img.isNull())
    {
        const int N = 256;
        img = Image (Image::ARGB, N, N, true);
        Graphics sg (img);
        const auto c = theme::groupColour (t);
        const float r = N * 0.5f;
        ColourGradient grad (c.withAlpha (0.22f), r, r, c.withAlpha (0.0f), r + r, r, true);
        grad.addColour (0.62, c.withAlpha (0.12f));
        grad.addColour (0.94, c.withAlpha (0.45f));
        sg.setGradientFill (grad);
        sg.fillEllipse (0, 0, (float) N, (float) N);
        ColourGradient hl (Colours::white.withAlpha (0.22f), r * 0.7f, r * 0.6f, Colours::white.withAlpha (0.0f), r * 0.7f + r * 0.45f, r * 0.6f, true);
        sg.setGradientFill (hl);
        sg.fillEllipse (0, 0, (float) N, (float) N);
    }
    return img;
}

void Hall3DView::paint (Graphics& g)
{
    cosY = std::cos (vs.yaw); sinY = std::sin (vs.yaw);
    cosP = std::cos (vs.pitch); sinP = std::sin (vs.pitch);
    const auto& M = vs.model;

    // The hall itself (walls, floor, seats) is drawn once and reused until the camera or seat colours change
    const float scale = jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const int pw = roundToInt ((float) getWidth() * scale), ph = roundToInt ((float) getHeight() * scale);
    const bool seatsLive = vs.show != 0 && vs.hasSignal;
    const String cacheKey = String (vs.yaw, 4) + "|" + String (vs.pitch, 4) + "|" + String (vs.zoom, 4) + "|" + String (pw) + "x" + String (ph)
                     + "|" + (seatsLive ? String (vs.covVersion) + "/" + String (vs.show) : String ("n"));
    if (cacheKey != staticKey || staticLayer.isNull())
    {
        staticLayer = Image (Image::ARGB, pw, ph, true);
        Graphics ig (staticLayer);
        ig.addTransform (AffineTransform::scale (scale));
        paintStatic (ig);
        staticKey = cacheKey;
    }
    g.drawImage (staticLayer, getLocalBounds().toFloat());

    g.saveState();
    g.reduceClipRegion (getLocalBounds().reduced (1));

    // Speakers, back to front
    {
        std::vector<std::pair<float, int>> order;
        for (size_t i = 0; i < M.speakers.size(); ++i)
            order.push_back ({ project (M.speakers[i].x, M.speakers[i].y, M.speakers[i].z).d, (int) i });
        std::sort (order.begin(), order.end());
        for (auto& o : order)
        {
            const int idx = o.second;
            const auto& s = M.speakers[(size_t) idx];
            const bool used = M.speakerUsed (vs.format, idx);
            const float k = vs.spkIntensity ((size_t) idx);
            auto pr = project (s.x, s.y, s.z);
            auto c = theme::groupColour (s.type);
            const float sz = jmax (6.0f, (s.type == hall::SpkType::Screen ? 0.9f : 0.55f) * pr.s);
            Rectangle<float> r (pr.p.x - sz * 0.5f, pr.p.y - sz * 0.5f, sz, sz);
            if (k > 0.02f)
            {
                g.setColour (c.withAlpha (0.25f * k));
                g.fillEllipse (r.expanded (sz * 0.8f * k + 3.0f));
                g.setColour (c.withAlpha (0.3f + 0.7f * k));
            }
            else
                g.setColour (Colour (0xff262b37).withMultipliedAlpha (used ? 1.0f : 0.2f));
            if (s.type == hall::SpkType::Top) g.fillEllipse (r); else g.fillRoundedRectangle (r, 2.0f);
            g.setColour ((k > 0.02f ? c : Colour (0xff454d60)).withMultipliedAlpha (used ? 1.0f : 0.2f));
            if (s.type == hall::SpkType::Top) g.drawEllipse (r, 1.0f); else g.drawRoundedRectangle (r, 2.0f, 1.0f);
        }
    }

    const auto S = vs.srcM;
    auto srcP = project (S.x, S.y, S.z);

    // Beams: the sound feeding each speaker that is playing it right now
    for (size_t i = 0; i < M.speakers.size(); ++i)
    {
        const float k = vs.spkIntensity (i);
        if (k < 0.15f) continue;
        const auto& sp = M.speakers[i];
        auto pr = project (sp.x, sp.y, sp.z);
        auto c = theme::groupColour (sp.type);
        g.setColour (c.withAlpha (0.08f + 0.40f * k));
        g.drawLine ({ srcP.p, pr.p }, 1.0f + 2.5f * k);
    }

    // Live coverage spheres, driven by the channel meters
    if (vs.show != 2)
    {
        struct Sph { float d, k; int idx; };
        std::vector<Sph> sph;
        for (size_t i = 0; i < M.speakers.size(); ++i)
        {
            const float k = vs.spkIntensity (i);
            if (k < 0.03f) continue;
            sph.push_back ({ project (M.speakers[i].x, M.speakers[i].y, M.speakers[i].z).d, k, (int) i });
        }
        std::vector<float> ks;
        for (auto& s : sph) ks.push_back (s.k);
        std::sort (ks.begin(), ks.end(), std::greater<float>());
        const float labelMin = ks.size() > 5 ? ks[4] : 0.0f;
        std::sort (sph.begin(), sph.end(), [] (const Sph& a, const Sph& b) { return a.d < b.d; });
        for (auto& s : sph)
        {
            const auto& sp = M.speakers[(size_t) s.idx];
            auto pr = project (sp.x, sp.y, sp.z);
            const float flash = s.idx < (int) vs.spkFlash.size() ? vs.spkFlash[(size_t) s.idx] : 0.0f;
            const float rad = (0.6f + 7.0f * s.k + 0.8f * flash) * pr.s;
            const float a = jmin (1.0f, s.k + 0.5f * flash);
            auto c = theme::groupColour (sp.type);
            // Transparent sphere: soft body, denser core, bright rim and a small highlight
            const Rectangle<float> body (pr.p.x - rad, pr.p.y - rad, rad * 2.0f, rad * 2.0f);
            g.setColour (c.withAlpha (0.05f + 0.08f * a));
            g.fillEllipse (body);
            g.setColour (c.withAlpha (0.04f + 0.08f * a));
            g.fillEllipse (body.reduced (rad * 0.38f));
            g.setColour (c.withAlpha (0.12f + 0.40f * a));
            g.drawEllipse (body.reduced (1.0f), 1.5f + 2.0f * a);
            g.setColour (c.withAlpha (0.06f + 0.12f * a));
            g.drawEllipse (body.reduced (rad * 0.06f), rad * 0.08f);


            // Wavefronts leaving the speaker
            const float ph = s.idx < (int) vs.wavePhase.size() ? vs.wavePhase[(size_t) s.idx] : 0.0f;
            for (int w = 0; w < 2; ++w)
            {
                const float f = std::fmod (ph + 0.5f * (float) w, 1.0f);
                const float wr = rad * (0.15f + 1.05f * f);
                g.setColour (c.withAlpha ((1.0f - f) * 0.6f * s.k));
                g.drawEllipse (pr.p.x - wr, pr.p.y - wr, wr * 2.0f, wr * 2.0f, 1.0f + 1.5f * s.k);
            }

            if (s.k < labelMin || s.k < 0.2f) continue;
            const int m = sp.ch == hall::LFE ? (int) hall::LFE : hall::mapChannel (vs.format, sp.ch);
            g.setFont (theme::mono (10.0f, true));
            g.setColour (c);
            g.drawText (sp.id + " " + fmtDb (m >= 0 ? vs.renderDb[(size_t) m] : -90.0f) + " dB",
                        (int) pr.p.x - 60, (int) (pr.p.y - rad - 16.0f), 120, 14, Justification::centred);
        }
    }

    // Trail: where the sound has just travelled
    if (vs.trail.size() > 1)
    {
        const size_t n = vs.trail.size();
        for (size_t i = 1; i < n; ++i)
        {
            const float t = (float) i / (float) n;
            auto a = project (vs.trail[i - 1].x, vs.trail[i - 1].y, vs.trail[i - 1].z).p;
            auto b = project (vs.trail[i].x, vs.trail[i].y, vs.trail[i].z).p;
            g.setColour (theme::accent.withAlpha (0.55f * t));
            g.drawLine ({ a, b }, 1.0f + 3.0f * t);
        }
    }

    // Source: floor ripple, drop line, object sphere (brightness follows the audio)
    if (! vs.hasSignal && ! vs.trackSource)
    {
        // faint marker: where the panner will place the sound once audio plays
        Path pth;
        for (int i = 0; i <= 32; ++i)
        {
            const float ang = (float) i / 32.0f * MathConstants<float>::twoPi;
            auto p = project (S.x + std::cos (ang) * 0.8f, S.y + std::sin (ang) * 0.8f, 0.02f).p;
            if (i == 0) pth.startNewSubPath (p); else pth.lineTo (p);
        }
        g.setColour (theme::accent.withAlpha (0.35f));
        g.strokePath (pth, PathStrokeType (1.2f));
        g.setFont (theme::mono (10.0f));
        g.drawText ("no audio", (int) srcP.p.x - 40, (int) srcP.p.y - 7, 80, 14, Justification::centred);
    }
    if (vs.hasSignal)
    {
        const float live = 0.25f + 0.75f * vs.srcLevel;
        const float rr = 1.0f + vs.size * 3.0f;
        const float phase = (float) std::fmod (Time::getMillisecondCounterHiRes() / (1600.0 - 900.0 * vs.srcLevel), 1.0);
        for (int pass = 0; pass < 2; ++pass)
        {
            Path pth;
            const float rad = pass == 0 ? rr : rr * (0.3f + 1.3f * phase);
            for (int i = 0; i <= 32; ++i)
            {
                const float ang = (float) i / 32.0f * MathConstants<float>::twoPi;
                auto p = project (S.x + std::cos (ang) * rad, S.y + std::sin (ang) * rad, 0.02f).p;
                if (i == 0) pth.startNewSubPath (p); else pth.lineTo (p);
            }
            g.setColour (theme::accent.withAlpha ((pass == 0 ? 0.35f : 0.8f * (1.0f - phase)) * live));
            g.strokePath (pth, PathStrokeType (2.0f));
        }
        auto top = srcP, base = project (S.x, S.y, 0.0f);
        ColourGradient dl (theme::accent.withAlpha (0.9f * live), top.p.x, top.p.y, theme::accent.withAlpha (0.15f * live), base.p.x, base.p.y, false);
        g.setGradientFill (dl);
        g.drawLine ({ top.p, base.p }, 2.0f);
        const float orad = (1.0f + vs.size * 5.4f) * top.s * (0.85f + 0.3f * vs.srcLevel);
        ColourGradient og (Colours::white.withAlpha (0.10f * live), top.p.x, top.p.y, Colours::white.withAlpha (0.0f), top.p.x + orad, top.p.y, true);
        og.addColour (0.7, Colours::white.withAlpha (0.06f * live));
        og.addColour (0.96, Colours::white.withAlpha (0.35f * live));
        g.setGradientFill (og);
        g.fillEllipse (top.p.x - orad, top.p.y - orad, orad * 2.0f, orad * 2.0f);
        const float core = jmax (8.0f, 0.5f * top.s) * (0.8f + 0.5f * vs.srcLevel);
        g.setColour (Colours::white.withAlpha (0.25f * live));
        g.fillEllipse (top.p.x - core * 2.0f, top.p.y - core * 2.0f, core * 4.0f, core * 4.0f);
        ColourGradient cg (Colours::white.withAlpha (live), top.p.x - core * 0.3f, top.p.y - core * 0.3f,
                           Colour (0xff9adff5).withAlpha (live), top.p.x + core, top.p.y + core, true);
        g.setGradientFill (cg);
        g.fillEllipse (top.p.x - core, top.p.y - core, core * 2.0f, core * 2.0f);
        g.setFont (theme::mono (11.0f, true));
        g.setColour (Colour (0xffbfefff).withAlpha (live));
        const String lbl = vs.srcLevel < 0.02f ? String ("NO SIGNAL") : (vs.trackSource ? String ("TRACK") : String ("SOUND"));
        g.drawText (lbl, (int) top.p.x - 50, (int) (top.p.y - orad - 20.0f), 100, 14, Justification::centred);
    }

    // Seat pins
    auto pin = [&] (int idx, const String& label, Colour bgc, Colour fg, float lift)
    {
        const auto& s = M.seats[(size_t) idx];
        auto p = project (s.x, s.y, s.floor + 1.0f);
        auto t = p.p.translated (0, -lift);
        g.setColour (bgc);
        g.drawLine ({ p.p, t }, 2.0f);
        g.setFont (theme::mono (11.0f, true));
        const float w = (float) label.length() * 8.0f + 12.0f;
        Rectangle<float> r (t.x - w * 0.5f, t.y - 18.0f, w, 18.0f);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (fg);
        g.drawText (label, r, Justification::centred);
    };
    pin (M.mixSeat, "MIX", theme::accent, Colour (0xff061018), 16.0f);
    if (vs.seat != M.mixSeat) pin (vs.seat, M.seats[(size_t) vs.seat].label, Colours::white, Colour (0xff0b0c11), 30.0f);

    g.restoreState();

    // Legend
    auto lr = Rectangle<float> (16.0f, (float) getHeight() - 52.0f, (float) getWidth() - 32.0f, 38.0f);
    g.setColour (Colour (0xd90d0f15));
    g.fillRoundedRectangle (lr, 10.0f);
    g.setColour (theme::line);
    g.drawRoundedRectangle (lr, 10.0f, 1.0f);
    int x = (int) lr.getX() + 14;
    const int y = (int) lr.getCentreY() - 7;
    g.setFont (theme::font (11.0f, true));
    g.setColour (Colour (0xff9aa3b5));
    g.drawText ("Spheres", x, y, 60, 14, Justification::left); x += 62;
    auto key = [&] (const String& s, Colour c)
    {
        g.setColour (c.withAlpha (0.35f));
        g.fillEllipse ((float) x, (float) y + 1.0f, 12.0f, 12.0f);
        g.setColour (c);
        g.drawEllipse ((float) x, (float) y + 1.0f, 12.0f, 12.0f, 1.5f);
        g.setFont (theme::font (11.0f));
        g.setColour (theme::text2);
        g.drawText (s, x + 18, y, 80, 14, Justification::left);
        x += 18 + (int) GlyphArrangement::getStringWidth (theme::font (11.0f), s) + 14;
    };
    key ("Screen", theme::groupColour (hall::SpkType::Screen));
    key ("Surrounds", theme::groupColour (hall::SpkType::Wall));
    key ("Ceiling", theme::groupColour (hall::SpkType::Top));
    g.setColour (theme::dim);
    g.setFont (theme::font (11.0f));
    g.drawText ("Bigger, brighter sphere = louder, reaches further  |  Drag to orbit, scroll to zoom", x + 6, y, 420, 14, Justification::left);
    if (vs.show != 0)
    {
        const float lx = lr.getRight() - 250.0f;
        ColourGradient lg (theme::heat (0), lx + 30.0f, 0, theme::heat (1), lx + 170.0f, 0, false);
        lg.addColour (0.33, theme::heat (0.33f));
        lg.addColour (0.66, theme::heat (0.66f));
        g.setGradientFill (lg);
        g.fillRoundedRectangle (lx + 30.0f, (float) y + 3.0f, 140.0f, 8.0f, 4.0f);
        g.setFont (theme::mono (10.0f));
        g.setColour (theme::dim);
        g.drawText (String::fromUTF8 ("\xe2\x88\x92" "12"), (int) lx, y, 28, 14, Justification::right);
        g.drawText ("+6 dB seats", (int) lx + 174, y, 80, 14, Justification::left);
    }
}

void Hall3DView::mouseDown (const MouseEvent& e)
{
    dragStart = e.position;
    dragYaw = vs.tYaw;
    dragPitch = vs.tPitch;
    vs.orbit = false;
}

void Hall3DView::mouseDrag (const MouseEvent& e)
{
    const auto d = e.position - dragStart;
    vs.tYaw = dragYaw + d.x * 0.006f;
    vs.tPitch = jlimit (0.0f, 1.43f, dragPitch - d.y * 0.005f);
    vs.yaw = vs.tYaw;
    vs.pitch = vs.tPitch;
}

void Hall3DView::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    vs.tZoom = jlimit (0.5f, 2.5f, vs.tZoom * (1.0f + w.deltaY * 0.5f));
}

// =====================================================================================
// Meters
// =====================================================================================
static float meterPos (float db)
{
    // 0..-20 dB uses 60 % of the height, -20..-40 dB 25 %, -40..-60 dB 15 %
    if (db >= 0.0f) return 1.0f;
    if (db >= -20.0f) return 0.40f + 0.60f * (db + 20.0f) / 20.0f;
    if (db >= -40.0f) return 0.15f + 0.25f * (db + 40.0f) / 20.0f;
    if (db >= -60.0f) return 0.15f * (db + 60.0f) / 20.0f;
    return 0.0f;
}

void MeterPanel::paint (Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    sectionTitle (g, "Channel meters - track input (true peak)", 14, 12);

    const float mh = 112.0f, top = 50.0f;
    const float left = 40.0f, right = (float) getWidth() - 14.0f;
    g.setFont (theme::mono (9.0f));
    for (int t : { 0, -3, -6, -10, -15, -20, -30, -40, -60 })
    {
        const float y = top + (1.0f - meterPos ((float) t)) * mh;
        g.setColour (Colour (0xff6f788b));
        g.drawText (String (t), 4, (int) y - 5, 30, 10, Justification::right);
        g.setColour (Colour (0x14ffffff));
        g.drawHorizontalLine ((int) y, left - 4.0f, right);
    }
    const auto& chs = vs.meterChannels;
    const int n = (int) chs.size();
    if (n == 0)
    {
        g.setColour (theme::dim);
        g.setFont (theme::font (12.0f));
        g.drawText ("No channels on this track", getLocalBounds().withTrimmedTop (60).withHeight (40), Justification::centred);
    }
    const float slot = (right - left) / (float) jmax (1, n);
    const float bw = jmin (14.0f, slot * 0.55f);
    const float yRed = top + (1.0f - meterPos (-1.0f)) * mh;     // above -1 dBTP
    const float yYel = top + (1.0f - meterPos (-10.0f)) * mh;    // -10 .. -1
    for (int i = 0; i < n; ++i)
    {
        const int c = chs[(size_t) i];
        const float cx = left + slot * ((float) i + 0.5f);
        Rectangle<float> r (cx - bw * 0.5f, top, bw, mh);
        g.setColour (Colour (0xff45201c)); g.fillRect (Rectangle<float>::leftTopRightBottom (r.getX(), top, r.getRight(), yRed));
        g.setColour (Colour (0xff41391a)); g.fillRect (Rectangle<float>::leftTopRightBottom (r.getX(), yRed, r.getRight(), yYel));
        g.setColour (Colour (0xff14382a)); g.fillRect (Rectangle<float>::leftTopRightBottom (r.getX(), yYel, r.getRight(), top + mh));
        const float lit = meterPos (vs.levelDb[(size_t) c]) * mh;
        if (lit > 0.0f)
        {
            g.saveState();
            g.reduceClipRegion (Rectangle<float>::leftTopRightBottom (r.getX(), top + mh - lit, r.getRight(), top + mh).getSmallestIntegerContainer());
            g.setColour (Colour (0xffff5d4a)); g.fillRect (Rectangle<float>::leftTopRightBottom (r.getX(), top, r.getRight(), yRed));
            g.setColour (Colour (0xfff2c84b)); g.fillRect (Rectangle<float>::leftTopRightBottom (r.getX(), yRed, r.getRight(), yYel));
            g.setColour (Colour (0xff2fd18a)); g.fillRect (Rectangle<float>::leftTopRightBottom (r.getX(), yYel, r.getRight(), top + mh));
            g.restoreState();
        }
        if (vs.holdDb[(size_t) c] > -60.0f)
        {
            g.setColour (theme::cream);
            g.fillRect (r.getX(), top + (1.0f - meterPos (vs.holdDb[(size_t) c])) * mh - 1.0f, bw, 2.0f);
        }
        // Max true peak since reset (red at -1 dBTP and above)
        const float mx = vs.maxTpDb[(size_t) c];
        const bool over = mx >= -1.0f;
        Rectangle<float> nb (cx - slot * 0.5f + 1.0f, 30.0f, slot - 2.0f, 14.0f);
        if (over) { g.setColour (Colour (0xffc0392b)); g.fillRoundedRectangle (nb, 3.0f); }
        g.setFont (theme::mono (slot < 26.0f ? 8.0f : 9.5f, true));
        g.setColour (over ? Colours::white : (mx > -60.0f ? theme::text2 : theme::dim2));
        g.drawText (mx > -100.0f ? String (mx, 1) : String ("-inf"), nb, Justification::centred);
        g.setFont (theme::font (9.5f, true));
        g.setColour (theme::text);
        g.drawText (hall::chName (c), (int) (cx - 16.0f), (int) (top + mh + 4.0f), 32, 12, Justification::centred);
    }

    // Loudness row
    const int ly = (int) (top + mh + 24.0f);
    g.setColour (Colour (0xff1e222c));
    g.fillRect (14, ly - 6, getWidth() - 28, 1);
    auto cell = [&] (int x, const String& v, const String& l, Colour c)
    {
        g.setFont (theme::mono (16.0f, true));
        g.setColour (c);
        g.drawText (v, x, ly, 80, 20, Justification::left);
        g.setFont (theme::font (10.0f));
        g.setColour (theme::dim);
        g.drawText (l, x, ly + 20, 90, 12, Justification::left);
    };
    cell (14, fmtDb (vs.lufsM), "Momentary LUFS", theme::text);
    cell (86, fmtDb (vs.lufsS), "Short-term LUFS", theme::text);
    cell (158, fmtDb (vs.lufsI), "Integrated LUFS", theme::text);
    cell (232, fmtDb (vs.maxPeak), "Max dBTP", vs.maxPeak >= -1.0f ? Colour (0xffff5d4a) : Colour (0xfff2c84b));
}

// =====================================================================================
// Sound panel
// =====================================================================================
SoundPanel::SoundPanel()
{
    setOpaque (true);
    for (auto* b : { &srcPanner, &srcTrack, &modeBed, &modeObject, &flyBtn })
        addAndMakeVisible (*b);
    for (auto* s : { &sx, &sy, &sz, &ssize })
    {
        s->setSliderStyle (Slider::LinearHorizontal);
        s->setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (*s);
    }
}

void SoundPanel::resized()
{
    srcPanner.setBounds (14, 36, 80, 28);
    srcTrack.setBounds (96, 36, 100, 28);
    modeBed.setBounds (getWidth() - 126, 36, 52, 28);
    modeObject.setBounds (getWidth() - 72, 36, 58, 28);
    const int colW = (getWidth() - 28 - 16) / 2;
    sx.setBounds (14, 94, colW, 22);
    sy.setBounds (14 + colW + 16, 94, colW, 22);
    sz.setBounds (14, 144, colW, 22);
    ssize.setBounds (14 + colW + 16, 144, colW, 22);
    flyBtn.setBounds (14, getHeight() - 42, getWidth() - 28, 30);
}

void SoundPanel::paint (Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    sectionTitle (g, "Sound position", 14, 14);
    const int colW = (getWidth() - 28 - 16) / 2;
    auto label = [&] (Slider& s, const String& name, const String& val)
    {
        g.setFont (theme::font (12.0f));
        g.setColour (s.isEnabled() ? theme::text2 : theme::dim2);
        g.drawText (name, s.getX(), s.getY() - 18, colW, 16, Justification::left);
        g.setFont (theme::mono (12.0f, true));
        g.setColour (s.isEnabled() ? theme::cream : theme::dim2);
        g.drawText (val, s.getX(), s.getY() - 18, colW, 16, Justification::right);
    };
    label (sx, String::fromUTF8 ("Left \xe2\x86\x94 Right"), String (roundToInt (sx.getValue() * 100.0)));
    label (sy, String::fromUTF8 ("Screen \xe2\x86\x94 Rear"), String (roundToInt (sy.getValue() * 100.0)));
    label (sz, "Height", String (roundToInt (sz.getValue() * 100.0)));
    label (ssize, "Size", String (roundToInt (ssize.getValue() * 100.0)));
}

// =====================================================================================
// Seat panel
// =====================================================================================
SeatPanel::SeatPanel (ViewState& v) : vs (v)
{
    setOpaque (true);
    for (int r = 0; r < hall::numRows; ++r)
        rowBox.addItem (String::charToString ((juce_wchar) ("ABCDEFGHIJKLMN"[r])), r + 1);
    for (int n = 0; n < hall::seatsPerRow; ++n)
        seatBox.addItem (String (n + 1), n + 1);
    addAndMakeVisible (rowBox);
    addAndMakeVisible (seatBox);
    addAndMakeVisible (mixBtn);
}

void SeatPanel::resized()
{
    rowBox.setBounds (48, 36, 62, 28);
    seatBox.setBounds (152, 36, 62, 28);
    mixBtn.setBounds (getWidth() - 94, 36, 80, 28);
}

void SeatPanel::paint (Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    sectionTitle (g, "Seat check", 14, 14);
    g.setFont (theme::font (11.0f));
    g.setColour (theme::dim);
    g.drawText ("Row", 14, 36, 32, 28, Justification::left);
    g.drawText ("Seat", 116, 36, 34, 28, Justification::left);

    const auto& R = vs.seatRes;
    const bool live = vs.hasSignal && R.dominant >= 0;
    // Direction dial
    const Rectangle<float> dial (14.0f, 76.0f, 112.0f, 112.0f);
    const auto c = dial.getCentre();
    g.setColour (Colour (0xff0d0f15));
    g.fillEllipse (dial);
    g.setColour (Colour (0xff2a3040));
    g.drawEllipse (dial, 1.0f);
    g.setColour (Colour (0xffe9edf5));
    g.fillRoundedRectangle (c.x - 22.0f, dial.getY() + 6.0f, 44.0f, 4.0f, 2.0f);
    g.setFont (theme::mono (7.5f));
    g.setColour (Colour (0xff6f788b));
    g.drawText ("SCREEN", (int) c.x - 30, (int) dial.getY() + 12, 60, 10, Justification::centred);
    if (live)
    {
        Path dash;
        dash.startNewSubPath (c);
        dash.lineTo (c + Point<float> (std::cos (R.intended), std::sin (R.intended)) * 46.0f);
        float dl[] = { 4.0f, 3.0f };
        PathStrokeType (1.5f).createDashedStroke (dash, dash, dl, 2);
        g.setColour (theme::cream.withAlpha (0.8f));
        g.fillPath (dash);
        const auto pe = c + Point<float> (std::cos (R.perceived), std::sin (R.perceived)) * 46.0f;
        g.setColour (theme::amber);
        g.drawLine ({ c, pe }, 3.0f);
        g.fillEllipse (pe.x - 4.0f, pe.y - 4.0f, 8.0f, 8.0f);
    }
    g.setColour (Colours::white);
    g.fillRoundedRectangle (c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f, 2.0f);

    // Numbers
    const float rel = vs.cov.rel.empty() ? 0.0f : vs.cov.rel[(size_t) vs.seat];
    const float off = std::abs (R.offsetDeg);
    auto num = [&] (int y, const String& v, const String& l, Colour col, float h)
    {
        g.setFont (theme::mono (h, true));
        g.setColour (col);
        g.drawText (v, 140, y, getWidth() - 154, (int) h + 4, Justification::left);
        g.setFont (theme::font (10.5f));
        g.setColour (theme::dim);
        g.drawText (l, 140, y + (int) h + 3, getWidth() - 154, 12, Justification::left);
    };
    const String dash = String::fromUTF8 ("\xe2\x80\x94");
    num (76, live ? signedDb (rel) + " dB" : dash, "Level vs mix seat", live ? theme::text : theme::dim2, 20.0f);
    num (114, live ? String (roundToInt (off)) + String::fromUTF8 ("\xc2\xb0") : dash, "Off from your pan",
         ! live ? theme::dim2 : (off > 15.0f ? theme::amber : Colour (0xff9be8c4)), 20.0f);
    num (152, live && R.first >= 0 ? vs.model.speakers[(size_t) R.first].id + "  " + String (R.firstMs, 1) + " ms" : dash,
         "Arrives first", live ? theme::text : theme::dim2, 14.0f);

    // Plain-language note
    const auto& seatLabel = vs.model.seats[(size_t) vs.seat].label;
    String note; Colour nb, nf;
    if (! live) { note = "Waiting for audio. Press play: this seat's level and direction appear when sound reaches the hall."; nb = Colour (0xff141824); nf = theme::dim; }
    else if (off > 15.0f)
    {
        note = "Seat " + seatLabel + " hears it " + String (roundToInt (off)) + String::fromUTF8 ("\xc2\xb0")
             + " away from your pan, pulled toward " + vs.model.speakers[(size_t) R.dominant].id
             + " (closest loud speaker, arrives first).";
        nb = Colour (0xff2a1d0c); nf = Colour (0xffffcf8a);
    }
    else if (rel < -6.0f)
    {
        note = "Seat " + seatLabel + " hears it in the right place, but " + String (std::abs (rel), 1) + " dB quieter than the mix seat.";
        nb = Colour (0xff141c2e); nf = Colour (0xffa9c1ff);
    }
    else
    {
        note = "Seat " + seatLabel + " hears it where you panned it, within " + String (std::abs (rel), 1) + " dB of the mix seat.";
        nb = Colour (0xff0f1f19); nf = Colour (0xff9be8c4);
    }
    Rectangle<float> nr (14.0f, 198.0f, (float) getWidth() - 28.0f, 52.0f);
    g.setColour (nb);
    g.fillRoundedRectangle (nr, 10.0f);
    g.setColour (nf);
    g.setFont (theme::font (11.5f));
    g.drawFittedText (note, nr.reduced (10.0f, 6.0f).toNearestInt(), Justification::centredLeft, 3, 1.0f);

    // Legend + stats
    g.setFont (theme::font (10.5f));
    g.setColour (theme::dim);
    const int ly = getHeight() - 30;
    g.setColour (theme::cream);
    g.drawLine (14.0f, (float) ly + 7.0f, 30.0f, (float) ly + 7.0f, 1.5f);
    g.setColour (theme::dim);
    g.drawText ("Your pan", 34, ly, 60, 14, Justification::left);
    g.setColour (theme::amber);
    g.fillRoundedRectangle (100.0f, (float) ly + 5.5f, 16.0f, 3.0f, 1.5f);
    g.setColour (theme::dim);
    g.drawText ("What this seat hears", 120, ly, 130, 14, Justification::left);
    g.setFont (theme::mono (10.5f, true));
    g.setColour (Colour (0xff8fe0f7));
    g.drawText (String::fromUTF8 ("\xc2\xb1") + "3 dB: " + (live ? String (roundToInt (vs.cov.within3 * 100.0f)) + "%" : dash),
                getWidth() - 104, ly, 90, 14, Justification::right);
}

// =====================================================================================
// Editor
// =====================================================================================
TheatreEditor::TheatreEditor (TheatreProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (root);

    for (auto* b : { &tabPlan, &tab3D }) root.addAndMakeVisible (*b);
    tabPlan.onClick = [this] { setTab (0); };
    tab3D.onClick = [this] { setTab (1); };
    const char* names[] = { "7.1.4", "7.1", "5.1", "Stereo" };
    for (int i = 0; i < 4; ++i)
    {
        fmtBtns[(size_t) i].setButtonText (names[i]);
        fmtBtns[(size_t) i].onClick = [this, i] { setChoice ("monitor", i); };
        root.addAndMakeVisible (fmtBtns[(size_t) i]);
    }

    root.addChildComponent (plan);
    root.addChildComponent (hall3d);
    root.addAndMakeVisible (meters);
    root.addAndMakeVisible (sound);
    root.addAndMakeVisible (seatPanel);

    ax    = std::make_unique<SA> (proc.apvts, "x", sound.sx);
    ay    = std::make_unique<SA> (proc.apvts, "y", sound.sy);
    az    = std::make_unique<SA> (proc.apvts, "z", sound.sz);
    asize = std::make_unique<SA> (proc.apvts, "size", sound.ssize);

    sound.srcPanner.onClick  = [this] { setChoice ("source", 0); };
    sound.srcTrack.onClick   = [this] { setChoice ("source", 1); };
    sound.modeBed.onClick    = [this] { setChoice ("mode", 0); };
    sound.modeObject.onClick = [this] { setChoice ("mode", 1); };
    sound.flyBtn.onClick     = [this]
    {
        vs.fly = ! vs.fly;
        for (auto* id : { "x", "y", "z" })
            vs.fly ? beginGesture (id) : endGesture (id);
    };
    meters.resetBtn.onClick = [this] { gated400.clear(); loud100.clear(); maxPeakLin = 0; vs.maxTpDb.fill (-140.0f); };

    plan.onPan = [this] (float px, float py, int phase)
    {
        if (vs.trackSource) return;
        vs.fly = false;
        if (phase == 0) { beginGesture ("x"); beginGesture ("y"); }
        setParam ("x", px); setParam ("y", py);
        if (phase == 2) { endGesture ("x"); endGesture ("y"); }
    };
    plan.onSection = [this] (float py, float pz, int phase)
    {
        if (vs.trackSource) return;
        vs.fly = false;
        if (phase == 0) { beginGesture ("y"); beginGesture ("z"); }
        setParam ("y", py); setParam ("z", pz);
        if (phase == 2) { endGesture ("y"); endGesture ("z"); }
    };
    plan.onPickSeat = [this] (int s) { vs.seat = s; covKey.clear(); saveUi(); };
    plan.onToolChanged = [this] { refreshButtons(); };

    seatPanel.rowBox.onChange = [this]
    {
        vs.seat = (seatPanel.rowBox.getSelectedId() - 1) * hall::seatsPerRow + (vs.seat % hall::seatsPerRow);
        covKey.clear(); saveUi();
    };
    seatPanel.seatBox.onChange = [this]
    {
        vs.seat = (vs.seat / hall::seatsPerRow) * hall::seatsPerRow + (seatPanel.seatBox.getSelectedId() - 1);
        covKey.clear(); saveUi();
    };
    seatPanel.mixBtn.onClick = [this] { vs.seat = vs.model.mixSeat; covKey.clear(); saveUi(); };

    // Restore UI state
    auto& ui = proc.uiState;
    vs.seat = jlimit (0, (int) vs.model.seats.size() - 1, (int) ui.getProperty ("seat", vs.model.mixSeat));
    vs.show = jlimit (0, 2, (int) ui.getProperty ("show", 0));
    hall3d.setCamera (0);
    vs.yaw = vs.tYaw; vs.pitch = vs.tPitch; vs.zoom = vs.tZoom;
    tab = jlimit (0, 1, (int) ui.getProperty ("tab", 1));

    // Open filling the screen: match the layout to the screen's shape, then scale to fit.
    Rectangle<int> screen (0, 0, 1440, 900);
    if (auto* d = Desktop::getInstance().getDisplays().getPrimaryDisplay())
        screen = d->userArea;
    const int hostBar = 44;                                   // the DAW's own plug-in window title bar
    const int availW = jmax (800, screen.getWidth());
    const int availH = jmax (500, screen.getHeight() - hostBar);
    LW = jlimit (W, W * 2, roundToInt ((double) H * (double) availW / (double) availH));
    root.setBounds (0, 0, LW, H);
    const double fit = jmin ((double) availW / (double) LW, (double) availH / (double) H);
    constrainer.setFixedAspectRatio ((double) LW / (double) H);
    constrainer.setSizeLimits (LW / 2, H / 2, jmax (availW, LW * 2), jmax (availH, H * 2));
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (roundToInt (LW * fit), roundToInt (H * fit));

    setTab (tab);
    lastTime = Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

TheatreEditor::~TheatreEditor()
{
    stopTimer();
    if (vs.fly) for (auto* id : { "x", "y", "z" }) endGesture (id);
    setLookAndFeel (nullptr);
}

void TheatreEditor::resized()
{
    const float s = (float) getHeight() / (float) H;
    root.setTransform (AffineTransform::scale (s));
    const int W = LW;   // logical width matches the screen's shape

    tabPlan.setBounds (232, 14, 92, 32);
    tab3D.setBounds (326, 14, 80, 32);
    for (int i = 0; i < 4; ++i)
        fmtBtns[(size_t) i].setBounds (W - 24 - (4 - i) * 70, 14, 66, 32);

    const int sw = 356, sxp = W - 16 - sw;
    const Rectangle<int> view (16, 64, sxp - 16 - 16, 780);
    plan.setBounds (view);
    hall3d.setBounds (view);
    meters.setBounds (sxp, 64, sw, 228);
    sound.setBounds (sxp, 306, sw, 222);
    seatPanel.setBounds (sxp, 542, sw, 302);
}

void TheatreEditor::Root::paint (Graphics& g)
{
    const int W = ed.LW;

    g.fillAll (theme::bg);
    ColourGradient bg (Colour (0xff141824), (float) W * 0.5f, 0.0f, theme::bg, (float) W * 0.5f, 60.0f, false);
    g.setGradientFill (bg);
    g.fillRect (0, 0, W, 60);

    g.setColour (theme::cream);
    g.setFont (theme::font (24.0f, true));
    g.drawText ("T H E A T R E", 20, 12, 200, 24, Justification::left);
    g.setColour (theme::dim);
    g.setFont (theme::font (11.0f));
    g.drawText ("by Sound Brewery  -  v" + String (JucePlugin_VersionString), 20, 36, 200, 14, Justification::left);

    const auto& v = ed.vs;
    String info = String (hall::formatName (v.format)) + " - ";
    switch (v.format)
    {
        case hall::F714: info << "33 speakers incl. ceiling + subs"; break;
        case hall::F71:  info << "21 speakers, ceiling off"; break;
        case hall::F51:  info << "21 speakers, rear arrays join the sides"; break;
        default:         info << "L + R screen speakers only"; break;
    }
    if (v.trackSource) info << " - following the track signal";
    else if (v.object && v.format != hall::F714) info << " - objects folded to channels";
    g.setColour (theme::text2);
    g.setFont (theme::font (12.0f));
    g.drawFittedText ("Hall: " + info, 424, 8, 336, 22, Justification::centredLeft, 1, 0.8f);
    g.setColour (theme::dim);
    g.setFont (theme::font (11.0f));
    g.drawFittedText ("Track: " + ed.proc.getLayoutDescription(), 424, 30, 336, 20, Justification::centredLeft, 1, 0.7f);

    // latency chip
    g.setColour (Colour (0xff0f1a16));
    g.fillRoundedRectangle (770.0f, 18.0f, 186.0f, 24.0f, 12.0f);
    g.setColour (Colour (0xff1f4436));
    g.drawRoundedRectangle (770.0f, 18.0f, 186.0f, 24.0f, 12.0f, 1.0f);
    g.setColour (theme::green);
    g.fillEllipse (782.0f, 26.0f, 8.0f, 8.0f);
    g.setColour (Colour (0xff9be8c4));
    g.setFont (theme::mono (10.5f));
    g.drawText ("0 samples added - pass-through", 796, 18, 172, 24, Justification::left);

    g.setColour (Colour (0xff9aa3b5));
    g.setFont (theme::font (10.0f, true));
    g.drawText ("MONITORING", W - 24 - 280 - 96, 14, 88, 32, Justification::right);
}

void TheatreEditor::setTab (int t)
{
    tab = t;
    plan.setVisible (t == 0);
    hall3d.setVisible (t == 1);
    proc.uiState.setProperty ("tab", t, nullptr);
    refreshButtons();
}

void TheatreEditor::setChoice (const char* id, int index)
{
    if (auto* p = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (id)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
        p->endChangeGesture();
    }
    covKey.clear();
}

void TheatreEditor::setParam (const char* id, float value)
{
    if (auto* p = proc.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void TheatreEditor::beginGesture (const char* id) { if (auto* p = proc.apvts.getParameter (id)) p->beginChangeGesture(); }
void TheatreEditor::endGesture (const char* id)   { if (auto* p = proc.apvts.getParameter (id)) p->endChangeGesture(); }

void TheatreEditor::saveUi()
{
    proc.uiState.setProperty ("seat", vs.seat, nullptr);
    proc.uiState.setProperty ("show", vs.show, nullptr);
}

void TheatreEditor::refreshButtons()
{
    tabPlan.setToggleState (tab == 0, dontSendNotification);
    tab3D.setToggleState (tab == 1, dontSendNotification);
    for (int i = 0; i < 4; ++i)
    {
        fmtBtns[(size_t) i].setToggleState (vs.format == i, dontSendNotification);
        fmtBtns[(size_t) i].setEnabled (! vs.trackSource);
    }
    sound.srcPanner.setToggleState (! vs.trackSource, dontSendNotification);
    sound.srcTrack.setToggleState (vs.trackSource, dontSendNotification);
    sound.modeBed.setToggleState (! vs.object, dontSendNotification);
    sound.modeObject.setToggleState (vs.object, dontSendNotification);
    for (auto* c : std::initializer_list<Component*> { &sound.modeBed, &sound.modeObject, &sound.sx, &sound.sy, &sound.sz, &sound.ssize, &sound.flyBtn })
        c->setEnabled (! vs.trackSource);
    sound.flyBtn.setToggleState (vs.fly, dontSendNotification);
    sound.flyBtn.setButtonText (vs.fly ? "Stop fly-over" : "Fly-over test");
    plan.moveBtn.setToggleState (vs.tool == 0, dontSendNotification);
    plan.pickBtn.setToggleState (vs.tool == 1, dontSendNotification);
    hall3d.showSpheres.setToggleState (vs.show == 0, dontSendNotification);
    hall3d.showBoth.setToggleState (vs.show == 1, dontSendNotification);
    hall3d.showSeats.setToggleState (vs.show == 2, dontSendNotification);
    hall3d.orbitBtn.setToggleState (vs.orbit, dontSendNotification);
    seatPanel.rowBox.setSelectedId (vs.seat / hall::seatsPerRow + 1, dontSendNotification);
    seatPanel.seatBox.setSelectedId (vs.seat % hall::seatsPerRow + 1, dontSendNotification);
}

void TheatreEditor::readMeters (float dt)
{
    // IEC 60268-18 style peak meter: instant attack, 20 dB fall in 1.7 s, 2 s peak hold.
    const float fall = 20.0f / 1.7f * dt;
    auto toDb = [] (float v) { return v > 1.0e-7f ? 20.0f * std::log10 (v) : -140.0f; };

    float srcPk = 0.0f;
    std::array<float, hall::NumCh> rawTp {};
    for (int c = 0; c < hall::NumCh; ++c)
    {
        auto& m = proc.meters[(size_t) c];
        const float pk = m.peak.exchange (0.0f);
        const float tp = m.truePeak.exchange (0.0f);
        const float ss = m.sumSq.exchange (0.0f);
        const int n = m.count.exchange (0);
        rawTp[(size_t) c] = tp;
        srcPk = jmax (srcPk, pk);
        maxPeakLin = jmax (maxPeakLin, tp);
        vs.maxTpDb[(size_t) c] = jmax (vs.maxTpDb[(size_t) c], toDb (tp));
        const float ms = n > 0 ? ss / (float) n : 0.0f;
        fastEnergy[(size_t) c] = ms > fastEnergy[(size_t) c] ? ms : 0.6f * fastEnergy[(size_t) c] + 0.4f * ms;
        energy[(size_t) c] = 0.85f * energy[(size_t) c] + 0.15f * ms;

        // The meters always show the real channels of the track
        vs.onBus[(size_t) c] = proc.channelPresent[(size_t) c].load();
        const float db = vs.onBus[(size_t) c] ? toDb (tp) : -140.0f;
        auto& lv = vs.levelDb[(size_t) c];
        if (db > lv + 3.0f) vs.chFlash[(size_t) c] = 1.0f;
        lv = db > lv ? db : jmax (db, lv - fall);
        vs.chFlash[(size_t) c] = jmax (0.0f, vs.chFlash[(size_t) c] - 4.0f * dt);
        auto& hold = vs.holdDb[(size_t) c];
        auto& ht = vs.holdTime[(size_t) c];
        if (lv >= hold) { hold = lv; ht = 0.0f; }
        else { ht += dt; if (ht > 2.0f) hold = jmax (lv, hold - fall); }
    }
    vs.maxPeak = toDb (maxPeakLin);
    const float srcDb = toDb (srcPk);

    // Meter list: the channels actually on the track, in film order
    vs.meterChannels.clear();
    for (int c = 0; c < hall::NumCh; ++c)
        if (vs.onBus[(size_t) c]) vs.meterChannels.push_back (c);

    // What each hall channel plays: Track signal = the real channels; Panner = the track's audio
    // rendered by Theatre's panner (same ballistics as the meters).
    const auto chs = hall::formatChannels (vs.format);
    for (int c = 0; c < hall::NumCh; ++c)
    {
        float db;
        if (vs.trackSource)
            db = toDb (rawTp[(size_t) c]);
        else
        {
            const bool inFormat = std::find (chs.begin(), chs.end(), c) != chs.end();
            const float e = vs.gains.chEnergy[(size_t) c];
            db = (inFormat && e > 1.0e-9f) ? srcDb + 10.0f * std::log10 (e) : -140.0f;
        }
        auto& r = vs.renderDb[(size_t) c];
        if (db > r + 3.0f) vs.renderFlash[(size_t) c] = 1.0f;
        r = db > r ? db : jmax (db, r - fall);
        vs.renderFlash[(size_t) c] = jmax (0.0f, vs.renderFlash[(size_t) c] - 4.0f * dt);
    }

    // Drive every speaker (and its sphere) from its channel
    const auto& spk = vs.model.speakers;
    vs.spkLive.resize (spk.size());
    vs.spkFlash.resize (spk.size());
    vs.wavePhase.resize (spk.size(), 0.0f);
    for (size_t i = 0; i < spk.size(); ++i)
    {
        const int m = spk[i].ch == hall::LFE ? (vs.format == hall::FStereo ? -1 : (int) hall::LFE)
                                             : hall::mapChannel (vs.format, spk[i].ch);
        float k = 0.0f, fl = 0.0f;
        if (m >= 0)
        {
            k = jlimit (0.0f, 1.0f, (vs.renderDb[(size_t) m] + 54.0f) / 48.0f);
            fl = vs.renderFlash[(size_t) m];
        }
        vs.spkLive[i] = k;
        vs.spkFlash[i] = fl * k;
        vs.wavePhase[i] = std::fmod (vs.wavePhase[i] + dt * (0.6f + 1.4f * k), 1.0f);
    }
    vs.srcLevel = jlimit (0.0f, 1.0f, (srcDb + 54.0f) / 48.0f);

    // Signal present = any channel on the track is above -70 dBTP (after meter fall-back)
    bool sig = false;
    for (int c = 0; c < hall::NumCh; ++c)
        if (vs.onBus[(size_t) c] && vs.levelDb[(size_t) c] > -70.0f) sig = true;
    if (sig != vs.hasSignal) { vs.hasSignal = sig; ++vs.covVersion; covKey.clear(); }
    if (! sig) vs.trail.clear();
}

void TheatreEditor::updateLoudness()
{
    float tmp[256];
    int n;
    while ((n = proc.readLoudnessBlocks (tmp, 256)) > 0)
        for (int i = 0; i < n; ++i)
        {
            loud100.push_back (tmp[i]);
            if (loud100.size() > 30) loud100.erase (loud100.begin());
            if (loud100.size() >= 4)
            {
                double e = 0;
                for (size_t k = loud100.size() - 4; k < loud100.size(); ++k) e += loud100[k];
                e /= 4.0;
                const double l = -0.691 + 10.0 * std::log10 (e + 1.0e-12);
                if (l > -70.0) gated400.push_back ((float) e);
            }
        }
    auto lufs = [] (double e) { return (float) (-0.691 + 10.0 * std::log10 (e + 1.0e-12)); };
    auto meanLast = [this] (size_t k)
    {
        if (loud100.size() < k) return 0.0;
        double e = 0;
        for (size_t i = loud100.size() - k; i < loud100.size(); ++i) e += loud100[i];
        return e / (double) k;
    };
    vs.lufsM = loud100.size() >= 4 ? lufs (meanLast (4)) : -70.0f;
    vs.lufsS = loud100.size() >= 30 ? lufs (meanLast (30)) : -70.0f;
    if (! gated400.empty())
    {
        double e = 0;
        for (auto v : gated400) e += v;
        e /= (double) gated400.size();
        const double rel = lufs (e) - 10.0;
        double e2 = 0; int k = 0;
        for (auto v : gated400) if (lufs (v) > rel) { e2 += v; ++k; }
        vs.lufsI = k > 0 ? lufs (e2 / k) : -70.0f;
    }
    else vs.lufsI = -70.0f;
    if (vs.lufsM < -69.0f) vs.lufsM = -70.0f;
    if (vs.lufsS < -69.0f) vs.lufsS = -70.0f;
}

void TheatreEditor::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    const float dt = (float) jlimit (0.0, 0.2, (now - lastTime) / 1000.0);
    lastTime = now;
    ++tick;

    auto& a = proc.apvts;
    vs.trackSource = (int) a.getRawParameterValue ("source")->load() == 1;
    vs.object = (int) a.getRawParameterValue ("mode")->load() == 1;
    vs.format = vs.trackSource ? proc.busFormat.load() : (int) a.getRawParameterValue ("monitor")->load();

    if (vs.fly && ! vs.trackSource)
    {
        const float f = (float) std::fmod (now / 1000.0 * 0.11, 1.0);
        setParam ("x", 0.15f * std::sin (f * MathConstants<float>::twoPi));
        setParam ("y", f);
        setParam ("z", std::sin (MathConstants<float>::pi * f));
    }
    vs.px = a.getRawParameterValue ("x")->load();
    vs.py = a.getRawParameterValue ("y")->load();
    vs.pz = a.getRawParameterValue ("z")->load();
    vs.size = a.getRawParameterValue ("size")->load();

    if (vs.orbit) vs.tYaw += 0.5f * dt;
    const float k = 1.0f - std::exp (-dt * 8.0f);
    vs.yaw += (vs.tYaw - vs.yaw) * k;
    vs.pitch += (vs.tPitch - vs.pitch) * k;
    vs.zoom += (vs.tZoom - vs.zoom) * k;

    if (vs.trackSource)
    {
        // The sound's position follows the live channel energies (fast), the seat map updates ~10x a second
        vs.gains = vs.model.fromChannelEnergy (vs.format, fastEnergy);
        float tot = 0; for (auto e : fastEnergy) tot += e;
        if (tot > 1.0e-9f)
        {
            const auto est = vs.model.estimatePan (vs.format, fastEnergy);
            const float e = 1.0f - std::exp (-dt * 10.0f);
            vs.px += (est.x - vs.px) * e; vs.py += (est.y - vs.py) * e; vs.pz += (est.z - vs.pz) * e;
        }
        vs.srcM = hall::panToMetres (vs.px, vs.py, vs.pz);
        if (tick % 6 == 0 || covKey != "track")
        {
            const auto slow = vs.model.fromChannelEnergy (vs.format, energy);
            vs.cov = vs.model.coverage (slow, vs.srcM);
            ++vs.covVersion;
            vs.seatRes = vs.model.analyseSeat (slow, vs.model.seats[(size_t) vs.seat], vs.srcM, true);
            covKey = "track";
        }
    }
    else
    {
        const String key = String (vs.px, 3) + "|" + String (vs.py, 3) + "|" + String (vs.pz, 3) + "|" + String (vs.size, 3)
                         + "|" + String ((int) vs.object) + "|" + String (vs.format) + "|" + String (vs.seat);
        if (key != covKey)
        {
            vs.gains = vs.model.panner (vs.format, vs.object, vs.px, vs.py, vs.pz, vs.size);
            vs.srcM = hall::panToMetres (vs.px, vs.py, vs.pz);
            if (tick - lastCovTick >= 4 || ! vs.fly)
            {
                vs.cov = vs.model.coverage (vs.gains, vs.srcM);
                ++vs.covVersion;
                vs.seatRes = vs.model.analyseSeat (vs.gains, vs.model.seats[(size_t) vs.seat], vs.srcM, true);
                lastCovTick = tick;
                covKey = key;
            }
        }
    }

    readMeters (dt);
    updateLoudness();

    // Trail of where the sound has been (shows it travelling from speaker to speaker)
    if (tick % 2 == 0)
    {
        if (vs.srcLevel > 0.02f)
            vs.trail.push_back ({ vs.srcM.x, vs.srcM.y, vs.srcM.z });
        else if (! vs.trail.empty())
            vs.trail.erase (vs.trail.begin());
        while (vs.trail.size() > 36) vs.trail.erase (vs.trail.begin());
    }

    refreshButtons();
    saveUi();
    if (tick % 15 == 0) root.repaint (0, 0, LW, 60);
    if (tab == 0) plan.repaint(); else hall3d.repaint();
    meters.repaint();
    const String sk = String (vs.px, 3) + String (vs.py, 3) + String (vs.pz, 3) + String (vs.size, 3) + String ((int) vs.trackSource) + String ((int) vs.object);
    if (sk != soundKey) { soundKey = sk; sound.repaint(); }
    if (tick % 3 == 0) seatPanel.repaint();
}
