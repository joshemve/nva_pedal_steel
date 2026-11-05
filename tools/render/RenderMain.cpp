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

    // 1. Sweet Georgia E9 (83 BPM, E Major) - Exact 100% diatonic E-Major transcription of CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav
    {
        std::vector<ScheduledMidiEvent> ev;
        auto noteOn  = [&](double t, int n, float v) { ev.push_back({ t, juce::MidiMessage::noteOn(1, n, v) }); };
        auto noteOff = [&](double t, int n)          { ev.push_back({ t, juce::MidiMessage::noteOff(1, n) }); };

        // Phrase 1: F#4 (66) bending up to G#4 (68) via B-pedal at 0.24s, then fresh pluck on F#4 at 2.17s sliding down to E4 (64) at 2.85s
        noteOn(0.02, 66, 0.84f);
        noteOff(0.23, 66);
        noteOn(0.24, 68, 0.86f); // Smooth B-pedal pull F#4 -> G#4
        noteOff(2.08, 68);       // Clean 90ms separation so F#4 at 2.17s is freshly plucked, not slid!

        noteOn(2.17, 66, 0.88f); // Fresh fingerpick on F#4
        noteOff(2.84, 66);
        noteOn(2.85, 64, 0.84f); // Direct E-Major slide F#4 -> E4
        noteOff(5.55, 64);

        // Phrase 2: High B4 (71) bell chime at 5.78s held as an oblique dyad while F#4 (66) -> G#4 (68) moves underneath,
        // then fresh B4 re-pluck at 7.96s while G#4 -> F#4 -> E4 resolves underneath!
        noteOn(5.78, 71, 0.86f); // String 5 (B4) rings out across the dyad!

        noteOn(6.50, 66, 0.86f); // Fresh pluck F#4 on String 1 under held B4
        noteOff(7.13, 66);
        noteOn(7.14, 68, 0.88f); // Oblique B-pedal pull F#4 -> G#4 under held B4

        noteOff(7.90, 71);
        noteOn(7.96, 71, 0.86f); // Fresh re-pluck on high B4 at 7.96s while G#4 stays held

        noteOff(8.29, 68);
        noteOn(8.30, 66, 0.84f); // Oblique B-pedal release G#4 -> F#4
        noteOff(8.68, 66);
        noteOn(8.69, 64, 0.82f); // Resolves cleanly to E4 under decaying B4
        noteOff(9.90, 71);
        noteOff(9.90, 64);

        // Pickup note at t=9.96s: E4 -> F#4 -> E4
        noteOn(9.96, 64, 0.78f);
        noteOff(9.98, 64);
        noteOn(9.99, 66, 0.84f);
        noteOff(10.74, 66);
        noteOn(10.75, 64, 0.78f);
        noteOff(11.10, 64);

        renderSequenceToWav(0, 11.5, ev, outDir.getChildFile("01_sweet_georgia_EMaj.wav"));
    }

    // 2. Hedonist Bootgaze (90 BPM, G Major) - Exact transcription of CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav
    {
        std::vector<ScheduledMidiEvent> ev;
        auto noteOn  = [&](double t, int n, float v) { ev.push_back({ t, juce::MidiMessage::noteOn(1, n, v) }); };
        auto noteOff = [&](double t, int n)          { ev.push_back({ t, juce::MidiMessage::noteOff(1, n) }); };

        // Phrase 1: A4 (69) bending up to B4 (71) via A-pedal, then D5 (74) sliding down to A4 (69)
        noteOn(0.02, 69, 0.88f);
        noteOff(0.56, 69);
        noteOn(0.57, 71, 0.92f); // Smooth crying A-pedal pull A4 -> B4
        noteOff(1.94, 71);
        noteOn(1.98, 74, 0.94f); // High D5 enters with metallic pick bite
        noteOff(2.58, 74);
        noteOn(2.59, 69, 0.88f); // Crying bar slide D5 -> A4 with warm 2nd-harmonic octave bloom
        noteOff(5.25, 69);

        // Phrase 2 (5.36s - 11.40s): C4 (60) -> D4 (62) -> G4 (67) -> A4 (69) -> B4 (71) -> E5 (76) -> F#5 (78) -> G5 (79)
        noteOn(5.36, 60, 0.90f); // Low-mid C4 (with natural MS_5_HB_C4 H2=C5 and H3=G5 octave bloom!)
        noteOff(5.88, 60);
        noteOn(5.89, 62, 0.88f); // Smooth legato slide C4 -> D4
        noteOn(6.70, 67, 0.90f); // G4 chime enters over ringing D4
        noteOff(7.90, 62);
        noteOff(7.90, 67);

        // 7.98s: Pure crying A4 (69) sliding smoothly up at 8.58s to B4 (71), then E5 -> F#5 -> G5 resolution!
        noteOn(7.98, 69, 0.92f);
        noteOff(8.57, 69);
        noteOn(8.58, 71, 0.90f); // Smooth A-pedal pull A4 -> B4 on the same string
        noteOn(10.02, 76, 0.88f); // High E5 chime enters at 10.02s over held B4
        noteOff(10.72, 76);
        noteOn(10.73, 78, 0.86f); // Slides E5 -> F#5 -> G5
        noteOff(10.85, 78);
        noteOn(10.86, 79, 0.88f);
        noteOff(11.35, 71);
        noteOff(11.35, 79);

        renderSequenceToWav(1, 11.5, ev, outDir.getChildFile("02_hedonist_bootgaze_GMaj.wav"));
    }

    // 3. Mikey's Grit Vibey (120 BPM, D Major) - Exact polyphonic dyad/triad glissando transcription
    {
        std::vector<ScheduledMidiEvent> ev;
        auto noteOn  = [&](double t, int n, float v) { ev.push_back({ t, juce::MidiMessage::noteOn(1, n, v) }); };
        auto noteOff = [&](double t, int n)          { ev.push_back({ t, juce::MidiMessage::noteOff(1, n) }); };
        auto bend = [&](double t, float semitones) {
            int pw = juce::jlimit(0, 16383, 8192 + static_cast<int>(std::round((semitones / 7.0f) * 8191.0f)));
            ev.push_back({ t, juce::MidiMessage::pitchWheel(1, pw) });
        };
        auto slideCurve = [&](double t0, double t1, float s0, float s1) {
            for (int k = 0; k <= 24; ++k) {
                double u = static_cast<double>(k) / 24.0;
                double shape = 0.5 * (1.0 - std::cos(3.14159265358979323846 * u));
                bend(t0 + (t1 - t0) * u, s0 + (s1 - s0) * static_cast<float>(shape));
            }
        };

        // Staggered pick entry: A5 (81) at 0.06s, then F#5 (78) at 0.54s!
        bend(0.0, 0.0f);
        noteOn(0.06, 81, 0.78f);
        noteOn(0.54, 78, 0.90f);

        // Smooth human tonebar slide down -7 semitones (1.02s -> 1.70s), brief pocket hold, then slide back up (1.96s -> 2.48s)
        slideCurve(1.02, 1.70, 0.0f, -7.0f);
        slideCurve(1.96, 2.48, -7.0f, 0.0f);
        noteOn(2.48, 74, 0.78f); // D5 joins F#5+A5 at top of resolution!
        // Let the resolved D5 + F#5 + A5 triad ring out naturally into the spring/plate canyon until 7.80s
        noteOff(5.80, 81);
        noteOff(6.40, 78);
        noteOff(7.80, 74);

        // Second phrase at t=7.98s: F#5 (78) plucked first, then A5 (81) at 8.55s, then crying -7 semitone slide to B4+D5 and back!
        bend(7.95, 0.0f);
        noteOn(7.98, 78, 0.92f);
        noteOn(8.55, 81, 0.84f);
        slideCurve(8.98, 9.76, 0.0f, -7.0f);
        slideCurve(9.98, 10.52, -7.0f, 0.0f);
        noteOn(10.52, 74, 0.74f);
        noteOff(11.40, 74);
        noteOff(11.40, 78);
        noteOff(11.40, 81);

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
