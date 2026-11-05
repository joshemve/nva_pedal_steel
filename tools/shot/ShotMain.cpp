#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main(int argc, char* argv[]) {
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::File outFile = (argc > 1)
        ? juce::File(juce::String(argv[1]))
        : juce::File::getCurrentWorkingDirectory().getChildFile("renders/ui_screenshot.png");
    outFile.getParentDirectory().createDirectory();

    PedalSteelAudioProcessor processor;
    processor.prepareToPlay(44100.0, 512);

    // Trigger an E-major E9th chord and engage Pedal A & B so the visualizer shows live string energy & bends
    processor.triggerChordAudition(0);
    processor.setLivePedalState(0, 0.85f);
    processor.setLivePedalState(1, 1.0f);

    juce::AudioBuffer<float> audioBuf(2, 512);
    juce::MidiBuffer midiBuf;
    for (int i = 0; i < 12; ++i) {
        processor.processBlock(audioBuf, midiBuf);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    editor->setSize(1060, 700);

    // Refresh visualizer state
    if (auto* psEditor = dynamic_cast<PedalSteelAudioProcessorEditor*>(editor.get())) {
        psEditor->refreshVisualizer();
    }

    juce::Image snapshot = editor->createComponentSnapshot(editor->getLocalBounds());
    juce::PNGImageFormat pngFormat;
    juce::FileOutputStream stream(outFile);
    if (stream.openedOk()) {
        stream.setPosition(0);
        stream.truncate();
        pngFormat.writeImageToStream(snapshot, stream);
        std::cout << "Saved UI screenshot to: " << outFile.getFullPathName() << std::endl;
        return 0;
    }

    std::cerr << "Failed to write screenshot: " << outFile.getFullPathName() << std::endl;
    return 1;
}
