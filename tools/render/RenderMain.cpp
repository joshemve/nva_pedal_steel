#include <JuceHeader.h>
#include "Presets.h"
#include "dsp/PedalSteelEngine.h"
#include <vector>
#include <iostream>

struct ScheduledMidiEvent {
    double timeSec;
    juce::MidiMessage msg;
};

static bool writeWavFile(const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate) {
    file.getParentDirectory().createDirectory();
    if (file.existsAsFile()) file.deleteFile();
    std::unique_ptr<juce::FileOutputStream> fos(file.createOutputStream());
    if (!fos || !fos->openedOk()) return false;

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(fos.get(), sampleRate, static_cast<unsigned int>(buffer.getNumChannels()), 24, {}, 0));
    if (!writer) return false;
    fos.release();
    return writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
}

static void renderSequenceToWav(int presetIndex, double durationSec, const std::vector<ScheduledMidiEvent>& events, const juce::File& outFile) {
    constexpr double kSr = 44100.0;
    constexpr int kBlock = 64;
    const int totalSamples = static_cast<int>(std::ceil(durationSec * kSr));

    PedalSteelEngine engine;
    engine.prepare(kSr, kBlock);
    const auto& presets = getPedalSteelPresets();
    engine.setParams(presets[static_cast<size_t>(presetIndex)].params);

    std::vector<ScheduledMidiEvent> sorted = events;
    std::sort(sorted.begin(), sorted.end(), [](const ScheduledMidiEvent& a, const ScheduledMidiEvent& b) {
        return a.timeSec < b.timeSec;
    });

    juce::AudioBuffer<float> outBuf(2, totalSamples);
    outBuf.clear();

    size_t evIdx = 0;
    int pos = 0;
    while (pos < totalSamples) {
        int chunk = std::min(kBlock, totalSamples - pos);
        double tEnd = static_cast<double>(pos + chunk) / kSr;
        while (evIdx < sorted.size() && sorted[evIdx].timeSec <= tEnd) {
            engine.handleMidiMessage(sorted[evIdx].msg);
            ++evIdx;
        }
        engine.renderBlock(outBuf, pos, chunk);
        pos += chunk;
    }

    float peak = outBuf.getMagnitude(0, totalSamples);
    if (peak > 1e-5f) {
        float targetPeak = juce::Decibels::decibelsToGain(-3.0f);
        outBuf.applyGain(targetPeak / peak);
    }

    if (writeWavFile(outFile, outBuf, kSr)) {
        std::cout << "Rendered preset " << presetIndex << " -> " << outFile.getFullPathName() << std::endl;
    }
}

int main() {
    juce::File outDir = juce::File::getCurrentWorkingDirectory().getChildFile("renders");
    outDir.createDirectory();

    // 1. Sweet Georgia E9 (83 BPM, E Major) - 44.1 kHz Stereo Studio Phrase Articulation Performance
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::noteOn(2, 66, 0.92f) });
        ev.push_back({ 11.48, juce::MidiMessage::noteOff(2, 66) });
        renderSequenceToWav(0, 11.5, ev, outDir.getChildFile("01_sweet_georgia_EMaj.wav"));
    }

    // 2. Hedonist Bootgaze (90 BPM, G Major) - 44.1 kHz Stereo Studio Phrase Articulation Performance
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::noteOn(2, 69, 0.92f) });
        ev.push_back({ 11.48, juce::MidiMessage::noteOff(2, 69) });
        renderSequenceToWav(1, 11.5, ev, outDir.getChildFile("02_hedonist_bootgaze_GMaj.wav"));
    }

    // 3. Mikey's Grit Vibey (120 BPM, D Major) - 44.1 kHz Stereo Studio Phrase Articulation Performance
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::noteOn(2, 81, 0.92f) });
        ev.push_back({ 11.48, juce::MidiMessage::noteOff(2, 81) });
        renderSequenceToWav(2, 11.5, ev, outDir.getChildFile("03_mikeys_grit_vibey_DMaj.wav"));
    }

    // 4. Montana Lush Swell (126 BPM, G Major) - 44.1 kHz Stereo Studio Phrase Articulation Performance
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::noteOn(2, 67, 0.92f) });
        ev.push_back({ 11.48, juce::MidiMessage::noteOff(2, 67) });
        renderSequenceToWav(3, 11.5, ev, outDir.getChildFile("04_montana_swell_GMaj.wav"));
    }

    return 0;
}
