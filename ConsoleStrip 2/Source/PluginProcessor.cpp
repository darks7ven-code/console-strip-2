#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;
using Coef = dsp::IIR::Coefficients<float>;

// ---------------------------------------------------------------------------
// Waveshapers : un caractère par modèle (pente 1 à l'origine)
// ---------------------------------------------------------------------------
static inline float shaper (int model, float x)
{
    switch (model)
    {
        case SSL:  return std::tanh (x);                               // propre, symétrique
        case NEVE: { const float b = 0.2f;                             // asymétrique : harmoniques paires
                     const float t = std::tanh (b);
                     return (std::tanh (x + b) - t) / (1.f - t * t); }
        default:   return x / std::cbrt (1.f + std::abs (x * x * x));  // API : coude plus franc
    }
}

// Gain qui garde le même niveau RMS (signal de référence ~ -14 dBFS) : le préampli colore sans "monter" le volume
static float satCompensation (int model, float k)
{
    double in2 = 0.0, out2 = 0.0;
    for (int i = 0; i < 64; ++i)
    {
        const float x = 0.2f * std::sin (MathConstants<float>::twoPi * (float) i / 64.f);
        const float y = shaper (model, x * k);
        in2 += (double) x * x;
        out2 += (double) y * y;
    }
    return out2 > 1.0e-9 ? (float) std::sqrt (in2 / out2) : 1.f;
}

static float snapTo (float f, std::initializer_list<float> list)
{
    float best = *list.begin();
    for (float s : list) if (std::abs (s - f) < std::abs (best - f)) best = s;
    return best;
}

// ---------------------------------------------------------------------------
// Paramètres
// ---------------------------------------------------------------------------
AudioProcessorValueTreeState::ParameterLayout ChannelStripProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto strFn = [] (int u) -> std::function<String (float, int)>
    {
        switch (u)
        {
            case 1:  return [] (float v, int) { return String (v, 1) + " dB"; };
            case 2:  return [] (float v, int) { return v >= 1000.f ? String (v / 1000.f, 2) + " kHz" : String (v, 0) + " Hz"; };
            case 3:  return [] (float v, int) { return String (v, 2) + " s"; };
            case 4:  return [] (float v, int) { return String (v * 100.f, 1) + " %"; };
            case 5:  return [] (float v, int) { return v >= 19.9f ? String (CharPointer_UTF8 ("\xe2\x88\x9e")) + ":1" : String (v, 1) + ":1"; };
            case 6:  return [] (float v, int) { return v <= -59.9f ? String ("-inf") : String (v, 1) + " dB"; };
            default: return [] (float v, int) { return String (v, 2); };
        }
    };

    // unit : 0 aucun, 1 dB, 2 Hz, 3 s, 4 %, 5 ratio
    auto f = [&] (const String& id, const String& name, float lo, float hi, float def, float skew, int unit)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                     NormalisableRange<float> (lo, hi, 0.01f, skew), def,
                     AudioParameterFloatAttributes().withStringFromValueFunction (strFn (unit))));
    };
    auto model = [&] (const String& id, const String& name)
    {
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name,
                     StringArray { "SSL", "NEVE", "API" }, 0));
    };
    auto b = [&] (const String& id, const String& name, bool def)
    {
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    b ("power", "Power", true);
    b ("eqFirst", "EQ Before Dyn", false);

    // --- Filtres / préampli / entrée
    model ("preModel", "Pre Model");
    f ("inTrim", "In Trim", -20.f, 20.f, 0.f, 1.f, 1);
    b ("phase", "Phase", false);
    f ("preamp", "Preamp", 0.f, 10.f, 0.f, 1.f, 0);
    f ("hp", "HP", 20.f, 500.f, 20.f, 0.4f, 2);
    f ("lp", "LP", 3000.f, 20000.f, 20000.f, 0.4f, 2);

    // --- EQ
    model ("eqModel", "EQ Model");
    b ("eqOn", "EQ On", true);
    b ("eqE", "EQ E-Series", false);
    f ("hfGain", "HF Gain", -15.f, 15.f, 0.f, 1.f, 1);
    f ("hfFreq", "HF Freq", 1500.f, 22000.f, 8000.f, 0.5f, 2);
    b ("hfBell", "HF Bell", false);
    f ("hmfGain", "HMF Gain", -15.f, 15.f, 0.f, 1.f, 1);
    f ("hmfFreq", "HMF Freq", 600.f, 7000.f, 3000.f, 0.5f, 2);
    f ("hmfQ", "HMF Q", 0.4f, 4.f, 1.f, 0.6f, 0);
    f ("lmfGain", "LMF Gain", -15.f, 15.f, 0.f, 1.f, 1);
    f ("lmfFreq", "LMF Freq", 200.f, 2000.f, 600.f, 0.5f, 2);
    f ("lmfQ", "LMF Q", 0.4f, 4.f, 1.f, 0.6f, 0);
    f ("lfGain", "LF Gain", -15.f, 15.f, 0.f, 1.f, 1);
    f ("lfFreq", "LF Freq", 40.f, 600.f, 100.f, 0.5f, 2);
    b ("lfBell", "LF Bell", false);

    // --- Dynamique
    model ("dynModel", "Dyn Model");
    b ("dynOn", "Dyn On", true);
    f ("compThr", "Comp Threshold", -20.f, 10.f, 10.f, 1.f, 1);
    f ("compRatio", "Comp Ratio", 1.f, 20.f, 4.f, 0.5f, 5);
    f ("compRel", "Comp Release", 0.1f, 4.f, 0.3f, 0.5f, 3);
    b ("compFast", "Comp Fast Attack", false);
    b ("compPeak", "Comp Peak", false);
    f ("mix", "Comp Mix", 0.f, 1.f, 1.f, 1.f, 4);
    f ("gateThr", "Gate Threshold", -30.f, 10.f, -30.f, 1.f, 1);
    f ("gateRange", "Gate Range", 0.f, 40.f, 20.f, 1.f, 1);
    f ("gateRel", "Gate Release", 0.1f, 4.f, 0.2f, 0.5f, 3);
    f ("gateHold", "Gate Hold", 0.f, 4.f, 0.f, 0.5f, 3);
    b ("gateExpand", "Gate Expand", false);
    b ("gateFast", "Gate Fast Attack", false);

    // --- Sortie
    f ("width", "Width", 0.f, 2.f, 1.f, 1.f, 0);
    f ("pan", "Pan", -1.f, 1.f, 0.f, 1.f, 0);
    b ("cut", "Cut", false);
    NormalisableRange<float> faderRange (-60.f, 12.f,
        [] (float, float, float pos) { return FaderLaw::toDb (pos); },
        [] (float, float, float db)  { return FaderLaw::toPos (db); },
        [] (float, float, float db)  { return jlimit (-60.f, 12.f, db); });
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "fader", 1 }, "Fader", faderRange, 0.f,
                 AudioParameterFloatAttributes().withStringFromValueFunction (strFn (6))));
    f ("outTrim", "Out Trim", -24.f, 24.f, 0.f, 1.f, 1);

    return { p.begin(), p.end() };
}

ChannelStripProcessor::ChannelStripProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "STATE", createLayout())
{
    for (auto& a : inPeak)  a.store (0.f);
    for (auto& a : outPeak) a.store (0.f);
}

bool ChannelStripProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    auto out = l.getMainOutputChannelSet();
    return (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo())
           && out == l.getMainInputChannelSet();
}

float ChannelStripProcessor::trimTarget() const
{
    return Decibels::decibelsToGain (get ("inTrim")) * (get ("phase") > 0.5f ? -1.f : 1.f);
}

float ChannelStripProcessor::outTarget() const
{
    if (get ("cut") > 0.5f) return 0.f;
    return Decibels::decibelsToGain (get ("fader"), -59.9f) * Decibels::decibelsToGain (get ("outTrim"));
}

void ChannelStripProcessor::prepareToPlay (double rate, int blockSize)
{
    sr = rate;
    for (auto& h : hp) h.reset();
    for (auto& l : lp) l.reset();
    for (auto& ch : eq) for (auto& e : ch) e.reset();

    gateDb = compGrDb = rmsState = 0.f;
    holdCount = 0;
    for (int c = 0; c < 2; ++c) dcX[c] = dcY[c] = 0.f;
    lastAmt = -1.f;
    lastSatModel = -1;

    oversampling.initProcessing ((size_t) blockSize);
    oversampling.reset();
    setLatencySamples (roundToInt (oversampling.getLatencyInSamples()));

    lastTrim = trimTarget();
    lastOut = outTarget();
    updateFilters (true);
    updateEq (true);
}

// ---------------------------------------------------------------------------
// Préampli : saturation de couleur, compensée en niveau (ne monte pas le volume)
// ---------------------------------------------------------------------------
void ChannelStripProcessor::processPreamp (AudioBuffer<float>& buf)
{
    const int m = (int) get ("preModel");
    const float amt = get ("preamp") / 10.f;
    const float k = 1.f + amt * 2.f + amt * amt * 14.f;     // 1 .. 17
    const bool active = amt > 0.001f;

    if (amt != lastAmt || m != lastSatModel)
    {
        satComp = satCompensation (m, k);
        lastAmt = amt;
        lastSatModel = m;
    }

    // On passe toujours par le sur-échantillonnage pour garder une latence constante
    dsp::AudioBlock<float> block (buf);
    auto up = oversampling.processSamplesUp (block);

    if (active)
    {
        for (size_t c = 0; c < up.getNumChannels(); ++c)
        {
            auto* d = up.getChannelPointer (c);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
                d[i] = shaper (m, d[i] * k) * satComp;
        }
    }
    oversampling.processSamplesDown (block);

    if (active)   // bloqueur de continu (la saturation asymétrique crée un léger offset)
    {
        for (int c = 0; c < buf.getNumChannels(); ++c)
        {
            auto* d = buf.getWritePointer (c);
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                const float x = d[i];
                const float y = x - dcX[c] + 0.995f * dcY[c];
                dcX[c] = x; dcY[c] = y;
                d[i] = y;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Filtres HP / LP
// ---------------------------------------------------------------------------
void ChannelStripProcessor::updateFilters (bool force)
{
    const int m = (int) get ("preModel");
    float fh = get ("hp");
    if (m == NEVE && fh > 25.f) fh = snapTo (fh, { 50.f, 80.f, 160.f, 300.f });   // fréquences fixes type Neve
    const float fl = get ("lp");

    if (! force && fh == lastHp && fl == lastLp && m == lastFilterModel) return;
    lastHp = fh; lastLp = fl; lastFilterModel = m;

    const float nyq = (float) sr * 0.45f;
    auto ch = Coef::makeHighPass (sr, jlimit (20.f, nyq, fh), m == API ? 0.8f : 0.707f);
    auto cl = Coef::makeLowPass  (sr, jlimit (1000.f, nyq, fl), 0.707f);
    for (int c = 0; c < 2; ++c) { hp[c].coefficients = ch; lp[c].coefficients = cl; }
}

void ChannelStripProcessor::processFilters (AudioBuffer<float>& buf)
{
    updateFilters (false);
    const bool hpOn = get ("hp") > 20.5f, lpOn = get ("lp") < 19900.f;
    if (! hpOn && ! lpOn) return;

    for (int c = 0; c < buf.getNumChannels(); ++c)
    {
        auto* d = buf.getWritePointer (c);
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            float x = d[i];
            if (hpOn) x = hp[c].processSample (x);
            if (lpOn) x = lp[c].processSample (x);
            d[i] = x;
        }
    }
}

// ---------------------------------------------------------------------------
// EQ 4 bandes : fréquences / Q selon le modèle
//   SSL  : Q réglable, bouton E (courbes de plateau différentes)
//   NEVE : fréquences fixes (type 1073), Q doux
//   API  : "proportional Q" (le Q se resserre quand le gain augmente)
// ---------------------------------------------------------------------------
void ChannelStripProcessor::updateEq (bool force)
{
    const int m = (int) get ("eqModel");
    float fr[4] { get ("lfFreq"), get ("lmfFreq"), get ("hmfFreq"), get ("hfFreq") };
    const float gn[4] { get ("lfGain"), get ("lmfGain"), get ("hmfGain"), get ("hfGain") };
    const float qk[2] { get ("lmfQ"), get ("hmfQ") };
    const bool lfBell = get ("lfBell") > 0.5f, hfBell = get ("hfBell") > 0.5f, eMode = get ("eqE") > 0.5f;

    if (m == NEVE)
    {
        fr[0] = snapTo (fr[0], { 35.f, 60.f, 110.f, 220.f });
        fr[1] = snapTo (fr[1], { 360.f, 700.f, 1600.f });
        fr[2] = snapTo (fr[2], { 3200.f, 4800.f, 7200.f });
        fr[3] = snapTo (fr[3], { 10000.f, 12000.f, 16000.f });
    }

    const std::array<float, 16> now {{ fr[0], fr[1], fr[2], fr[3], gn[0], gn[1], gn[2], gn[3],
                                       qk[0], qk[1], lfBell ? 1.f : 0.f, hfBell ? 1.f : 0.f,
                                       eMode ? 1.f : 0.f, (float) m, 0.f, 0.f }};
    if (! force && now == lastEq) return;
    lastEq = now;

    const float shelfQ = m == SSL ? (eMode ? 0.9f : 0.6f) : (m == NEVE ? 0.6f : 0.7f);
    auto bellQ = [m] (float knob, float g)
    {
        switch (m)
        {
            case SSL:  return knob;
            case NEVE: return 0.5f + 0.4f * knob;
            default:   return jmax (0.2f, knob * (0.6f + 0.08f * std::abs (g)));
        }
    };

    const float nyq = (float) sr * 0.45f;
    auto lo  = lfBell ? Coef::makePeakFilter (sr, fr[0], bellQ (1.f, gn[0]), Decibels::decibelsToGain (gn[0]))
                      : Coef::makeLowShelf   (sr, fr[0], shelfQ, Decibels::decibelsToGain (gn[0]));
    auto lmf = Coef::makePeakFilter (sr, fr[1], bellQ (qk[0], gn[1]), Decibels::decibelsToGain (gn[1]));
    auto hmf = Coef::makePeakFilter (sr, fr[2], bellQ (qk[1], gn[2]), Decibels::decibelsToGain (gn[2]));
    auto hi  = hfBell ? Coef::makePeakFilter (sr, jmin (fr[3], nyq), bellQ (1.f, gn[3]), Decibels::decibelsToGain (gn[3]))
                      : Coef::makeHighShelf  (sr, jmin (fr[3], nyq), shelfQ, Decibels::decibelsToGain (gn[3]));

    for (int c = 0; c < 2; ++c)
    {
        eq[c][0].coefficients = lo;  eq[c][1].coefficients = lmf;
        eq[c][2].coefficients = hmf; eq[c][3].coefficients = hi;
    }
}

void ChannelStripProcessor::processEq (AudioBuffer<float>& buf)
{
    if (get ("eqOn") < 0.5f) return;
    updateEq (false);
    for (int c = 0; c < buf.getNumChannels(); ++c)
    {
        auto* d = buf.getWritePointer (c);
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            float x = d[i];
            for (int band = 0; band < 4; ++band) x = eq[c][band].processSample (x);
            d[i] = x;
        }
    }
}

// ---------------------------------------------------------------------------
// Dynamique : gate/expander (avec hold) puis compresseur feed-forward
// ---------------------------------------------------------------------------
void ChannelStripProcessor::processDynamics (AudioBuffer<float>& buf)
{
    if (get ("dynOn") < 0.5f)
    {
        gateDb = compGrDb = 0.f;
        grMeter.store (0.f);
        return;
    }

    const int m = (int) get ("dynModel");

    // Gate / expander
    const float gThr = get ("gateThr"), gRange = get ("gateRange");
    const bool expandMode = get ("gateExpand") > 0.5f, gateFast = get ("gateFast") > 0.5f;
    const float gRatio = expandMode ? 2.f : (m == NEVE ? 6.f : (m == API ? 8.f : 10.f));
    const bool gateOn = gThr > -29.9f && gRange > 0.01f;
    const float gAtkMs = (gateFast ? 0.05f : 0.5f) * (m == NEVE ? 1.5f : 1.f);
    const float gRelMs = get ("gateRel") * 1000.f;
    const int holdSamples = (int) (get ("gateHold") * (float) sr);

    // Compresseur
    const float thr = get ("compThr");
    float ratio = get ("compRatio");
    if (ratio >= 19.9f) ratio = 100.f;                       // position max = infini (limiteur)
    const bool fast = get ("compFast") > 0.5f, peak = get ("compPeak") > 0.5f;
    const float knee   = m == SSL ? 2.f : (m == NEVE ? 8.f : 3.f);
    const float atkMs  = m == SSL ? (fast ? 0.1f : 3.f) : (m == NEVE ? (fast ? 0.5f : 10.f) : (fast ? 0.3f : 5.f));
    const float relMul = m == SSL ? 1.f : (m == NEVE ? 1.3f : 0.7f);
    const float relMs  = get ("compRel") * 1000.f * relMul;
    const float mix = get ("mix");
    const float makeup = jmax (0.f, -thr) * (1.f - 1.f / ratio) * 0.5f;   // makeup automatique

    auto coef = [this] (float ms) { return std::exp (-1.f / (float) (sr * (double) ms * 0.001)); };
    const float gAtk = coef (gAtkMs), gRel = coef (gRelMs);
    const float cAtk = coef (atkMs), cRel = coef (relMs), rmsCoef = coef (10.f);

    const int nCh = buf.getNumChannels(), n = buf.getNumSamples();
    float minGr = 0.f;

    for (int i = 0; i < n; ++i)
    {
        float pk = 0.f;
        for (int c = 0; c < nCh; ++c) pk = jmax (pk, std::abs (buf.getSample (c, i)));
        const float pkDb = Decibels::gainToDecibels (pk, -120.f);

        // Gate : "attack" = ouverture, "release" = fermeture, après le temps de hold
        if (gateOn)
        {
            if (pkDb >= gThr) holdCount = holdSamples;
            else if (holdCount > 0) --holdCount;

            const float gTarget = (pkDb < gThr && holdCount == 0)
                                    ? -jmin (gRange, (gThr - pkDb) * (gRatio - 1.f)) : 0.f;
            const float gc = gTarget < gateDb ? gRel : gAtk;
            gateDb = gc * gateDb + (1.f - gc) * gTarget;
        }
        else gateDb = 0.f;

        // Détection du compresseur : crête ou RMS
        float lvl = pk;
        if (! peak)
        {
            rmsState = rmsCoef * rmsState + (1.f - rmsCoef) * pk * pk;
            lvl = std::sqrt (2.f * rmsState);
        }
        const float l2 = Decibels::gainToDecibels (lvl, -120.f) + gateDb;

        const float over = l2 - thr;
        float out;
        if (2.f * over < -knee)                    out = l2;
        else if (2.f * std::abs (over) <= knee)    out = l2 + (1.f / ratio - 1.f) * square (over + knee * 0.5f) / (2.f * knee);
        else                                       out = thr + over / ratio;

        const float grTarget = out - l2;           // <= 0
        const float cc = grTarget < compGrDb ? cAtk : cRel;
        compGrDb = cc * compGrDb + (1.f - cc) * grTarget;
        minGr = jmin (minGr, compGrDb);

        const float dryG = Decibels::decibelsToGain (gateDb);
        const float wetG = Decibels::decibelsToGain (gateDb + compGrDb + makeup);
        for (int c = 0; c < nCh; ++c)
        {
            const float x = buf.getSample (c, i);
            const float dry = x * dryG, wet = x * wetG;
            buf.setSample (c, i, dry + (wet - dry) * mix);
        }
    }
    grMeter.store (-minGr);
}

// ---------------------------------------------------------------------------
// Width (mid/side) et Pan
// ---------------------------------------------------------------------------
void ChannelStripProcessor::processWidthPan (AudioBuffer<float>& buf)
{
    if (buf.getNumChannels() != 2) return;
    const float width = get ("width"), pan = get ("pan");
    if (std::abs (width - 1.f) < 0.001f && std::abs (pan) < 0.001f) return;

    const float ang = (pan + 1.f) * MathConstants<float>::pi * 0.25f;
    const float gl = std::cos (ang) * 1.41421356f, gr = std::sin (ang) * 1.41421356f;
    auto* l = buf.getWritePointer (0);
    auto* r = buf.getWritePointer (1);
    for (int i = 0; i < buf.getNumSamples(); ++i)
    {
        const float mid = 0.5f * (l[i] + r[i]);
        const float side = 0.5f * (l[i] - r[i]) * width;
        l[i] = (mid + side) * gl;
        r[i] = (mid - side) * gr;
    }
}

// ---------------------------------------------------------------------------
void ChannelStripProcessor::processBlock (AudioBuffer<float>& buf, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buf.clear (c, 0, buf.getNumSamples());

    const int nCh = buf.getNumChannels(), n = buf.getNumSamples();
    if (nCh > 2) return;

    for (int c = 0; c < 2; ++c)
        inPeak[c].store (c < nCh ? buf.getMagnitude (c, 0, n) : 0.f);

    if (get ("power") < 0.5f)
    {
        for (int c = 0; c < 2; ++c) outPeak[c].store (inPeak[c].load());
        grMeter.store (0.f);
        return;
    }

    // In Trim + inversion de phase (avec rampe anti-clic)
    const float trim = trimTarget();
    buf.applyGainRamp (0, n, lastTrim, trim);
    lastTrim = trim;

    processPreamp (buf);
    processFilters (buf);

    if (get ("eqFirst") > 0.5f) { processEq (buf); processDynamics (buf); }
    else                        { processDynamics (buf); processEq (buf); }

    processWidthPan (buf);

    const float outG = outTarget();
    buf.applyGainRamp (0, n, lastOut, outG);
    lastOut = outG;

    for (int c = 0; c < 2; ++c)
        outPeak[c].store (c < nCh ? buf.getMagnitude (c, 0, n) : 0.f);
}

// ---------------------------------------------------------------------------
void ChannelStripProcessor::selectSlot (int s)
{
    if (s == activeSlot || s < 0 || s > 1) return;
    slotState[activeSlot] = apvts.copyState();
    if (slotState[s].isValid())
        apvts.replaceState (slotState[s].createCopy());
    activeSlot = s;
}

AudioProcessorEditor* ChannelStripProcessor::createEditor() { return new ChannelStripEditor (*this); }

void ChannelStripProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void ChannelStripProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChannelStripProcessor(); }
