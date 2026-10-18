#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// Modèles disponibles dans chaque section
enum Model { SSL = 0, NEVE = 1, API = 2 };

// Loi du fader : positions de l'échelle  +12  6  0  -5  -10  -20  -30  -40  -50  -inf
namespace FaderLaw
{
    inline constexpr float pts[10] = { -60.f, -50.f, -40.f, -30.f, -20.f, -10.f, -5.f, 0.f, 6.f, 12.f };

    inline float toPos (float db)
    {
        db = juce::jlimit (-60.f, 12.f, db);
        for (int i = 0; i < 9; ++i)
            if (db <= pts[i + 1])
                return ((float) i + (db - pts[i]) / (pts[i + 1] - pts[i])) / 9.f;
        return 1.f;
    }

    inline float toDb (float p)
    {
        p = juce::jlimit (0.f, 1.f, p) * 9.f;
        const int i = juce::jmin (8, (int) p);
        return pts[i] + (p - (float) i) * (pts[i + 1] - pts[i]);
    }
}

class ChannelStripProcessor : public juce::AudioProcessor
{
public:
    ChannelStripProcessor();
    ~ChannelStripProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Console Strip"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Comparaison A/B
    void selectSlot (int slot);
    int getActiveSlot() const { return activeSlot; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::UndoManager undoManager;                 // doit être déclaré avant apvts
    juce::AudioProcessorValueTreeState apvts;

    // Valeurs lues par l'interface (vumètres)
    std::atomic<float> inPeak[2], outPeak[2];
    std::atomic<float> grMeter { 0.f };

private:
    float get (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    float trimTarget() const;
    float outTarget() const;

    void processPreamp (juce::AudioBuffer<float>&);
    void processFilters (juce::AudioBuffer<float>&);
    void processDynamics (juce::AudioBuffer<float>&);
    void processEq (juce::AudioBuffer<float>&);
    void processWidthPan (juce::AudioBuffer<float>&);
    void updateFilters (bool force);
    void updateEq (bool force);

    using Filter = juce::dsp::IIR::Filter<float>;

    double sr = 44100.0;

    // Filtres
    Filter hp[2], lp[2];
    float lastHp = -1.f, lastLp = -1.f;
    int lastFilterModel = -1;

    // EQ
    Filter eq[2][4];
    std::array<float, 16> lastEq {};

    // Préampli (saturation compensée en niveau)
    float lastAmt = -1.f, satComp = 1.f;
    int lastSatModel = -1;
    float dcX[2] {}, dcY[2] {};
    juce::dsp::Oversampling<float> oversampling { 2, 1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };

    // Dynamique
    float gateDb = 0.f, compGrDb = 0.f, rmsState = 0.f;
    int holdCount = 0;

    // Lissage des gains
    float lastTrim = 1.f, lastOut = 1.f;

    // A/B
    juce::ValueTree slotState[2];
    int activeSlot = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStripProcessor)
};
