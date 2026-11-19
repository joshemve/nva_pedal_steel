#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class PedalSteelLookAndFeel : public juce::LookAndFeel_V4 {
public:
    PedalSteelLookAndFeel();
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;
    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

class PedalSteelKeyboardComponent : public juce::MidiKeyboardComponent {
public:
    PedalSteelKeyboardComponent(juce::MidiKeyboardState& state, Orientation orientation)
        : juce::MidiKeyboardComponent(state, orientation) {}

    void setPhraseKeyswitchActive(bool active) {
        if (phraseKsActive != active) {
            phraseKsActive = active;
            repaint();
        }
    }

protected:
    void drawWhiteNote(int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                       bool isDown, bool isOver, juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote(int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                       bool isDown, bool isOver, juce::Colour noteFillColour) override;

private:
    bool phraseKsActive = false;
};

class FretboardVisualizer : public juce::Component {
public:
    explicit FretboardVisualizer(PedalSteelAudioProcessor& proc);
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void updateStatus(const CopedentStatus& st);
    void setPatchInfoText(const juce::String& info);
    void clearLatchedPedals();

private:
    void syncPedalsFromUiState(bool beginGesture, bool endGesture);
    void setOpticalSwellFromMouseX(float mouseX);
    void showPedalContextMenu(int buttonIndex);
    void showOpticalSwellContextMenu();

    PedalSteelAudioProcessor& processor;
    CopedentStatus status{};
    juce::String patchInfoText;
    juce::Rectangle<float> cachedNeckArea{};
    juce::Rectangle<float> cachedVolRect{};
    float cachedNutX = 0.0f;
    float cachedScaleWidth = 1.0f;
    float cachedStrTop = 0.0f;
    float cachedStrStep = 1.0f;
    std::array<juce::Rectangle<float>, 6> pedalRects{};
    std::array<bool, 6> latchedUiPedals{}; // 0=A, 1=A+B, 2=B, 3=C, 4=LKL, 5=LKR
    int heldUiPedal = -1;                  // 0=A, 1=A+B, 2=B, 3=C, 4=LKL, 5=LKR
    int draggedString = -1;                // 0..9 interactive neck string pluck/slide
    bool draggingVolPedal = false;
};

class PedalSteelAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer {
public:
    explicit PedalSteelAudioProcessorEditor(PedalSteelAudioProcessor&);
    ~PedalSteelAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;
    bool keyStateChanged(bool isKeyDown) override;
    void focusLost(FocusChangeType cause) override;
    void refreshVisualizer();

private:
    void timerCallback() override;
    void rebuildPresetMenu();
    void buildCategorizedCharacterMenu();
    void syncPresetComboSelection();
    void updatePresetInfoAndTooltips();
    void updateKnobDoubleClickTargets();
    void showKnobContextMenu(int knobIdx);
    void pollQwertyKeys();
    void releaseAllQwertyKeys();
    void updateQwertyButtonAppearance();
    void promptSaveUserPreset();
    void promptLoadUserPreset();

    PedalSteelAudioProcessor& audioProcessor;
    PedalSteelLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow{ this, 420 };
    std::shared_ptr<juce::FileChooser> activeFileChooser;
    std::vector<juce::File> cachedUserPresets;
    uint32_t lastSeenPresetRevision = 0;
    int lastCharSelectedId = -1;
    bool lastPresetModifiedState = true;
    bool ignoreCharComboCallback = false;

    // Top bar controls
    juce::TextButton prevPresetBtn{ "<" };
    juce::TextButton nextPresetBtn{ ">" };
    juce::TextButton revertBtn{ "REV" };
    juce::ComboBox presetBox;
    juce::ComboBox styleBox;
    juce::ComboBox modeBox;
    juce::ComboBox charBox;
    juce::TextButton syncBtn{ "SYNC" };
    juce::TextButton qwertyBtn{ "KEYS" };
    juce::TextButton lickBtn{ "PLAY LICK" };
    std::array<juce::TextButton, 4> chordButtons;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;
    std::unique_ptr<juce::ParameterAttachment> charParamAttach;
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

    // Laptop QWERTY Musical Typing state
    bool qwertyEnabled = false;
    int qwertyBaseMidi = 60; // C3 default (Z/X shifts 36..84)
    std::array<int, 23> activeQwertyMidi{}; // -1 if key slot not currently down

    float meterL = 0.0f;
    float meterR = 0.0f;
    bool phraseActiveUi = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PedalSteelAudioProcessorEditor)
};
