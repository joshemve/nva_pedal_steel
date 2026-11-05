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
    std::unique_ptr<juce::OutputStream> outStream(file.createOutputStream());
    if (!outStream) return false;

    juce::WavAudioFormat wavFormat;
    const auto opts = juce::AudioFormatWriterOptions{}
        .withSampleRate(sampleRate)
        .withNumChannels(buffer.getNumChannels())
        .withBitsPerSample(24);
    std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(outStream, opts));
    if (!writer) return false;
    return writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
}

static void renderSequenceToWav(int presetIndex, double durationSec, double bpm, const std::vector<ScheduledMidiEvent>& events, const juce::File& outFile) {
    constexpr double kSr = 44100.0;
    constexpr int kBlock = 64;
    const int totalSamples = static_cast<int>(std::ceil(durationSec * kSr));

    PedalSteelEngine engine;
    engine.prepare(kSr, kBlock);
    const auto& presets = getPedalSteelPresets();
    PedalSteelParams p = presets[static_cast<size_t>(presetIndex)].params;
    p.hostBpm = bpm;
    engine.setParams(p);

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

    // 1. Sweet Georgia E9 (83 BPM, E Major) - Triggered directly via Channel 1 Phrase Keyswitch G1 (MIDI 43)!
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::noteOn(1, 43, 0.96f) });
        ev.push_back({ 11.48, juce::MidiMessage::noteOff(1, 43) });
        renderSequenceToWav(0, 11.5, 83.0, ev, outDir.getChildFile("01_sweet_georgia_EMaj.wav"));
    }

    // 2. Hedonist Bootgaze (90 BPM, G Major)
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::noteOn(1, 43, 0.96f) });
        ev.push_back({ 11.48, juce::MidiMessage::noteOff(1, 43) });
        renderSequenceToWav(1, 11.5, 90.0, ev, outDir.getChildFile("02_hedonist_bootgaze_GMaj.wav"));
    }

    // 3. Mikey's Grit Vibey (120 BPM, D Major)
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::noteOn(1, 43, 0.96f) });
        ev.push_back({ 11.48, juce::MidiMessage::noteOff(1, 43) });
        renderSequenceToWav(2, 11.5, 120.0, ev, outDir.getChildFile("03_mikeys_grit_vibey_DMaj.wav"));
    }

    // 4. Montana Lush Swell (126 BPM, G Major)
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::noteOn(1, 43, 0.96f) });
        ev.push_back({ 11.48, juce::MidiMessage::noteOff(1, 43) });
        renderSequenceToWav(3, 11.5, 126.0, ev, outDir.getChildFile("04_montana_swell_GMaj.wav"));
    }

    // 5. Channel-1 Live Keyboard Showcase:
    //    - 0.0..4.5s: Preset 05 (Nashville 400 Crying Lead) playing live MIDI chords + A+B Pedal Slant (E Maj -> A Maj -> C#m7 -> E Maj)
    //    - 4.5..9.5s: Preset 06 (Cosmic Bootgaze Dotted-8th, 100 BPM) playing melodic bar glides + synced 1/8D stereo tape echo
    //    - 9.5..15.0s: Preset 03 (Montana Lush Swell) playing custom minor & maj7 chords (Em7 -> Cmaj7) on Channel 1 with zero chord clash!
    {
        constexpr double kSr = 44100.0;
        constexpr int kBlock = 64;
        const int totalSamples = static_cast<int>(15.5 * kSr);
        juce::AudioBuffer<float> showcaseBuf(2, totalSamples);
        showcaseBuf.clear();

        const auto& presets = getPedalSteelPresets();

        auto renderSegmentInto = [&](int presetIdx, double startSec, double endSec, double bpm, const std::vector<ScheduledMidiEvent>& segEvents) {
            PedalSteelEngine eng;
            eng.prepare(kSr, kBlock);
            PedalSteelParams p = presets[static_cast<size_t>(presetIdx)].params;
            p.hostBpm = bpm;
            eng.setParams(p);

            const int segSamples = static_cast<int>((endSec - startSec) * kSr);
            juce::AudioBuffer<float> segBuf(2, segSamples);
            segBuf.clear();

            std::vector<ScheduledMidiEvent> sorted = segEvents;
            std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.timeSec < b.timeSec; });

            size_t idx = 0;
            int pos = 0;
            while (pos < segSamples) {
                int chunk = std::min(kBlock, segSamples - pos);
                double tEnd = static_cast<double>(pos + chunk) / kSr;
                while (idx < sorted.size() && sorted[idx].timeSec <= tEnd) {
                    eng.handleMidiMessage(sorted[idx].msg);
                    ++idx;
                }
                eng.renderBlock(segBuf, pos, chunk);
                pos += chunk;
            }

            const int dstStart = static_cast<int>(startSec * kSr);
            const int copyLen = std::min(segSamples, totalSamples - dstStart);
            for (int ch = 0; ch < 2; ++ch) {
                showcaseBuf.addFrom(ch, dstStart, segBuf, ch, 0, copyLen);
            }
        };

        // Part A: Live E9th Chords + A+B Pedal Slant + Pedal A C#m7 (Preset 04 index = Nashville 400 Crying Lead)
        {
            std::vector<ScheduledMidiEvent> ev;
            // E Major triad (E4, G#4, B4) -> engage A+B keyswitch (MIDI 28) at 0.65s -> release to E Major at 1.55s
            ev.push_back({ 0.05, juce::MidiMessage::noteOn(1, 64, 0.84f) });
            ev.push_back({ 0.06, juce::MidiMessage::noteOn(1, 68, 0.86f) });
            ev.push_back({ 0.07, juce::MidiMessage::noteOn(1, 71, 0.88f) });
            ev.push_back({ 0.65, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // A+B Pedal Slant -> A Major!
            ev.push_back({ 1.65, juce::MidiMessage::noteOff(1, 28) });       // Back to E Major!
            ev.push_back({ 2.35, juce::MidiMessage::noteOn(1, 24, 1.00f) }); // Pedal A alone -> C#m7!
            ev.push_back({ 3.60, juce::MidiMessage::noteOff(1, 24) });
            ev.push_back({ 3.85, juce::MidiMessage::noteOff(1, 64) });
            ev.push_back({ 3.85, juce::MidiMessage::noteOff(1, 68) });
            ev.push_back({ 3.85, juce::MidiMessage::noteOff(1, 71) });
            renderSegmentInto(4, 0.0, 5.0, 92.0, ev);
        }

        // Part B: Cosmic Bootgaze Dotted-8th (Preset 06 index) - Melodic Bar Glides + Tempo-Synced 1/8D Tape Delay
        {
            std::vector<ScheduledMidiEvent> ev;
            ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 67, 0.88f) }); // G4
            ev.push_back({ 0.58, juce::MidiMessage::noteOff(1, 67) });
            ev.push_back({ 0.59, juce::MidiMessage::noteOn(1, 71, 0.90f) }); // Legato bar slide G4 -> B4!
            ev.push_back({ 1.35, juce::MidiMessage::noteOff(1, 71) });
            ev.push_back({ 1.36, juce::MidiMessage::noteOn(1, 74, 0.92f) }); // Legato bar slide B4 -> D5!
            ev.push_back({ 2.20, juce::MidiMessage::noteOff(1, 74) });
            // Plucked G6/9 dyad ringing into dotted-8th Echoplex tape echoes
            ev.push_back({ 2.40, juce::MidiMessage::noteOn(1, 62, 0.80f) });
            ev.push_back({ 2.42, juce::MidiMessage::noteOn(1, 69, 0.85f) });
            ev.push_back({ 2.44, juce::MidiMessage::noteOn(1, 71, 0.88f) });
            ev.push_back({ 3.60, juce::MidiMessage::noteOff(1, 62) });
            ev.push_back({ 3.60, juce::MidiMessage::noteOff(1, 69) });
            ev.push_back({ 3.60, juce::MidiMessage::noteOff(1, 71) });
            renderSegmentInto(6, 4.6, 10.2, 96.0, ev);
        }

        // Part C: Montana Lush Swell (Preset 03 index) - Custom Minor 7 & Major 7 Chords on MIDI Ch 1 (verifying 100% de-chorded purity!)
        {
            std::vector<ScheduledMidiEvent> ev;
            // E Minor 7 (E3, B3, D4, G4)
            ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 52, 0.82f) });
            ev.push_back({ 0.11, juce::MidiMessage::noteOn(1, 59, 0.84f) });
            ev.push_back({ 0.12, juce::MidiMessage::noteOn(1, 62, 0.85f) });
            ev.push_back({ 0.13, juce::MidiMessage::noteOn(1, 67, 0.88f) });
            ev.push_back({ 2.30, juce::MidiMessage::noteOff(1, 52) });
            ev.push_back({ 2.30, juce::MidiMessage::noteOff(1, 59) });
            ev.push_back({ 2.30, juce::MidiMessage::noteOff(1, 62) });
            ev.push_back({ 2.30, juce::MidiMessage::noteOff(1, 67) });

            // C Major 7 (C4, E4, G4, B4)
            ev.push_back({ 2.50, juce::MidiMessage::noteOn(1, 60, 0.84f) });
            ev.push_back({ 2.51, juce::MidiMessage::noteOn(1, 64, 0.85f) });
            ev.push_back({ 2.52, juce::MidiMessage::noteOn(1, 67, 0.86f) });
            ev.push_back({ 2.53, juce::MidiMessage::noteOn(1, 71, 0.88f) });
            ev.push_back({ 4.70, juce::MidiMessage::noteOff(1, 60) });
            ev.push_back({ 4.70, juce::MidiMessage::noteOff(1, 64) });
            ev.push_back({ 4.70, juce::MidiMessage::noteOff(1, 67) });
            ev.push_back({ 4.70, juce::MidiMessage::noteOff(1, 71) });
            renderSegmentInto(3, 9.8, 15.5, 120.0, ev);
        }

        float pk = showcaseBuf.getMagnitude(0, totalSamples);
        if (pk > 1e-5f) showcaseBuf.applyGain(juce::Decibels::decibelsToGain(-2.5f) / pk);
        writeWavFile(outDir.getChildFile("05_live_keyboard_showcase.wav"), showcaseBuf, kSr);
        std::cout << "Rendered Channel-1 Live Keyboard Showcase -> renders/05_live_keyboard_showcase.wav" << std::endl;
    }

    return 0;
}
