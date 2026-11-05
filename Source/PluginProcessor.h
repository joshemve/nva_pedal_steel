#pragma once
#include <JuceHeader.h>
#include "Params.h"
#include "Presets.h"
#include "dsp/PedalSteelEngine.h"

class PedalSteelAudioProcessor : public juce::AudioProcessor {
public:
    PedalSteelAudioProcessor();
    ~PedalSteelAudioProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPresetIndex; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    void loadPreset(int index);
    void setLivePedalState(int pedalIdx, float val);
    void triggerChordAudition(int chordIdx);
    CopedentStatus getLiveStatus() const { return engine.getStatus(); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    PedalSteelEngine engine;
    int currentPresetIndex = 0;

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
    std::atomic<float>* pAmpDrive      = nullptr;
    std::atomic<float>* pToneWarmth    = nullptr;
    std::atomic<float>* pReverbMix     = nullptr;
    std::atomic<float>* pReverbDecay   = nullptr;
    std::atomic<float>* pOutputLevel   = nullptr;

    PedalSteelParams readParamsFromAtomics() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PedalSteelAudioProcessor)
};
