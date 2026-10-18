#include "PluginEditor.h"

using namespace juce;

namespace
{
    // L'interface est dessinée dans un espace de 615 x 620 puis agrandie
    constexpr float kScale = 1.35f;
    constexpr float kW = 615.f, kH = 620.f;

    const Colour kOrange (0xffff9a3c);
    const Colour kRed    (0xffc0202a);
    const Colour kGreen  (0xff2e9a47);
    const Colour kBlue   (0xff2a6fc9);
    const Colour kBlack  (0xff222426);
    const Colour kWhite  (0xffe9e9e9);
    const Colour kAmber  (0xffe0b050);

    Rectangle<int> scaled (float x, float y, float w, float h)
    {
        return { roundToInt (x * kScale), roundToInt (y * kScale), roundToInt (w * kScale), roundToInt (h * kScale) };
    }

    String infinity() { return String::fromUTF8 ("\xe2\x88\x9e"); }
}

// ===========================================================================
// Look & feel
// ===========================================================================
ConsoleLnF::ConsoleLnF()
{
    setColour (ComboBox::backgroundColourId, Colour (0xff1e2022));
    setColour (ComboBox::textColourId, Colours::white);
    setColour (ComboBox::outlineColourId, Colour (0xff6b6e72));
    setColour (ComboBox::arrowColourId, Colours::white);
    setColour (PopupMenu::backgroundColourId, Colour (0xff1e2022));
    setColour (PopupMenu::textColourId, Colours::white);
    setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff3d6aa3));
    setColour (Slider::textBoxTextColourId, Colour (0xff6fe8a8));
    setColour (Slider::textBoxBackgroundColourId, Colour (0xff0b1a14));
    setColour (Slider::textBoxOutlineColourId, Colour (0xff3c5b4e));
    setColour (Slider::backgroundColourId, Colour (0xff0b1a14));
    setColour (Slider::trackColourId, Colour (0xff1d4d3d));
    setColour (TextButton::buttonColourId, Colour (0xff2d2f31));
    setColour (TextButton::buttonOnColourId, kOrange);
    setColour (TextButton::textColourOffId, Colours::white);
    setColour (TextButton::textColourOnId, Colours::black);
}

void ConsoleLnF::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                   float a0, float a1, Slider& s)
{
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.f);
    const float d = jmin (b.getWidth(), b.getHeight());
    const auto c = Rectangle<float> (d, d).withCentre (b.getCentre()).getCentre();
    const float R = d * 0.5f;
    const float ang = a0 + pos * (a1 - a0);

    // repères autour du potard
    g.setColour (Colours::white.withAlpha (0.55f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = a0 + (a1 - a0) * (float) i / 10.f;
        const float rr = R * 0.97f;
        g.fillEllipse (c.x + std::sin (a) * rr - 1.f, c.y - std::cos (a) * rr - 1.f, 2.f, 2.f);
    }

    // capuchon
    const float cr = R * 0.72f;
    auto cap = Rectangle<float> (cr * 2.f, cr * 2.f).withCentre (c);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (cap.translated (0.f, 1.5f).expanded (1.f));

    const Colour col = s.findColour (Slider::thumbColourId);
    g.setGradientFill (ColourGradient (col.brighter (0.35f), c.x - cr * 0.5f, c.y - cr * 0.6f,
                                       col.darker (0.5f), c.x + cr * 0.6f, c.y + cr * 0.8f, true));
    g.fillEllipse (cap);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawEllipse (cap, 1.f);

    // aiguille
    g.setColour (col.getBrightness() > 0.6f ? Colours::black : Colours::white);
    Path p;
    p.addRoundedRectangle (-1.3f, -cr, 2.6f, cr * 0.55f, 1.f);
    g.fillPath (p, AffineTransform::rotation (ang).translated (c.x, c.y));
}

void ConsoleLnF::drawToggleButton (Graphics& g, ToggleButton& b, bool, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const float sz = r.getHeight();
    auto sq = r.removeFromRight (sz).reduced (1.5f);
    r.removeFromRight (3.f);

    g.setColour (Colours::white.withAlpha (0.88f));
    g.setFont (Font (FontOptions (sz * 0.6f, Font::bold)));
    g.drawText (b.getButtonText(), r, Justification::centredRight, false);

    const bool on = b.getToggleState();
    const Colour led = b.getComponentID() == "cut" ? Colour (0xffe0402a) : kOrange;

    if (on)
    {
        g.setColour (led.withAlpha (0.30f));
        g.fillRoundedRectangle (sq.expanded (2.f), 3.f);
    }
    g.setGradientFill (ColourGradient (on ? led.brighter (0.3f) : Colour (0xffa4a7aa), sq.getX(), sq.getY(),
                                       on ? led.darker (0.2f) : Colour (0xff6c6f72), sq.getX(), sq.getBottom(), false));
    g.fillRoundedRectangle (sq, 2.f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (sq, 2.f, 1.f);
}

void ConsoleLnF::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos,
                                   float minPos, float maxPos, Slider::SliderStyle style, Slider& s)
{
    if (style != Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, s);
        return;
    }

    // fader de console
    const float cx = (float) x + (float) w * 0.5f;
    g.setColour (Colour (0xff0d0e0f));
    g.fillRoundedRectangle (cx - 2.5f, (float) y + 4.f, 5.f, (float) h - 8.f, 2.5f);

    const float cw = (float) w * 0.92f, ch = (float) w * 0.62f;
    Rectangle<float> cap (cx - cw * 0.5f, pos - ch * 0.5f, cw, ch);
    g.setGradientFill (ColourGradient (Colour (0xffbfc1c3), cap.getX(), cap.getY(),
                                       Colour (0xff6f7173), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 2.f);
    g.setColour (Colours::black.withAlpha (0.55f));
    for (int i = -2; i <= 2; ++i)
        g.fillRect (cap.getX() + 2.f, pos + (float) i * ch * 0.18f, cap.getWidth() - 4.f, 1.f);
    g.setColour (Colours::white.withAlpha (0.9f));
    g.fillRect (cap.getX() + 2.f, pos - 0.5f, cap.getWidth() - 4.f, 1.5f);
}

// ===========================================================================
// ConsoleKnob
// ===========================================================================
ConsoleKnob::ConsoleKnob (const String& n, const String& mn, const String& mx, Colour cap)
    : name (n), minText (mn), maxText (mx)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
    slider.setColour (Slider::thumbColourId, cap);
    slider.setPopupDisplayEnabled (true, true, nullptr);
    addAndMakeVisible (slider);
}

void ConsoleKnob::resized()
{
    auto r = getLocalBounds();
    const int h = getHeight();
    r.removeFromTop ((int) (h * 0.18f));
    r.removeFromBottom ((int) (h * 0.16f));
    const int d = jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (d, d));
}

void ConsoleKnob::paint (Graphics& g)
{
    const float h = (float) getHeight();
    g.setColour (Colours::white.withAlpha (0.9f));
    g.setFont (Font (FontOptions (h * 0.16f, Font::bold)));
    g.drawText (name, 0, 0, getWidth(), (int) (h * 0.18f), Justification::centred, false);

    g.setColour (Colours::white.withAlpha (0.6f));
    g.setFont (Font (FontOptions (h * 0.13f)));
    const int y = (int) (h * 0.84f), hh = (int) (h * 0.16f);
    g.drawText (minText, 0, y, getWidth() / 2, hh, Justification::centredLeft, false);
    g.drawText (maxText, getWidth() / 2, y, getWidth() / 2, hh, Justification::centredRight, false);
}

// ===========================================================================
// Éditeur
// ===========================================================================
void ChannelStripEditor::addKnob (const String& id, const String& name, const String& mn,
                                  const String& mx, Colour cap, float cx, float cy)
{
    KnobEntry e;
    e.knob = std::make_unique<ConsoleKnob> (name, mn, mx, cap);
    addAndMakeVisible (*e.knob);
    e.att = std::make_unique<APVTS::SliderAttachment> (proc.apvts, id, e.knob->slider);
    if (auto* par = proc.apvts.getParameter (id))
        e.knob->slider.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
    e.centre = { cx, cy };
    knobs.push_back (std::move (e));
}

void ChannelStripEditor::addToggle (const String& id, const String& text, float x, float y, float w, float h)
{
    ToggleEntry e;
    e.btn = std::make_unique<ToggleButton> (text);
    e.btn->setComponentID (id);
    addAndMakeVisible (*e.btn);
    e.att = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id, *e.btn);
    e.rect = { x, y, w, h };
    toggles.push_back (std::move (e));
}

ChannelStripEditor::ChannelStripEditor (ChannelStripProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    // ---- Filtres / préampli (colonne gauche, haut)
    addKnob ("lp", "LP", "3k", "out", kWhite, 41.f, 34.f);
    addKnob ("hp", "HP", "out", "500", kWhite, 112.f, 70.f);
    addKnob ("preamp", "PREAMP", "0", "10", kAmber, 41.f, 100.f);

    // ---- EQ (colonne gauche, bas)
    addKnob ("hfGain", "HF", "-", "+", kRed, 43.f, 191.f);
    addKnob ("hfFreq", "FREQ", "1.5", "22k", kRed, 109.f, 228.f);
    addKnob ("hmfGain", "HMF", "-", "+", kGreen, 43.f, 267.f);
    addKnob ("hmfFreq", "FREQ", "0.6", "7.0", kGreen, 109.f, 299.f);
    addKnob ("hmfQ", "Q", "lo", "hi", kGreen, 43.f, 330.f);
    addKnob ("lmfGain", "LMF", "-", "+", kBlue, 43.f, 403.f);
    addKnob ("lmfFreq", "FREQ", "0.2", "2.0", kBlue, 109.f, 434.f);
    addKnob ("lmfQ", "Q", "lo", "hi", kBlue, 43.f, 470.f);
    addKnob ("lfFreq", "FREQ", "40", "600", kBlack, 109.f, 513.f);
    addKnob ("lfGain", "LF", "-", "+", kBlack, 43.f, 541.f);

    // ---- Centre
    addKnob ("inTrim", "IN TRIM", "-20", "+20", kWhite, 273.f, 110.f);
    addKnob ("width", "WIDTH", "0", "2", kBlue, 275.f, 214.f);
    addKnob ("pan", "PAN", "L", "R", kBlue, 338.f, 213.f);

    // ---- Compresseur
    addKnob ("compRatio", "RATIO", "1.0", infinity(), kWhite, 505.f, 85.f);
    addKnob ("compThr", "THRESH", "-20", "+10", kWhite, 570.f, 119.f);
    addKnob ("compRel", "RELEASE", "0.1", "4.0", kWhite, 505.f, 153.f);

    // ---- Gate / expander
    addKnob ("gateRange", "RANGE", "0", "40", kWhite, 504.f, 253.f);
    addKnob ("gateThr", "THRESH", "-30", "+10", kWhite, 570.f, 286.f);
    addKnob ("gateRel", "RELEASE", "0.1", "4.0", kWhite, 504.f, 320.f);
    addKnob ("gateHold", "HOLD", "0", "4.0", kWhite, 570.f, 355.f);

    // ---- Boutons
    addToggle ("power",      "",         168.f, 6.f,   18.f, 18.f);
    addToggle ("phase",      "PHASE",    318.f, 114.f, 60.f, 16.f);
    addToggle ("cut",        "CUT",      252.f, 316.f, 60.f, 18.f);
    addToggle ("hfBell",     "BELL",     82.f,  172.f, 64.f, 16.f);
    addToggle ("eqE",        "E",        82.f,  349.f, 64.f, 16.f);
    addToggle ("eqOn",       "EQ ON",    82.f,  380.f, 64.f, 16.f);
    addToggle ("lfBell",     "BELL",     82.f,  553.f, 64.f, 16.f);
    addToggle ("compFast",   "FAST ATK", 538.f, 32.f,  72.f, 16.f);
    addToggle ("compPeak",   "PEAK",     538.f, 52.f,  72.f, 16.f);
    addToggle ("dynOn",      "DYN",      470.f, 192.f, 62.f, 16.f);
    addToggle ("gateExpand", "EXPAND",   470.f, 376.f, 66.f, 16.f);
    addToggle ("gateFast",   "FAST ATK", 470.f, 398.f, 66.f, 16.f);

    // ---- Sélecteurs de modèle : un par section
    for (auto* box : { &preBox, &eqBox, &dynBox })
    {
        box->addItemList ({ "SSL", "NEVE", "API" }, 1);
        box->onChange = [this] { repaint(); };
        addAndMakeVisible (*box);
    }
    preAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "preModel", preBox);
    eqAtt  = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "eqModel", eqBox);
    dynAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "dynModel", dynBox);

    // ---- Fader, mix, out trim
    fader.setSliderStyle (Slider::LinearVertical);
    fader.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    fader.setPopupDisplayEnabled (true, true, nullptr);
    addAndMakeVisible (fader);
    faderAtt = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "fader", fader);
    fader.setDoubleClickReturnValue (true, 0.0);

    for (auto* bar : { &mixBar, &outTrimBar })
    {
        bar->setSliderStyle (Slider::LinearBar);
        bar->setTextBoxStyle (Slider::TextBoxLeft, false, 50, 18);
        addAndMakeVisible (*bar);
    }
    mixAtt     = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "mix", mixBar);
    outTrimAtt = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "outTrim", outTrimBar);
    mixBar.setDoubleClickReturnValue (true, 1.0);
    outTrimBar.setDoubleClickReturnValue (true, 0.0);

    // ---- Pied de page : undo / redo / A-B
    for (auto* t : { &undoBtn, &redoBtn, &aBtn, &bBtn }) addAndMakeVisible (*t);
    undoBtn.onClick = [this] { proc.undoManager.undo(); };
    redoBtn.onClick = [this] { proc.undoManager.redo(); };
    for (auto* t : { &aBtn, &bBtn })
    {
        t->setRadioGroupId (1);
        t->setClickingTogglesState (true);
    }
    aBtn.setToggleState (proc.getActiveSlot() == 0, dontSendNotification);
    bBtn.setToggleState (proc.getActiveSlot() == 1, dontSendNotification);
    aBtn.onClick = [this] { proc.selectSlot (0); };
    bBtn.onClick = [this] { proc.selectSlot (1); };

    // ---- Zone cliquable de l'écran d'ordre du signal (EQ avant / après dynamique)
    orderBtn.setClickingTogglesState (true);
    orderBtn.setColour (TextButton::buttonColourId, Colours::transparentBlack);
    orderBtn.setColour (TextButton::buttonOnColourId, Colours::transparentBlack);
    orderBtn.setColour (ComboBox::outlineColourId, Colours::transparentBlack);
    addAndMakeVisible (orderBtn);
    orderAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "eqFirst", orderBtn);
    orderBtn.onClick = [this] { repaint(); };

    setSize (roundToInt (kW * kScale), roundToInt (kH * kScale));
    startTimerHz (30);
}

ChannelStripEditor::~ChannelStripEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ChannelStripEditor::resized()
{
    for (auto& k : knobs)
        k.knob->setBounds (scaled (k.centre.x - 28.f, k.centre.y - 27.f, 56.f, 58.f));
    for (auto& t : toggles)
        t.btn->setBounds (scaled (t.rect.getX(), t.rect.getY(), t.rect.getWidth(), t.rect.getHeight()));

    preBox.setBounds (scaled (84.f, 6.f, 66.f, 22.f));
    eqBox.setBounds (scaled (8.f, 139.f, 64.f, 22.f));
    dynBox.setBounds (scaled (470.f, 32.f, 66.f, 20.f));

    fader.setBounds (scaled (318.f, 248.f, 38.f, 300.f));
    mixBar.setBounds (scaled (540.f, 7.f, 50.f, 18.f));
    outTrimBar.setBounds (scaled (248.f, 562.f, 56.f, 18.f));

    undoBtn.setBounds (scaled (6.f, 598.f, 38.f, 18.f));
    redoBtn.setBounds (scaled (46.f, 598.f, 38.f, 18.f));
    aBtn.setBounds (scaled (108.f, 598.f, 20.f, 18.f));
    bBtn.setBounds (scaled (130.f, 598.f, 20.f, 18.f));

    orderBtn.setBounds (scaled (470.f, 436.f, 137.f, 140.f));
}

void ChannelStripEditor::timerCallback()
{
    for (int i = 0; i < 2; ++i)
    {
        inM[i]  = jmax (proc.inPeak[i].load(),  inM[i]  * 0.88f);
        outM[i] = jmax (proc.outPeak[i].load(), outM[i] * 0.88f);
    }
    grM = jmax (proc.grMeter.load(), grM * 0.85f);

    repaint (scaled (176.f, 58.f, 42.f, 521.f));
    repaint (scaled (396.f, 58.f, 66.f, 521.f));
    repaint (scaled (550.f, 170.f, 60.f, 64.f));
}

void ChannelStripEditor::paint (Graphics& g)
{
    g.fillAll (Colour (0xff131415));
    g.addTransform (AffineTransform::scale (kScale));

    auto panelColour = [] (int m)
    {
        return m == 0 ? Colour (0xff57524d)       // SSL : gris chaud
             : m == 1 ? Colour (0xff35567f)       // Neve : bleu
                      : Colour (0xff3b3e42);      // API : gris sombre
    };
    auto fillPanel = [&g] (float x, float y, float w, float h, Colour c)
    {
        g.setGradientFill (ColourGradient (c.brighter (0.12f), x, y, c.darker (0.30f), x, y + h, false));
        g.fillRect (x, y, w, h);
    };
    auto text = [&g] (const String& t, float x, float y, float w, float h, float size, Justification j,
                      Colour c = Colours::white, int style = Font::plain)
    {
        g.setColour (c);
        g.setFont (Font (FontOptions (size, style)));
        g.drawText (t, Rectangle<float> (x, y, w, h), j, false);
    };

    const Colour cPre = panelColour (preBox.getSelectedItemIndex());
    const Colour cEq  = panelColour (eqBox.getSelectedItemIndex());
    const Colour cDyn = panelColour (dynBox.getSelectedItemIndex());

    // ---- Panneaux
    fillPanel (0.f, 0.f, 153.f, 136.f, cPre);
    fillPanel (0.f, 136.f, 153.f, 457.f, cEq);
    fillPanel (153.f, 0.f, 309.f, 593.f, Colour (0xff2c2d2f));
    fillPanel (462.f, 0.f, 153.f, 424.f, cDyn);
    fillPanel (462.f, 424.f, 153.f, 169.f, cDyn.darker (0.35f));

    g.setColour (Colours::black.withAlpha (0.4f));
    g.fillRect (0.f, 136.f, 153.f, 1.f);
    g.fillRect (152.f, 0.f, 2.f, 593.f);
    g.fillRect (461.f, 0.f, 2.f, 593.f);
    g.fillRect (462.f, 424.f, 153.f, 1.f);

    // ---- Barre du haut (centre)
    g.setGradientFill (ColourGradient (Colour (0xff77797b), 0.f, 0.f, Colour (0xff4b4d4f), 0.f, 28.f, false));
    g.fillRect (153.f, 0.f, 309.f, 28.f);
    g.setColour (Colour (0xff0b1a14));
    g.fillRoundedRectangle (213.f, 6.f, 189.f, 18.f, 2.f);
    text ("001", 218.f, 6.f, 28.f, 18.f, 11.f, Justification::centredLeft, Colour (0xffe03030), Font::bold);
    text ("CONSOLE STRIP", 250.f, 6.f, 150.f, 18.f, 10.f, Justification::centredLeft, Colour (0xff6fe8a8), Font::bold);

    // ---- Titres des sections
    text ("FILTERS", 80.f, 106.f, 70.f, 14.f, 8.5f, Justification::centredRight, Colours::white, Font::bold);
    text ("EQUALIZER", 78.f, 139.f, 70.f, 22.f, 8.5f, Justification::centredRight, Colours::white, Font::bold);
    text ("COMPRESSOR", 470.f, 6.f, 70.f, 20.f, 8.f, Justification::centredLeft, Colours::white, Font::bold);
    text ("MIX", 592.f, 7.f, 22.f, 18.f, 7.5f, Justification::centredLeft, Colours::white.withAlpha (0.8f), Font::bold);
    text ("GATE / EXPANDER", 540.f, 404.f, 72.f, 14.f, 6.5f, Justification::centredRight, Colours::white, Font::bold);

    // ---- Panneau central
    g.setColour (Colour (0xff39393b));
    g.fillRoundedRectangle (224.f, 38.f, 166.f, 547.f, 4.f);
    text ("CONSOLE", 224.f, 46.f, 166.f, 12.f, 7.5f, Justification::centred, Colours::white.withAlpha (0.55f));
    text ("Channel Strip", 224.f, 58.f, 166.f, 22.f, 13.f, Justification::centred, Colour (0xffdddddd), Font::bold | Font::italic);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRect (224.f, 163.f, 166.f, 1.f);
    text ("OUT TRIM", 248.f, 548.f, 56.f, 12.f, 7.5f, Justification::centred, Colours::white.withAlpha (0.8f), Font::bold);

    // ---- Vumètres (segments)
    auto meter = [&g] (float x, float top, float h, float lvl)
    {
        const int segs = 120;
        const float segH = h / (float) segs;
        const float frac = jlimit (0.f, 1.f, (Decibels::gainToDecibels (lvl, -60.f) + 60.f) / 60.f);
        const int lit = (int) (frac * (float) segs);
        for (int s = 0; s < segs; ++s)
        {
            const float db = -60.f + 60.f * (float) s / (float) segs;
            g.setColour (s < lit ? (db > -3.f ? Colour (0xffff3b2f) : Colour (0xffee6a2c)) : Colour (0xff2a1511));
            g.fillRect (x, top + h - (float) (s + 1) * segH + 0.4f, 15.f, segH - 0.8f);
        }
    };
    g.setColour (Colour (0xff0e0f10));
    g.fillRoundedRectangle (176.f, 58.f, 42.f, 521.f, 3.f);
    g.fillRoundedRectangle (396.f, 58.f, 41.f, 521.f, 3.f);
    meter (180.f, 62.f, 513.f, inM[0]);
    meter (198.f, 62.f, 513.f, inM[1]);
    meter (400.f, 62.f, 513.f, outM[0]);
    meter (418.f, 62.f, 513.f, outM[1]);

    for (int n = 0; n <= 60; n += 6)
    {
        const float y = 62.f + (float) n / 60.f * 513.f;
        const Colour c = n <= 12 ? Colour (0xffe84a3a) : Colour (0xff5ec26a);
        text (String (n), 148.f, y - 5.f, 25.f, 10.f, 7.f, Justification::centredRight, c);
        text (String (n), 440.f, y - 5.f, 21.f, 10.f, 7.f, Justification::centredLeft, c);
    }

    // ---- Graduation du fader
    {
        const float dbs[10] = { 12.f, 6.f, 0.f, -5.f, -10.f, -20.f, -30.f, -40.f, -50.f, -60.f };
        const String labels[10] = { "+12", "6", "0", "5", "10", "20", "30", "40", "50", infinity() };
        for (int i = 0; i < 10; ++i)
        {
            const float y = ((float) fader.getY() + fader.getPositionOfValue (dbs[i])) / kScale;
            g.setColour (Colours::white.withAlpha (0.5f));
            g.fillRect (357.f, y, 4.f, 1.f);
            text (labels[i], 362.f, y - 5.f, 26.f, 10.f, 7.5f, Justification::centredLeft, Colours::white.withAlpha (0.85f));
        }
    }

    // ---- LEDs de réduction de gain
    {
        const float thr[5] = { 20.f, 14.f, 10.f, 6.f, 3.f };
        for (int i = 0; i < 5; ++i)
        {
            const float y = 177.f + 12.f * (float) i;
            const bool lit = grM >= thr[i];
            const Colour c = i < 2 ? Colour (0xffe03030) : kOrange;
            if (lit)
            {
                g.setColour (c.withAlpha (0.35f));
                g.fillEllipse (554.f, y - 6.f, 12.f, 12.f);
            }
            g.setColour (lit ? c : c.darker (0.85f));
            g.fillEllipse (556.f, y - 3.5f, 7.f, 7.f);
            text (String ((int) thr[i]), 566.f, y - 5.f, 18.f, 10.f, 7.f, Justification::centredLeft, Colours::white.withAlpha (0.8f));
        }
    }

    // ---- Écran d'ordre du signal
    {
        const bool eqFirst = proc.apvts.getRawParameterValue ("eqFirst")->load() > 0.5f;
        g.setColour (Colour (0xff0a1f18));
        g.fillRoundedRectangle (470.f, 436.f, 137.f, 140.f, 4.f);
        g.setColour (Colour (0xff3c5b4e));
        g.drawRoundedRectangle (470.f, 436.f, 137.f, 140.f, 4.f, 1.f);
        text ("SIGNAL FLOW", 470.f, 440.f, 137.f, 12.f, 7.f, Justification::centred, Colour (0xff6fe8a8).withAlpha (0.7f), Font::bold);

        const String order[3] = { "FILTERS", eqFirst ? "EQ" : "DYN", eqFirst ? "DYN" : "EQ" };
        for (int i = 0; i < 3; ++i)
        {
            const float y = 458.f + 40.f * (float) i;
            g.setColour (Colour (0xff17382c));
            g.fillRoundedRectangle (490.f, y, 97.f, 22.f, 3.f);
            text (order[i], 490.f, y, 97.f, 22.f, 9.f, Justification::centred, Colour (0xff6fe8a8), Font::bold);
            if (i < 2)
            {
                Path arrow;
                const float ax = 538.5f, ay = y + 26.f;
                arrow.addTriangle (ax - 4.f, ay, ax + 4.f, ay, ax, ay + 8.f);
                g.setColour (Colour (0xff6fe8a8));
                g.fillPath (arrow);
            }
        }
        text ("CLICK TO SWAP EQ / DYN", 470.f, 562.f, 137.f, 12.f, 6.5f, Justification::centred, Colour (0xff6fe8a8).withAlpha (0.6f));
    }

    // ---- Pied de page
    g.setColour (Colour (0xff1c1d1f));
    g.fillRect (0.f, 593.f, kW, 27.f);
    text ("PRE " + preBox.getText() + "   |   EQ " + eqBox.getText() + "   |   DYN " + dynBox.getText(),
          153.f, 593.f, 309.f, 27.f, 9.f, Justification::centred, Colour (0xff6fe8a8), Font::bold);
    text ("v0.2", 560.f, 593.f, 50.f, 27.f, 8.f, Justification::centredRight, Colours::white.withAlpha (0.4f));
}
