#include "PluginEditor.h"

using namespace juce;

namespace
{
    Font F (float h, bool bold = false, bool italic = false)
    {
        Font f { FontOptions (h) };
        f.setBold (bold);
        f.setItalic (italic);
        return f;
    }

    Point<float> dirOf (float a) { return { std::sin (a), -std::cos (a) }; }

    const Colour kCream (0xffe8e2c8);
    const Colour kInk (0xff2a2418);
}

//==============================================================================
void TapeLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                        float start, float end, Slider& s)
{
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.f);
    const float d = jmin (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();
    const float R = d * 0.5f, kr = R * 0.66f;
    const float angle = start + pos * (end - start);

    // marcas da escala
    g.setColour (Colour (0xffcfcab8));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = start + (end - start) * (float) i / 10.f;
        const bool major = (i % 5 == 0);
        g.drawLine (Line<float> (c + dirOf (a) * (R * (major ? 0.80f : 0.84f)),
                                 c + dirOf (a) * (R * 0.95f)), major ? 2.5f : 1.6f);
    }

    // rotulos das pontas
    g.setFont (F (11.f, true));
    g.setColour (Colour (0xffa8a496));
    const String lo = s.getProperties().getWithDefault ("lo", "0").toString();
    const String hi = s.getProperties().getWithDefault ("hi", "10").toString();
    g.drawText (lo, Rectangle<float> (0.f, 0.f, 36.f, 14.f).withCentre (c + dirOf (start) * (R * 0.9f) + Point<float> (0.f, 12.f)),
                Justification::centred, false);
    g.drawText (hi, Rectangle<float> (0.f, 0.f, 36.f, 14.f).withCentre (c + dirOf (end) * (R * 0.9f) + Point<float> (0.f, 12.f)),
                Justification::centred, false);

    // sombra + corpo do knob
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (c.x - kr + 2.f, c.y - kr + 4.f, kr * 2.f, kr * 2.f);

    g.setGradientFill (ColourGradient (Colour (0xff4d4d54), c.x - kr * 0.4f, c.y - kr * 0.6f,
                                       Colour (0xff101013), c.x + kr * 0.7f, c.y + kr, true));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.f, kr * 2.f);
    g.setColour (Colour (0xff050506));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.f, kr * 2.f, 2.f);

    // indicador colorido
    Path p;
    p.addRoundedRectangle (c.x - 5.f, c.y - kr * 0.97f, 10.f, kr * 0.55f, 4.f);
    p.applyTransform (AffineTransform::rotation (angle, c.x, c.y));
    g.setColour (s.findColour (Slider::thumbColourId));
    g.fillPath (p);
    g.setColour (Colours::white.withAlpha (0.25f));
    g.strokePath (p, PathStrokeType (1.f));
}

void TapeLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float sliderPos,
                                        float minPos, float maxPos, Slider::SliderStyle, Slider&)
{
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    auto slot = b.reduced (0.f, b.getHeight() * 0.22f);

    g.setColour (Colour (0xff0c0c0e));
    g.fillRoundedRectangle (slot, 4.f);
    g.setColour (Colour (0xff55555c));
    g.drawRoundedRectangle (slot, 4.f, 1.f);

    g.setColour (Colour (0xff2c2c30));
    for (float tx : { minPos, (minPos + maxPos) * 0.5f, maxPos })
        g.drawLine (tx, slot.getY() + 3.f, tx, slot.getBottom() - 3.f, 1.f);

    const float hw = 22.f, hh = b.getHeight() * 0.88f;
    Rectangle<float> hnd (sliderPos - hw * 0.5f, b.getCentreY() - hh * 0.5f, hw, hh);
    g.setGradientFill (ColourGradient (Colour (0xff77777e), 0.f, hnd.getY(),
                                       Colour (0xff35353a), 0.f, hnd.getBottom(), false));
    g.fillRoundedRectangle (hnd, 3.f);
    g.setColour (Colour (0xff111113));
    g.drawRoundedRectangle (hnd, 3.f, 1.f);
    g.setColour (kCream);
    g.fillRect (sliderPos - 1.5f, hnd.getY() + 3.f, 3.f, hnd.getHeight() - 6.f);
}

//==============================================================================
TapeBoxEditor::TapeBoxEditor (TapeBoxProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setSize (920, 600);

    struct KD { const char* id; const char* name; uint32 col; const char* lo; const char* hi; };
    const KD kd[6] = {
        { "drive",  "DRIVE",    0xffe8a33d, "0",  "10" },
        { "color",  "COLOR",    0xffd6584a, "0",  "10" },
        { "wow",    "WOW/FLUT", 0xff4fb3a5, "0",  "10" },
        { "noise",  "NOISE",    0xff8c9bd6, "0",  "10" },
        { "tone",   "TONE",     0xffe8e2c8, "LO", "HI" },
        { "output", "OUTPUT",   0xffcf7bd1, "0",  "10" } };

    for (size_t i = 0; i < 6; ++i)
    {
        auto& k = knobs[i];
        k.name = kd[i].name;
        k.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        k.slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        k.slider.setColour (Slider::thumbColourId, Colour (kd[i].col));
        k.slider.getProperties().set ("lo", kd[i].lo);
        k.slider.getProperties().set ("hi", kd[i].hi);
        k.slider.setPopupDisplayEnabled (true, false, this);
        k.slider.setNumDecimalPlacesToDisplay (1);
        addAndMakeVisible (k.slider);
        k.att = std::make_unique<SliderAtt> (proc.apvts, kd[i].id, k.slider);

        if (auto* prm = proc.apvts.getParameter (kd[i].id))
            k.slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    }

    struct SD { const char* id; const char* name; StringArray labels; };
    const SD sd[3] = {
        { "age",  "AGE",  { "NEW", "USED", "WORN" } },
        { "tape", "TAPE", { "LOW", "STD", "HIGH" } },
        { "comp", "COMP", { "OFF", "LITE", "HEAVY" } } };

    for (size_t i = 0; i < 3; ++i)
    {
        auto& s = switches[i];
        s.name = sd[i].name;
        s.labels = sd[i].labels;
        s.slider.setSliderStyle (Slider::LinearHorizontal);
        s.slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (s.slider);
        s.att = std::make_unique<SliderAtt> (proc.apvts, sd[i].id, s.slider);
    }

    startTimerHz (30);
}

TapeBoxEditor::~TapeBoxEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void TapeBoxEditor::resized()
{
    for (int i = 0; i < 6; ++i)
    {
        const int col = i % 3, row = i / 3;
        const int x0 = 15 + col * 172, y0 = 138 + row * 165;
        knobs[(size_t) i].slider.setBounds (x0 + 16, y0 + 18, 140, 135);
    }
    for (int i = 0; i < 3; ++i)
    {
        const int x0 = 15 + i * 172;
        switches[(size_t) i].slider.setBounds (x0 + 16, 486, 140, 26);
    }
}

void TapeBoxEditor::timerCallback()
{
    auto upd = [] (float& cur, float target)
    {
        const float db = Decibels::gainToDecibels (target, -60.f);
        const float t = jlimit (0.f, 1.f, (db + 40.f) / 40.f);
        cur += (t - cur) * (t > cur ? 0.5f : 0.15f);
    };
    upd (vuL, proc.meterL.load());
    upd (vuR, proc.meterR.load());

    ++frame;
    const float wow = proc.apvts.getRawParameterValue ("wow")->load();
    reelAngle += 0.10f + (wow / 10.f) * 0.07f * std::sin ((float) frame * 0.21f);
    tapeProg += 0.0004f;
    if (tapeProg > 1.f) tapeProg -= 1.f;
    if (frame % 8 == 0) counter = (counter + 1) % 10000;

    repaint();
}

//==============================================================================
void TapeBoxEditor::drawVU (Graphics& g, Rectangle<float> r, float v, const String& label)
{
    auto housing = r.expanded (6.f);
    g.setColour (Colour (0xff0c0c0e));
    g.fillRoundedRectangle (housing, 8.f);
    g.setColour (Colour (0xff55555c));
    g.drawRoundedRectangle (housing, 8.f, 1.5f);
    g.setColour (Colour (0xffe9e2bb));
    g.fillRoundedRectangle (r, 4.f);

    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (r.toNearestInt());

    const Point<float> pivot (r.getCentreX(), r.getBottom() + r.getHeight() * 0.35f);
    const float radius = r.getHeight() * 1.1f;
    auto ang = [] (float t) { return -0.7f + 1.4f * t; };

    for (int i = 0; i <= 20; ++i)
    {
        const float a = ang ((float) i / 20.f);
        const bool major = (i % 5 == 0);
        g.setColour (i >= 17 ? Colour (0xffb3261e) : kInk);
        g.drawLine (Line<float> (pivot + dirOf (a) * (radius * (major ? 0.86f : 0.91f)),
                                 pivot + dirOf (a) * (radius * 0.97f)), major ? 2.f : 1.f);
    }

    Path arc, red;
    arc.addCentredArc (pivot.x, pivot.y, radius * 0.97f, radius * 0.97f, 0.f, ang (0.f), ang (0.85f), true);
    red.addCentredArc (pivot.x, pivot.y, radius * 0.97f, radius * 0.97f, 0.f, ang (0.85f), ang (1.f), true);
    g.setColour (kInk);
    g.strokePath (arc, PathStrokeType (1.5f));
    g.setColour (Colour (0xffb3261e));
    g.strokePath (red, PathStrokeType (4.f));

    g.setFont (F (11.f, true));
    g.setColour (kInk);
    const char* tl[5] = { "-40", "-30", "-20", "-10", "0" };
    for (int i = 0; i < 5; ++i)
    {
        auto pt = pivot + dirOf (ang ((float) i * 0.25f)) * (radius * 0.76f);
        g.drawText (tl[i], Rectangle<float> (0.f, 0.f, 30.f, 14.f).withCentre (pt), Justification::centred, false);
    }
    g.setFont (F (12.f, true));
    g.drawText ("OUT " + label, Rectangle<float> (r.getX(), r.getBottom() - 22.f, r.getWidth(), 16.f),
                Justification::centred, false);

    g.setColour (Colour (0xff151515));
    g.drawLine (Line<float> (pivot, pivot + dirOf (ang (v)) * (radius * 0.98f)), 2.f);

    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.22f), r.getX(), r.getY(),
                                       Colours::white.withAlpha (0.f), r.getX(), r.getY() + r.getHeight() * 0.5f, false));
    g.fillRect (r);
}

void TapeBoxEditor::drawReel (Graphics& g, Point<float> c, float wound, float angle)
{
    g.setColour (Colour (0xff3b2a21));
    g.fillEllipse (c.x - wound, c.y - wound, wound * 2.f, wound * 2.f);
    g.setColour (Colour (0xff5a4333));
    g.drawEllipse (c.x - wound, c.y - wound, wound * 2.f, wound * 2.f, 1.f);

    const float hub = 13.f;
    g.setColour (Colour (0xffe6e2d6));
    g.fillEllipse (c.x - hub, c.y - hub, hub * 2.f, hub * 2.f);
    const float in = hub * 0.62f;
    g.setColour (Colour (0xff17171a));
    g.fillEllipse (c.x - in, c.y - in, in * 2.f, in * 2.f);

    g.setColour (Colour (0xffe6e2d6));
    for (int i = 0; i < 6; ++i)
    {
        const float a = angle + (float) i * MathConstants<float>::pi / 3.f;
        g.drawLine (Line<float> (c + dirOf (a) * (in * 0.45f), c + dirOf (a) * in), 3.f);
    }
}

void TapeBoxEditor::drawCassette (Graphics& g, Rectangle<float> r)
{
    static const char* ageNames[3] = { "NEW", "USED", "WORN" };
    static const char* tapeNames[3] = { "LOW", "STD", "HIGH" };
    const int age = jlimit (0, 2, (int) proc.apvts.getRawParameterValue ("age")->load());
    const int tape = jlimit (0, 2, (int) proc.apvts.getRawParameterValue ("tape")->load());

    g.setColour (Colour (0xff161618));
    g.fillRoundedRectangle (r, 12.f);
    g.setColour (Colour (0xff4a4a50));
    g.drawRoundedRectangle (r, 12.f, 2.f);

    auto inner = r.reduced (12.f);
    auto label = inner.removeFromTop (r.getHeight() * 0.46f);

    const Colour labelCols[3] = { Colour (0xfff2ead0), Colour (0xffdcae6a), Colour (0xffb07445) };
    g.setColour (labelCols[age]);
    g.fillRoundedRectangle (label, 6.f);
    g.setColour (Colour (0xffc2452f));
    g.fillRect (label.withTop (label.getBottom() - 8.f));

    g.setColour (Colour (0xff3a2a1c));
    g.setFont (F (11.f, true));
    g.drawText ("SIDE A", label.reduced (8.f, 5.f), Justification::topLeft, false);
    g.setFont (F (26.f, true, true));
    g.drawText ("TAPEBOX", label, Justification::centred, false);
    g.setFont (F (12.f, true));
    g.drawText (String (ageNames[age]) + "  /  " + tapeNames[tape] + " BIAS",
                label.withTrimmedBottom (12.f).reduced (8.f, 4.f), Justification::bottomLeft, false);

    // janela com os carreteis
    auto win = inner.reduced (26.f, 8.f);
    g.setColour (Colour (0xff09090b));
    g.fillRoundedRectangle (win, 10.f);

    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (win.toNearestInt());
        const Point<float> cl (win.getX() + win.getWidth() * 0.27f, win.getCentreY());
        const Point<float> cr (win.getX() + win.getWidth() * 0.73f, win.getCentreY());
        g.setColour (Colour (0xff4a3426));
        g.drawLine (cl.x, win.getBottom() - 6.f, cr.x, win.getBottom() - 6.f, 3.f);
        drawReel (g, cl, 14.f + 16.f * (1.f - tapeProg), reelAngle);
        drawReel (g, cr, 14.f + 16.f * tapeProg, reelAngle);
    }

    g.setColour (Colour (0xff3a3a3f));
    for (auto pt : { r.getTopLeft(), r.getTopRight(), r.getBottomLeft(), r.getBottomRight() })
    {
        const auto c = pt + Point<float> (pt.x < r.getCentreX() ? 8.f : -8.f, pt.y < r.getCentreY() ? 8.f : -8.f);
        g.fillEllipse (c.x - 4.f, c.y - 4.f, 8.f, 8.f);
    }
}

void TapeBoxEditor::paint (Graphics& g)
{
    g.setGradientFill (ColourGradient (Colour (0xff2b2b2e), 0.f, 0.f,
                                       Colour (0xff17171a), 0.f, (float) getHeight(), false));
    g.fillAll();

    // VU meters
    drawVU (g, { 20.f, 14.f, 245.f, 105.f }, vuL, "L");
    drawVU (g, { 285.f, 14.f, 245.f, 105.f }, vuR, "R");

    // titulo + contador
    g.setColour (kCream);
    g.setFont (F (44.f, true));
    g.drawText ("TAPEBOX", Rectangle<float> (565.f, 14.f, 345.f, 52.f), Justification::centredLeft, false);
    g.setFont (F (12.f, true));
    g.setColour (Colour (0xffa8a496));
    g.drawText ("TAPE SATURATION  /  LO-FI", Rectangle<float> (567.f, 66.f, 240.f, 18.f), Justification::centredLeft, false);

    Rectangle<float> cnt (815.f, 74.f, 85.f, 30.f);
    g.setColour (Colour (0xff070708));
    g.fillRoundedRectangle (cnt, 4.f);
    g.setColour (Colour (0xff55555c));
    g.drawRoundedRectangle (cnt, 4.f, 1.f);
    g.setColour (Colour (0xffe8e2c8));
    g.setFont (F (20.f, true));
    g.drawText (String::formatted ("%04d", counter), cnt, Justification::centred, false);

    // painel esquerdo
    Rectangle<float> panel (10.f, 130.f, 530.f, 412.f);
    g.setColour (Colour (0xff35353a));
    g.fillRoundedRectangle (panel, 10.f);
    g.setColour (Colours::white.withAlpha (0.025f));
    for (float yy = panel.getY() + 2.f; yy < panel.getBottom() - 2.f; yy += 3.f)
        g.drawHorizontalLine ((int) yy, panel.getX() + 6.f, panel.getRight() - 6.f);
    g.setColour (Colour (0xff4a4a50));
    g.drawRoundedRectangle (panel, 10.f, 1.5f);

    g.setColour (Colour (0xffd8d4c4));
    g.setFont (F (13.f, true));
    for (auto& k : knobs)
    {
        auto b = k.slider.getBounds();
        g.drawText (k.name, b.getX(), b.getY() - 18, b.getWidth(), 16, Justification::centred, false);
    }

    g.setFont (F (12.f, true));
    for (auto& s : switches)
    {
        auto b = s.slider.getBounds();
        const float xs[3] = { (float) b.getX() + 11.f, (float) b.getCentreX(), (float) b.getRight() - 11.f };
        for (int i = 0; i < 3; ++i)
            g.drawText (s.labels[i], Rectangle<float> (xs[i] - 30.f, (float) b.getY() - 19.f, 60.f, 16.f),
                        Justification::centred, false);
        g.setFont (F (14.f, true));
        g.drawText (s.name, b.getX(), b.getBottom() + 4, b.getWidth(), 18, Justification::centred, false);
        g.setFont (F (12.f, true));
    }

    // deck da fita
    g.setColour (Colour (0xff222225));
    g.fillRoundedRectangle (555.f, 130.f, 355.f, 380.f, 10.f);
    g.setColour (Colour (0xff4a4a50));
    g.drawRoundedRectangle (555.f, 130.f, 355.f, 380.f, 10.f, 1.5f);
    drawCassette (g, { 575.f, 185.f, 315.f, 210.f });

    // botoes decorativos
    const char* names[6] = { "REC", "PLAY", "REW", "FF", "PAUSE", "STOP" };
    for (int i = 0; i < 6; ++i)
    {
        Rectangle<float> r (565.f + (float) i * 56.f, 522.f, 50.f, 50.f);
        const bool pressed = (i == 1);
        g.setGradientFill (ColourGradient (pressed ? Colour (0xff1c1c1f) : Colour (0xff4a4a50), 0.f, r.getY(),
                                           pressed ? Colour (0xff111113) : Colour (0xff222225), 0.f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 5.f);
        g.setColour (Colour (0xff0b0b0c));
        g.drawRoundedRectangle (r, 5.f, 1.2f);
        g.setColour (Colour (0xffd8d4c4));
        g.setFont (F (10.f, true));
        g.drawText (names[i], r.withTrimmedTop (8.f).withHeight (14.f), Justification::centred, false);
        if (i == 0)
        {
            g.setColour (Colour (0xffd6392b));
            g.fillEllipse (r.getCentreX() - 5.f, r.getY() + 28.f, 10.f, 10.f);
        }
    }
}
