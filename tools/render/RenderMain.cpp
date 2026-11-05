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

    // 1. Sweet Georgia E9 (83 BPM, E Major) - Exact transcription of CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav
    {
        std::vector<ScheduledMidiEvent> ev;
        auto noteOn  = [&](double t, int n, float v) { ev.push_back({ t, juce::MidiMessage::noteOn(1, n, v) }); };
        auto noteOff = [&](double t, int n)          { ev.push_back({ t, juce::MidiMessage::noteOff(1, n) }); };

        // Phrase 1: F#4 (66) bending up to G#4 (68), then F#4 sliding down to E4 (64)
        noteOn(0.02, 66, 0.80f);
        noteOff(0.14, 66);
        noteOn(0.15, 68, 0.84f); // Legato B-pedal pull F#4 -> G#4
        noteOff(2.12, 68);

        noteOn(2.18, 66, 0.88f);
        noteOff(2.74, 66);
        noteOn(2.75, 65, 0.78f);
        noteOff(2.94, 65);
        noteOn(2.95, 64, 0.82f); // Resolves to E4
        noteOff(5.45, 64);

        // Phrase 2: High B4 (71) stationary anchor while F#4 (66) bends obliquely to G#4 (68) and back to E4 (64)
        noteOn(5.78, 71, 0.84f);
        noteOn(6.50, 66, 0.86f);
        noteOff(7.14, 66);
        noteOn(7.15, 68, 0.86f); // Oblique pedal raise F#4 -> G#4 under stationary B4!
        noteOff(7.96, 71);
        noteOn(7.98, 71, 0.86f); // Re-plucked high B4 at t=7.98s before oblique release!
        noteOff(8.22, 68);
        noteOn(8.23, 66, 0.84f); // Oblique release G#4 -> F#4
        noteOff(8.56, 66);
        noteOn(8.57, 65, 0.76f);
        noteOff(8.75, 65);
        noteOn(8.76, 64, 0.80f); // Resolves to E4 + B4
        noteOff(9.92, 64);
        noteOff(9.92, 71);

        // Pickup note at t=9.96s: E4 -> F#4
        noteOn(9.96, 64, 0.76f);
        noteOff(10.08, 64);
        noteOn(10.09, 66, 0.82f);
        noteOff(10.60, 66);

        renderSequenceToWav(0, 11.5, ev, outDir.getChildFile("01_sweet_georgia_EMaj.wav"));
    }

    // 2. Hedonist Bootgaze (90 BPM, G Major) - Exact transcription of CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav
    {
        std::vector<ScheduledMidiEvent> ev;
        auto noteOn  = [&](double t, int n, float v) { ev.push_back({ t, juce::MidiMessage::noteOn(1, n, v) }); };
        auto noteOff = [&](double t, int n)          { ev.push_back({ t, juce::MidiMessage::noteOff(1, n) }); };

        // Phrase 1: A4 (69) bending up to B4 (71) via A-pedal, then D5 (74) sliding down to A4 (69)
        noteOn(0.02, 69, 0.82f);
        noteOff(0.56, 69);
        noteOn(0.57, 71, 0.86f); // Smooth A-pedal pull A4 -> B4
        noteOn(1.98, 74, 0.88f); // High D5 enters while B4 rings
        noteOff(2.38, 71);
        noteOff(2.55, 74);
        noteOn(2.56, 69, 0.82f); // Resolves to A4 + D5 dyad
        noteOn(2.60, 74, 0.70f);
        noteOff(5.25, 69);
        noteOff(5.25, 74);

        // Phrase 2 (5.38s - 10.50s): C4 (60) + G4 (67) -> D4 (62) -> G4 (67) + D5 (74) -> A3/A4 sliding up to B3/B4
        noteOn(5.38, 60, 0.88f);
        noteOn(5.40, 67, 0.78f);
        noteOff(5.96, 60);
        noteOn(5.97, 62, 0.84f); // Legato slide C4 -> D4
        noteOff(6.72, 67);
        noteOn(6.74, 67, 0.84f); // G4 + D5 chime at 6.74s
        noteOn(6.76, 74, 0.76f);
        noteOff(7.92, 62);
        noteOff(7.92, 67);
        noteOff(7.92, 74);

        // 8.00s: A3 (57) + A4 (69) + E5 (76) sliding up at 8.62s to B3 (59) + B4 (71)
        noteOn(7.98, 57, 0.84f);
        noteOn(8.00, 69, 0.88f);
        noteOn(8.02, 76, 0.76f);
        noteOff(8.60, 57);
        noteOff(8.60, 69);
        noteOn(8.61, 59, 0.84f);
        noteOn(8.62, 71, 0.86f);
        noteOff(10.60, 59);
        noteOff(10.60, 71);
        noteOff(10.60, 76);

        renderSequenceToWav(1, 11.5, ev, outDir.getChildFile("02_hedonist_bootgaze_GMaj.wav"));
    }

    // 3. Mikey's Grit Vibey (120 BPM, D Major) - Exact polyphonic dyad/triad glissando transcription
    {
        std::vector<ScheduledMidiEvent> ev;
        auto noteOn  = [&](double t, int n, float v) { ev.push_back({ t, juce::MidiMessage::noteOn(1, n, v) }); };
        auto noteOff = [&](double t, int n)          { ev.push_back({ t, juce::MidiMessage::noteOff(1, n) }); };
        auto bend    = [&](double t, float semitones) {
            int pw = juce::jlimit(0, 16383, 8192 + static_cast<int>(std::round((semitones / 7.0f) * 8191.0f)));
            ev.push_back({ t, juce::MidiMessage::pitchWheel(1, pw) });
        };

        // Staggered pick entry: A5 (81) at 0.06s, then F#5 (78) at 0.54s!
        bend(0.0, 0.0f);
        noteOn(0.06, 81, 0.78f);
        noteOn(0.54, 78, 0.90f);

        // Smooth 2nd-order tonebar slide down -7 semitones from F#5+A5 to B4+D5 (t=0.95s), hold, then slide back up to D5+F#5+A5 (t=2.05s)
        bend(0.95, -7.0f);
        bend(2.05, 0.0f);
        noteOn(2.45, 74, 0.78f); // D5 joins F#5+A5 at top of resolution!
        // Let the resolved D5 + F#5 + A5 triad ring out naturally into the spring/plate canyon until 7.80s
        noteOff(5.80, 81);
        noteOff(6.40, 78);
        noteOff(7.80, 74);

        // Second phrase at t=7.98s: F#5 (78) plucked first, then A5 (81) at 8.55s, then crying -7 semitone slide to B4+D5 and back!
        bend(7.95, 0.0f);
        noteOn(7.98, 78, 0.92f);
        noteOn(8.55, 81, 0.84f);
        bend(8.95, -7.0f);
        bend(9.90, 0.0f);
        noteOn(10.35, 74, 0.74f);
        noteOff(11.30, 74);
        noteOff(11.30, 78);
        noteOff(11.30, 81);

        renderSequenceToWav(2, 11.8, ev, outDir.getChildFile("03_mikeys_grit_vibey_DMaj.wav"));
    }

    // 4. Montana Lush Swell (126 BPM, G Major) - Exact transcription with Goodrich Volume Pedal Swells
    {
        std::vector<ScheduledMidiEvent> ev;
        auto noteOn  = [&](double t, int n, float v) { ev.push_back({ t, juce::MidiMessage::noteOn(1, n, v) }); };
        auto noteOff = [&](double t, int n)          { ev.push_back({ t, juce::MidiMessage::noteOff(1, n) }); };
        auto expr    = [&](double t, float norm) {
            ev.push_back({ t, juce::MidiMessage::controllerEvent(1, 11, juce::jlimit(0, 127, static_cast<int>(norm * 127.0f))) });
        };

        // Helper for continuous volume pedal swell in & gentle taper (never dropping to dead silence between chords!)
        auto swellChord = [&](double tStart, double tPeak, double tEnd) {
            for (int k = 0; k <= 20; ++k) {
                double t = tStart + (tPeak - tStart) * (k / 20.0);
                float u = static_cast<float>(k) / 20.0f;
                expr(t, 0.25f + 0.75f * u);
            }
            for (int k = 0; k <= 20; ++k) {
                double t = tPeak + (tEnd - tPeak) * (k / 20.0);
                float u = static_cast<float>(k) / 20.0f;
                expr(t, 1.0f - 0.52f * u);
            }
        };

        // Chord 1 (0.05s - 3.95s): Gsus2 (G3 + D4 + G4 + A4) swelling into G major (A4 -> B4 via Pedal A, then releasing B4 -> A4 at 3.30s)
        swellChord(0.05, 1.35, 3.92);
        noteOn(0.05, 55, 0.80f);
        noteOn(0.06, 62, 0.84f);
        noteOn(0.07, 67, 0.86f);
        noteOn(0.08, 69, 0.84f);
        noteOff(1.12, 69);
        noteOn(1.13, 71, 0.86f); // Oblique A4 -> B4 while G3, D4, G4 stay anchored!
        noteOff(3.30, 71);
        noteOn(3.31, 69, 0.76f); // Gentle pedal release B4 -> A4 at end of bar 2!
        noteOff(3.94, 55);
        noteOff(3.94, 62);
        noteOff(3.94, 67);
        noteOff(3.94, 69);

        // Chord 2 (3.96s - 7.75s): Dsus2 (D4 + A4 + E5) swelling into D major (E5 -> F#5)
        swellChord(3.96, 5.05, 7.72);
        noteOn(3.96, 62, 0.84f);
        noteOn(3.97, 69, 0.86f);
        noteOn(3.98, 76, 0.86f);
        noteOff(4.82, 76);
        noteOn(4.83, 78, 0.88f); // Oblique E5 -> F#5
        noteOff(7.74, 62);
        noteOff(7.74, 69);
        noteOff(7.74, 78);

        // Chord 3 (7.76s - 11.20s): Emadd11 -> Em7 (E4 + B4 + A5 resolving A5 -> G5 at 8.55s)
        swellChord(7.76, 8.85, 11.00);
        noteOn(7.76, 64, 0.84f);
        noteOn(7.77, 71, 0.86f);
        noteOn(7.78, 81, 0.84f);
        noteOff(8.54, 81);
        noteOn(8.55, 79, 0.86f); // Oblique A5 -> G5 resolution
        noteOff(11.00, 64);
        noteOff(11.00, 71);
        noteOff(11.00, 79);

        renderSequenceToWav(3, 11.8, ev, outDir.getChildFile("04_montana_swell_GMaj.wav"));
    }

    return 0;
}
