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

class FretboardVisualizer : public juce::Component {
public:
    explicit FretboardVisualizer(PedalSteelAudioProcessor& proc);
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void updateStatus(const CopedentStatus& st);

private:
    PedalSteelAudioProcessor& processor;
    CopedentStatus status{};
    std::array<juce::Rectangle<float>, 6> pedalRects{};
    int heldUiPedal = -1; // 0=A, 1=A+B, 2=B, 3=C, 4=LKL, 5=LKR
};

class PedalSteelAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer {
public:
    explicit PedalSteelAudioProcessorEditor(PedalSteelAudioProcessor&);
    ~PedalSteelAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshVisualizer() { fretboard.updateStatus(audioProcessor.getLiveStatus()); }

private:
    void timerCallback() override;

    PedalSteelAudioProcessor& audioProcessor;
    PedalSteelLookAndFeel lookAndFeel;

    // Top bar controls
    juce::ComboBox presetBox;
    juce::ComboBox styleBox;
    juce::ComboBox modeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;

    std::array<juce::TextButton, 4> chordButtons;

    // Center E9th Fretboard & Copedent Visualizer
    FretboardVisualizer fretboard;

    // 20 Rotary Knobs organized into 3 studio panels
    struct KnobWithLabel {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };
    std::array<KnobWithLabel, 20> knobs;

    // Bottom playable MIDI keyboard
    juce::MidiKeyboardComponent midiKeyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PedalSteelAudioProcessorEditor)
};
