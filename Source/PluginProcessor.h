#pragma once
#include <JuceHeader.h>
#include "Params.h"
#include "Presets.h"
#include "dsp/PedalSteelEngine.h"
#include "MidiLearn.h"

class PedalSteelAudioProcessor : public juce::AudioProcessor {
public:
    PedalSteelAudioProcessor();
    ~PedalSteelAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.5; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPresetIndex.load(); }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    MidiLearn midiLearn;   // CC -> parameter (the editor's MIDI LEARN button)
    int editorWidth = 1280;   // the window size the user last chose (kept while the plugin instance lives, and in its state)
    bool showMore = false;    // the MORE deck (second knob row) is open; kept like editorWidth

    void loadPreset(int index);
    void stepPreset(int delta);
    void setLivePedalState(int pedalIdx, float val, bool beginGesture = false, bool endGesture = false);
    void triggerChordAudition(int chordIdx);
    void triggerPhraseAudition();
    void triggerPanic();
    void triggerFretboardNote(int stringIdx, float fret, int style = -1);   // style: another neck's tuning (double neck)
    void slideFretboardNote(int stringIdx, float fret);
    void releaseFretboardNote(int stringIdx);

    // User Preset Management (.nvps XML in Documents/NVA Pedal Steel/Presets)
    static juce::File getUserPresetsFolder();
    std::vector<juce::File> getUserPresetFiles() const;
    bool saveUserPreset(const juce::File& file);
    bool loadUserPreset(const juce::File& file);
    juce::String getActiveUserPresetName() const;
    uint32_t getPresetRevision() const { return presetRevision.load(std::memory_order_relaxed); }
    float getBaselineParamValue(const juce::String& paramId) const;
    void captureCurrentAsBaseline();   // the current values become "the preset" (the display's * marker clears)

    CopedentStatus getLiveStatus() const;
    float getOutputPeakL() const { return outputPeakL.load(std::memory_order_relaxed); }
    float getOutputPeakR() const { return outputPeakR.load(std::memory_order_relaxed); }
    juce::uint32 getBlocksProcessed() const { return blocksProcessed.load(std::memory_order_relaxed); }
    double getHostBpm() const    { return currentHostBpm.load(std::memory_order_relaxed); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    enum class UiCmdType { Chord, Phrase, Panic, FretDown, FretDrag, FretUp, Pedal };
    struct UiCmd {
        UiCmdType type = UiCmdType::Chord;
        int intArg = 0;
        float floatArg = 0.0f;
        int styleArg = -1;   // FretDown: the tuning to play in (-1 = the preset's)
    };

    PedalSteelEngine engine;
    mutable juce::SpinLock statusLock;
    CopedentStatus cachedStatus{};

    juce::SpinLock uiCmdLock;
    std::vector<UiCmd> pendingUiCmds;

    mutable juce::CriticalSection presetMetaLock;
    juce::String activeUserPresetName;
    std::map<juce::String, float> baselineParamValues;

    std::atomic<int> currentPresetIndex{ 0 };
    std::atomic<uint32_t> presetRevision{ 1 };
    std::atomic<float> outputPeakL{ 0.0f };
    std::atomic<float> outputPeakR{ 0.0f };
    std::atomic<double> currentHostBpm{ 120.0 };
    std::atomic<juce::uint32> blocksProcessed{ 0 };


    // Cached atomic parameter pointers
    std::atomic<float>* pPresetStyle   = nullptr;
    std::atomic<float>* pPlayMode      = nullptr;
    std::atomic<float>* pPedalA        = nullptr;
    std::atomic<float>* pPedalB        = nullptr;
    std::atomic<float>* pPedalC        = nullptr;
    std::atomic<float>* pLeverLKL      = nullptr;
    std::atomic<float>* pLeverLKR      = nullptr;
    std::atomic<float>* pVolumePedal   = nullptr;
    std::atomic<float>* pScoopDepth    = nullptr;
    std::atomic<float>* pPedalSpeed    = nullptr;
    std::atomic<float>* pBarGlide      = nullptr;
    std::atomic<float>* pBarSlideNoise = nullptr;
    std::atomic<float>* pVibratoDepth  = nullptr;
    std::atomic<float>* pVibratoRate   = nullptr;
    std::atomic<float>* pAutoSwell     = nullptr;
    std::atomic<float>* pSwellTime     = nullptr;
    std::atomic<float>* pSampleBlend   = nullptr;
    std::atomic<float>* pPickSnap      = nullptr;
    std::atomic<float>* pPickupPos     = nullptr;
    std::atomic<float>* pCoilPeakFreq  = nullptr;
    std::atomic<float>* pCoilResonance = nullptr;
    std::atomic<float>* pSustainTime   = nullptr;
    std::atomic<float>* pCabinetDrop   = nullptr;
    std::atomic<float>* pStereoWidth   = nullptr;
    std::atomic<float>* pAmpDrive      = nullptr;
    std::atomic<float>* pToneWarmth    = nullptr;
    std::atomic<float>* pDelayMix      = nullptr;
    std::atomic<float>* pDelayTime     = nullptr;
    std::atomic<float>* pTempoSync     = nullptr;
    std::atomic<float>* pReverbMix     = nullptr;
    std::atomic<float>* pReverbDecay   = nullptr;
    std::atomic<float>* pFxCharacter   = nullptr;
    std::atomic<float>* pOutputLevel   = nullptr;

    PedalSteelParams readParamsFromAtomics() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PedalSteelAudioProcessor)
};
