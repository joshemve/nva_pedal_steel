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

    // Load Preset 15 (Dark Side Phase Steel) to showcase SYNC + Analog Phaser Character + E9th Pedals A+B
    processor.loadPreset(14);
    processor.triggerChordAudition(0);
    processor.setLivePedalState(0, 0.85f);
    processor.setLivePedalState(1, 1.0f);

    juce::AudioBuffer<float> audioBuf(2, 512);
    juce::MidiBuffer midiBuf;
    for (int i = 0; i < 16; ++i) {
        processor.processBlock(audioBuf, midiBuf);
    }

    // Verify full state, MIDI CC Learn & Baseline Dirty/Revert round-trip integrity
    {
        processor.loadPreset(14);
        if (processor.isCurrentPresetModified()) {
            std::cerr << "Freshly loaded preset should not be marked modified!" << std::endl;
            return 4;
        }
        // Test MIDI CC Learn on ampDrive -> CC 74
        processor.startMidiLearn(ParamIDs::ampDrive);
        juce::MidiBuffer learnMidi;
        learnMidi.addEvent(juce::MidiMessage::controllerEvent(1, 74, 100), 0);
        processor.processBlock(audioBuf, learnMidi);
        if (processor.getAssignedMidiCc(ParamIDs::ampDrive) != 74 || !processor.isCurrentPresetModified()) {
            std::cerr << "MIDI CC Learn or modified-param tracking failed!" << std::endl;
            return 5;
        }
        processor.revertToBaseline();
        if (processor.isCurrentPresetModified()) {
            std::cerr << "revertToBaseline() failed to clear modified state!" << std::endl;
            return 6;
        }

        juce::MemoryBlock stateBlob;
        processor.getStateInformation(stateBlob);
        if (stateBlob.getSize() == 0) {
            std::cerr << "State serialization failed!" << std::endl;
            return 2;
        }
        processor.clearMidiCcForTarget(ParamIDs::ampDrive);
        processor.setStateInformation(stateBlob.getData(), static_cast<int>(stateBlob.getSize()));
        if (processor.getAssignedMidiCc(ParamIDs::ampDrive) != 74) {
            std::cerr << "MIDI CC Learn XML state persistence failed!" << std::endl;
            return 7;
        }
        processor.clearMidiCcForTarget(ParamIDs::ampDrive);

        auto tempPreset = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("_nvps_audit_roundtrip.nvps");
        if (!processor.saveUserPreset(tempPreset) || !processor.loadUserPreset(tempPreset)) {
            tempPreset.deleteFile();
            std::cerr << "User preset .nvps round-trip failed!" << std::endl;
            return 3;
        }
        tempPreset.deleteFile();
        processor.loadPreset(14);
        processor.triggerChordAudition(0);
        processor.setLivePedalState(0, 0.85f);
        processor.setLivePedalState(1, 1.0f);
        for (int i = 0; i < 8; ++i) processor.processBlock(audioBuf, midiBuf);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    editor->setSize(1220, 720);

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
