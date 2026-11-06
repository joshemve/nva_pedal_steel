#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class PedalSteelLookAndFeel : public juce::LookAndFeel_V4 {
public:
    PedalSteelLookAndFeel();
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;
};

class PedalSteelKeyboardComponent : public juce::MidiKeyboardComponent {
public:
    PedalSteelKeyboardComponent(juce::MidiKeyboardState& state, Orientation orientation)
        : juce::MidiKeyboardComponent(state, orientation) {}

protected:
    void drawWhiteNote(int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                       bool isDown, bool isOver, juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote(int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                       bool isDown, bool isOver, juce::Colour noteFillColour) override;
};

class FretboardVisualizer : public juce::Component {
public:
    explicit FretboardVisualizer(PedalSteelAudioProcessor& proc);
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void updateStatus(const CopedentStatus& st);

private:
    PedalSteelAudioProcessor& processor;
    CopedentStatus status{};
    juce::Rectangle<float> cachedNeckArea{};
    float cachedNutX = 0.0f;
    float cachedScaleWidth = 1.0f;
    float cachedStrTop = 0.0f;
    float cachedStrStep = 1.0f;
    std::array<juce::Rectangle<float>, 6> pedalRects{};
    int heldUiPedal = -1;     // 0=A, 1=A+B, 2=B, 3=C, 4=LKL, 5=LKR
    int draggedString = -1;   // 0..9 interactive neck string pluck/slide
};

class PedalSteelAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer {
public:
    explicit PedalSteelAudioProcessorEditor(PedalSteelAudioProcessor&);
    ~PedalSteelAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshVisualizer();

private:
    void timerCallback() override;
    void rebuildPresetMenu();
    void syncPresetComboSelection();
    void updateKnobDoubleClickTargets();
    void promptSaveUserPreset();
    void promptLoadUserPreset();

    PedalSteelAudioProcessor& audioProcessor;
    PedalSteelLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow{ this, 450 };
    std::shared_ptr<juce::FileChooser> activeFileChooser;
    std::vector<juce::File> cachedUserPresets;
    uint32_t lastSeenPresetRevision = 0;
    int lastCharSelectedId = -1;

    // Top bar controls
    juce::TextButton prevPresetBtn{ "<" };
    juce::TextButton nextPresetBtn{ ">" };
    juce::ComboBox presetBox;
    juce::ComboBox styleBox;
    juce::ComboBox modeBox;
    juce::ComboBox charBox;
    juce::TextButton syncBtn{ "SYNC" };
    juce::TextButton lickBtn{ "PLAY LICK" };
    std::array<juce::TextButton, 4> chordButtons;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> charAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttach;

    // Center E9th Fretboard & Copedent Visualizer
    FretboardVisualizer fretboard;

    // 24 Symmetrical Rotary Knobs (8 + 8 + 8 across 3 studio hardware panels)
    struct KnobWithLabel {
        const char* paramId = "";
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };
    std::array<KnobWithLabel, 24> knobs;

    // Bottom Color-Coded Keyswitch & E9th MIDI Keyboard
    PedalSteelKeyboardComponent midiKeyboard;

    float meterL = 0.0f;
    float meterR = 0.0f;
    bool phraseActiveUi = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PedalSteelAudioProcessorEditor)
};
