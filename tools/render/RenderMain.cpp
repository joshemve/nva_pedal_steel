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

    // Smooth 35 ms fade-out guard at end of render so reverb/delay tails never clip off abruptly
    const int fadeSamples = std::min(totalSamples / 4, static_cast<int>(0.035 * kSr));
    for (int i = 0; i < fadeSamples; ++i) {
        const float g = static_cast<float>(i) / static_cast<float>(fadeSamples);
        const int idx = totalSamples - 1 - i;
        outBuf.setSample(0, idx, outBuf.getSample(0, idx) * g);
        outBuf.setSample(1, idx, outBuf.getSample(1, idx) * g);
    }

    float peak = outBuf.getMagnitude(0, totalSamples);
    if (peak > 1e-5f) {
        float targetPeak = juce::Decibels::decibelsToGain(-2.5f);
        outBuf.applyGain(targetPeak / peak);
    }

    if (writeWavFile(outFile, outBuf, kSr)) {
        std::cout << "Rendered preset " << (presetIndex + 1) << " -> " << outFile.getFullPathName() << std::endl;
    }
}

static void addPitchWheelRamp(std::vector<ScheduledMidiEvent>& ev, int ch, double t0, double t1, float normStart, float normEnd, int steps = 24) {
    for (int s = 0; s <= steps; ++s) {
        const double u = static_cast<double>(s) / static_cast<double>(steps);
        const double t = t0 + u * (t1 - t0);
        const float norm = juce::jlimit(-1.0f, 1.0f, normStart + static_cast<float>(u) * (normEnd - normStart));
        const int pwVal = juce::jlimit(0, 16383, static_cast<int>(std::round((norm * 0.5f + 0.5f) * 16383.0f)));
        ev.push_back({ t, juce::MidiMessage::pitchWheel(ch, pwVal) });
    }
}

static void addCcRamp(std::vector<ScheduledMidiEvent>& ev, int ch, int cc, double t0, double t1, float v0, float v1, int steps = 18) {
    for (int s = 0; s <= steps; ++s) {
        const double u = static_cast<double>(s) / static_cast<double>(steps);
        const double t = t0 + u * (t1 - t0);
        const int val = juce::jlimit(0, 127, static_cast<int>(std::round((v0 + static_cast<float>(u) * (v1 - v0)) * 127.0f)));
        ev.push_back({ t, juce::MidiMessage::controllerEvent(ch, cc, val) });
    }
}

int main() {
    juce::File outDir = juce::File::getCurrentWorkingDirectory().getChildFile("renders");
    juce::File presetsDir = outDir.getChildFile("presets");
    outDir.createDirectory();
    presetsDir.createDirectory();

    // =========================================================================
    // Presets 01 - 04: Reference Match Studio Performances
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev = {
            { 0.00, juce::MidiMessage::noteOn(1, 43, 0.96f) },
            { 11.45, juce::MidiMessage::noteOff(1, 43) }
        };
        renderSequenceToWav(0, 11.5, 83.0, ev, outDir.getChildFile("01_sweet_georgia_EMaj.wav"));
        renderSequenceToWav(0, 11.5, 83.0, ev, presetsDir.getChildFile("01_sweet_georgia_e9.wav"));
    }
    {
        std::vector<ScheduledMidiEvent> ev = {
            { 0.00, juce::MidiMessage::noteOn(1, 43, 0.96f) },
            { 11.45, juce::MidiMessage::noteOff(1, 43) }
        };
        renderSequenceToWav(1, 11.5, 90.0, ev, outDir.getChildFile("02_hedonist_bootgaze_GMaj.wav"));
        renderSequenceToWav(1, 11.5, 90.0, ev, presetsDir.getChildFile("02_hedonist_bootgaze.wav"));
    }
    {
        std::vector<ScheduledMidiEvent> ev = {
            { 0.00, juce::MidiMessage::noteOn(1, 43, 0.96f) },
            { 11.45, juce::MidiMessage::noteOff(1, 43) }
        };
        renderSequenceToWav(2, 11.5, 120.0, ev, outDir.getChildFile("03_mikeys_grit_vibey_DMaj.wav"));
        renderSequenceToWav(2, 11.5, 120.0, ev, presetsDir.getChildFile("03_mikeys_grit_vibey.wav"));
    }
    {
        std::vector<ScheduledMidiEvent> ev = {
            { 0.00, juce::MidiMessage::noteOn(1, 43, 0.96f) },
            { 11.45, juce::MidiMessage::noteOff(1, 43) }
        };
        renderSequenceToWav(3, 11.5, 126.0, ev, outDir.getChildFile("04_montana_swell_GMaj.wav"));
        renderSequenceToWav(3, 11.5, 126.0, ev, presetsDir.getChildFile("04_montana_lush_swell.wav"));
    }

    // =========================================================================
    // Preset 05: Nashville 400 Crying Lead (92 BPM, E Major)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        // Phrase 1: Plucked E Major shell (E4, G#4, B4) -> A+B Pedal Slant to A Major -> cascading release
        ev.push_back({ 0.08, juce::MidiMessage::noteOn(1, 64, 0.84f) });
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 68, 0.87f) });
        ev.push_back({ 0.12, juce::MidiMessage::noteOn(1, 71, 0.90f) });
        ev.push_back({ 0.72, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // A+B Pedals -> A Major
        ev.push_back({ 2.05, juce::MidiMessage::noteOff(1, 28) });       // Release back to E Major
        ev.push_back({ 3.20, juce::MidiMessage::noteOff(1, 64) });
        ev.push_back({ 3.20, juce::MidiMessage::noteOff(1, 68) });
        ev.push_back({ 3.20, juce::MidiMessage::noteOff(1, 71) });

        // Phrase 2: Crying high E9th single-string lead (G#4 -> B4 -> E5 -> weeping bar bend -> B4 -> G#4+E5 sixths)
        ev.push_back({ 3.55, juce::MidiMessage::noteOn(1, 68, 0.88f) });
        ev.push_back({ 4.15, juce::MidiMessage::noteOff(1, 68) });
        ev.push_back({ 4.16, juce::MidiMessage::noteOn(1, 71, 0.91f) }); // Legato bar glide G#4 -> B4
        ev.push_back({ 4.95, juce::MidiMessage::noteOff(1, 71) });
        ev.push_back({ 4.96, juce::MidiMessage::noteOn(1, 76, 0.92f) }); // Legato bar glide B4 -> E5
        addPitchWheelRamp(ev, 1, 5.55, 6.05, 0.0f, -2.0f / 7.0f);        // Crying bar slide E5 -> D5
        addPitchWheelRamp(ev, 1, 6.25, 6.60, -2.0f / 7.0f, -4.0f / 7.0f);// Down to C5 -> C#5
        ev.push_back({ 6.75, juce::MidiMessage::noteOff(1, 76) });
        ev.push_back({ 6.78, juce::MidiMessage::pitchWheel(1, 8192) });

        // Final resolving E Major chord with B-pedal grace suspension
        ev.push_back({ 6.85, juce::MidiMessage::noteOn(1, 26, 1.00f) }); // Pre-hold Pedal B (Esus4)
        ev.push_back({ 6.88, juce::MidiMessage::noteOn(1, 59, 0.80f) });
        ev.push_back({ 6.90, juce::MidiMessage::noteOn(1, 64, 0.84f) });
        ev.push_back({ 6.92, juce::MidiMessage::noteOn(1, 68, 0.88f) });
        ev.push_back({ 6.94, juce::MidiMessage::noteOn(1, 71, 0.89f) });
        ev.push_back({ 7.55, juce::MidiMessage::noteOff(1, 26) });       // Resolve Esus4 -> E Major!
        ev.push_back({ 9.30, juce::MidiMessage::noteOff(1, 59) });
        ev.push_back({ 9.30, juce::MidiMessage::noteOff(1, 64) });
        ev.push_back({ 9.30, juce::MidiMessage::noteOff(1, 68) });
        ev.push_back({ 9.30, juce::MidiMessage::noteOff(1, 71) });

        renderSequenceToWav(4, 10.6, 92.0, ev, presetsDir.getChildFile("05_nashville_400_crying_lead.wav"));
    }

    // =========================================================================
    // Preset 06: Bakersfield Tape Slapback (124 BPM, A Major)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        // Snappy Buck Owens / Ralph Mooney country sixths & pedal-flick double-stops in A Major
        // Pickup walk-in: E4 -> F#4 -> A Major sixths (C#4 + A4)
        ev.push_back({ 0.08, juce::MidiMessage::noteOn(1, 64, 0.88f) });
        ev.push_back({ 0.28, juce::MidiMessage::noteOff(1, 64) });
        ev.push_back({ 0.31, juce::MidiMessage::noteOn(1, 66, 0.90f) });
        ev.push_back({ 0.51, juce::MidiMessage::noteOff(1, 66) });

        // Bright A Major double-stop (E4 + C#5) with fast Pedal B country bounce (C#5 -> D5 -> C#5)
        ev.push_back({ 0.55, juce::MidiMessage::noteOn(1, 64, 0.91f) });
        ev.push_back({ 0.57, juce::MidiMessage::noteOn(1, 73, 0.93f) });
        ev.push_back({ 0.98, juce::MidiMessage::noteOn(1, 26, 1.00f) }); // Quick B-pedal flick!
        ev.push_back({ 1.38, juce::MidiMessage::noteOff(1, 26) });
        ev.push_back({ 1.85, juce::MidiMessage::noteOff(1, 64) });
        ev.push_back({ 1.85, juce::MidiMessage::noteOff(1, 73) });

        // Staccato slapback sixth stabs walking down: F#m (A4+F#5) -> E (G#4+E5) -> D (F#4+D5) -> A
        auto addSixthStab = [&](double t, double dur, int n1, int n2, float v) {
            ev.push_back({ t, juce::MidiMessage::noteOn(1, n1, v) });
            ev.push_back({ t + 0.018, juce::MidiMessage::noteOn(1, n2, v + 0.02f) });
            ev.push_back({ t + dur, juce::MidiMessage::noteOff(1, n1) });
            ev.push_back({ t + dur, juce::MidiMessage::noteOff(1, n2) });
        };
        addSixthStab(2.05, 0.34, 69, 78, 0.90f); // A4 + F#5
        addSixthStab(2.54, 0.32, 68, 76, 0.88f); // G#4 + E5
        addSixthStab(3.02, 0.36, 66, 74, 0.91f); // F#4 + D5
        addSixthStab(3.52, 0.28, 64, 73, 0.89f); // E4 + C#5

        // Twangy E7 turnaround lick -> bright A Major triad + A+B slant tag
        ev.push_back({ 4.02, juce::MidiMessage::noteOn(1, 62, 0.88f) }); // D4
        ev.push_back({ 4.04, juce::MidiMessage::noteOn(1, 68, 0.91f) }); // G#4
        addPitchWheelRamp(ev, 1, 4.18, 4.45, 0.0f, 2.0f / 7.0f);         // Slide E7 up +2 semis!
        ev.push_back({ 4.78, juce::MidiMessage::noteOff(1, 62) });
        ev.push_back({ 4.78, juce::MidiMessage::noteOff(1, 68) });
        ev.push_back({ 4.82, juce::MidiMessage::pitchWheel(1, 8192) });

        // Final ringing A Major triad (A3, E4, A4, C#5) with quick A+B IV-I country tag
        ev.push_back({ 4.90, juce::MidiMessage::noteOn(1, 57, 0.85f) });
        ev.push_back({ 4.92, juce::MidiMessage::noteOn(1, 64, 0.88f) });
        ev.push_back({ 4.94, juce::MidiMessage::noteOn(1, 69, 0.90f) });
        ev.push_back({ 4.96, juce::MidiMessage::noteOn(1, 73, 0.92f) });
        ev.push_back({ 5.48, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // Quick D Major tag
        ev.push_back({ 6.05, juce::MidiMessage::noteOff(1, 28) });       // Back to A Major!
        ev.push_back({ 7.60, juce::MidiMessage::noteOff(1, 57) });
        ev.push_back({ 7.60, juce::MidiMessage::noteOff(1, 64) });
        ev.push_back({ 7.60, juce::MidiMessage::noteOff(1, 69) });
        ev.push_back({ 7.60, juce::MidiMessage::noteOff(1, 73) });

        renderSequenceToWav(5, 8.8, 124.0, ev, presetsDir.getChildFile("06_bakersfield_tape_slapback.wav"));
    }

    // =========================================================================
    // Preset 07: Cosmic Bootgaze Dotted-8th (96 BPM, G Major)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        // Dreamy soaring bar glides + open Americana chords cascading into 1/8D Echoplex echoes
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 67, 0.88f) }); // G4
        ev.push_back({ 0.68, juce::MidiMessage::noteOff(1, 67) });
        ev.push_back({ 0.69, juce::MidiMessage::noteOn(1, 71, 0.90f) }); // Glide G4 -> B4
        ev.push_back({ 1.45, juce::MidiMessage::noteOff(1, 71) });
        ev.push_back({ 1.46, juce::MidiMessage::noteOn(1, 74, 0.92f) }); // Glide B4 -> D5
        ev.push_back({ 2.25, juce::MidiMessage::noteOff(1, 74) });
        ev.push_back({ 2.26, juce::MidiMessage::noteOn(1, 78, 0.91f) }); // Glide D5 -> F#5
        ev.push_back({ 3.35, juce::MidiMessage::noteOff(1, 78) });

        // Open Em9 voicing (E3, B3, F#4, G4, D5) blooming into dotted-8th echoes
        ev.push_back({ 3.65, juce::MidiMessage::noteOn(1, 59, 0.82f) });
        ev.push_back({ 3.68, juce::MidiMessage::noteOn(1, 66, 0.85f) });
        ev.push_back({ 3.71, juce::MidiMessage::noteOn(1, 71, 0.88f) });
        ev.push_back({ 3.74, juce::MidiMessage::noteOn(1, 74, 0.90f) });
        ev.push_back({ 5.60, juce::MidiMessage::noteOff(1, 59) });
        ev.push_back({ 5.60, juce::MidiMessage::noteOff(1, 66) });
        ev.push_back({ 5.60, juce::MidiMessage::noteOff(1, 71) });
        ev.push_back({ 5.60, juce::MidiMessage::noteOff(1, 74) });

        // Cmaj7 -> G6/9 pedal bloom
        ev.push_back({ 5.95, juce::MidiMessage::noteOn(1, 60, 0.83f) });
        ev.push_back({ 5.98, juce::MidiMessage::noteOn(1, 67, 0.86f) });
        ev.push_back({ 6.01, juce::MidiMessage::noteOn(1, 71, 0.89f) });
        ev.push_back({ 6.04, juce::MidiMessage::noteOn(1, 76, 0.91f) });
        ev.push_back({ 7.10, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // Oblique pedal lift
        ev.push_back({ 8.55, juce::MidiMessage::noteOff(1, 28) });
        ev.push_back({ 8.80, juce::MidiMessage::noteOff(1, 60) });
        ev.push_back({ 8.80, juce::MidiMessage::noteOff(1, 67) });
        ev.push_back({ 8.80, juce::MidiMessage::noteOff(1, 71) });
        ev.push_back({ 8.80, juce::MidiMessage::noteOff(1, 76) });

        renderSequenceToWav(6, 10.8, 96.0, ev, presetsDir.getChildFile("07_cosmic_bootgaze_dotted_8th.wav"));
    }

    // =========================================================================
    // Preset 08: Slow-Gear Dream Steel (Ambient, 80 BPM, D Major)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        // Lanois / Eno cathedral auto-swells: Dmaj7 -> Gmaj9 -> Bm7 -> Aadd9 -> Dmaj9
        auto addSwellChord = [&](double t, double dur, std::initializer_list<int> notes) {
            double k = 0.0;
            for (int n : notes) {
                ev.push_back({ t + k, juce::MidiMessage::noteOn(1, n, 0.86f) });
                ev.push_back({ t + dur, juce::MidiMessage::noteOff(1, n) });
                k += 0.02;
            }
        };
        addSwellChord(0.10, 2.75, { 50, 57, 66, 73 }); // Dmaj7 (D3, A3, F#4, C#5)
        addSwellChord(3.05, 2.75, { 55, 62, 66, 71 }); // Gmaj7 (G3, D4, F#4, B4)
        addSwellChord(6.00, 2.55, { 59, 66, 69, 74 }); // Bm7   (B3, F#4, A4, D5)
        addSwellChord(8.75, 2.85, { 50, 62, 66, 69 }); // D Major warm resolution
        ev.push_back({ 9.55, juce::MidiMessage::noteOn(1, 28, 1.00f) });  // Gentle G/D cathedral bloom
        ev.push_back({ 10.65, juce::MidiMessage::noteOff(1, 28) });

        renderSequenceToWav(7, 12.4, 80.0, ev, presetsDir.getChildFile("08_slow_gear_dream_steel.wav"));
    }

    // =========================================================================
    // Preset 09: Cryin' In My Beer Ballad (68 BPM, C Major)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        // Slow vocal bar slide + stainless winding sheen + LKR heartbreak passing chord
        ev.push_back({ 0.12, juce::MidiMessage::noteOn(1, 67, 0.86f) }); // G4
        ev.push_back({ 0.14, juce::MidiMessage::noteOn(1, 76, 0.90f) }); // E5 (sixth dyad)
        addPitchWheelRamp(ev, 1, 0.75, 1.45, 0.0f, -2.0f / 7.0f);        // Slow weeping bar slide down 2 frets
        addPitchWheelRamp(ev, 1, 1.85, 2.45, -2.0f / 7.0f, 0.0f);        // Cry back up to C Major sixth
        ev.push_back({ 3.10, juce::MidiMessage::noteOff(1, 67) });
        ev.push_back({ 3.10, juce::MidiMessage::noteOff(1, 76) });

        // F Major -> Fm6 / C Major ballad cadence using A+B slant and LKR knee lever
        ev.push_back({ 3.35, juce::MidiMessage::noteOn(1, 60, 0.84f) }); // C4
        ev.push_back({ 3.37, juce::MidiMessage::noteOn(1, 64, 0.86f) }); // E4
        ev.push_back({ 3.39, juce::MidiMessage::noteOn(1, 67, 0.88f) }); // G4
        ev.push_back({ 3.41, juce::MidiMessage::noteOn(1, 72, 0.90f) }); // C5
        ev.push_back({ 4.10, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // Pull A+B -> F Major!
        ev.push_back({ 5.65, juce::MidiMessage::noteOff(1, 28) });       // Weep back to C Major
        ev.push_back({ 6.60, juce::MidiMessage::noteOn(1, 31, 1.00f) }); // LKR lowers C -> B (G7/B tension)
        ev.push_back({ 7.65, juce::MidiMessage::noteOff(1, 31) });       // Resolve home to C Major
        ev.push_back({ 9.40, juce::MidiMessage::noteOff(1, 60) });
        ev.push_back({ 9.40, juce::MidiMessage::noteOff(1, 64) });
        ev.push_back({ 9.40, juce::MidiMessage::noteOff(1, 67) });
        ev.push_back({ 9.40, juce::MidiMessage::noteOff(1, 72) });

        renderSequenceToWav(8, 11.0, 68.0, ev, presetsDir.getChildFile("09_cryin_in_my_beer_ballad.wav"));
    }

    // =========================================================================
    // Preset 10: Hot Tweed Roadhouse Grit (112 BPM, D Major)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        // Overdriven '57 Tweed Deluxe double-stop bends & gritty Southern-rock lead in D
        // Bluesy slide from C5+E5 into D5+F#5!
        ev.push_back({ 0.08, juce::MidiMessage::pitchWheel(1, static_cast<int>(8192 - (2.0 / 7.0) * 8191)) });
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 62, 0.95f) }); // D4 root anchor
        ev.push_back({ 0.12, juce::MidiMessage::noteOn(1, 74, 0.97f) }); // D5
        ev.push_back({ 0.14, juce::MidiMessage::noteOn(1, 78, 0.98f) }); // F#5
        addPitchWheelRamp(ev, 1, 0.16, 0.48, -2.0f / 7.0f, 0.0f);        // Aggressive bar slide into D Major!
        ev.push_back({ 1.55, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // Grit slant to G Major!
        ev.push_back({ 2.35, juce::MidiMessage::noteOff(1, 28) });
        ev.push_back({ 3.10, juce::MidiMessage::noteOff(1, 62) });
        ev.push_back({ 3.10, juce::MidiMessage::noteOff(1, 74) });
        ev.push_back({ 3.10, juce::MidiMessage::noteOff(1, 78) });

        // High Tweed lead lick: A5 -> F#5 -> D5 -> B4 -> D5+A5 power dyad howl
        ev.push_back({ 3.30, juce::MidiMessage::noteOn(1, 81, 0.98f) }); // A5 grit!
        ev.push_back({ 4.05, juce::MidiMessage::noteOff(1, 81) });
        ev.push_back({ 4.06, juce::MidiMessage::noteOn(1, 78, 0.95f) }); // F#5
        ev.push_back({ 4.60, juce::MidiMessage::noteOff(1, 78) });
        ev.push_back({ 4.61, juce::MidiMessage::noteOn(1, 74, 0.94f) }); // D5
        ev.push_back({ 5.15, juce::MidiMessage::noteOff(1, 74) });
        ev.push_back({ 5.20, juce::MidiMessage::noteOn(1, 71, 0.92f) }); // B4
        ev.push_back({ 5.65, juce::MidiMessage::noteOff(1, 71) });

        // Final pushed D5 open-fifth + third chord (D4, A4, D5, F#5) with B-pedal curl
        ev.push_back({ 5.72, juce::MidiMessage::noteOn(1, 26, 1.00f) });
        ev.push_back({ 5.75, juce::MidiMessage::noteOn(1, 62, 0.94f) });
        ev.push_back({ 5.77, juce::MidiMessage::noteOn(1, 69, 0.95f) });
        ev.push_back({ 5.79, juce::MidiMessage::noteOn(1, 74, 0.96f) });
        ev.push_back({ 5.81, juce::MidiMessage::noteOn(1, 78, 0.97f) });
        ev.push_back({ 6.20, juce::MidiMessage::noteOff(1, 26) });
        ev.push_back({ 8.10, juce::MidiMessage::noteOff(1, 62) });
        ev.push_back({ 8.10, juce::MidiMessage::noteOff(1, 69) });
        ev.push_back({ 8.10, juce::MidiMessage::noteOff(1, 74) });
        ev.push_back({ 8.10, juce::MidiMessage::noteOff(1, 78) });

        renderSequenceToWav(9, 9.4, 112.0, ev, presetsDir.getChildFile("10_hot_tweed_roadhouse_grit.wav"));
    }

    // =========================================================================
    // Preset 11: Manual 10-String E9th Workstation (86 BPM, E Major Copedent Tour)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        // Full mechanical tour on a held E9th grip (E3, B3, E4, G#4, B4):
        // E Maj -> CC66 Pedal B (Esus4) -> +CC65 Pedal A (A Maj IV) -> release B (C#m7 vi) -> LKR (B7/D#) -> E Maj
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 52, 0.82f) });
        ev.push_back({ 0.12, juce::MidiMessage::noteOn(1, 59, 0.84f) });
        ev.push_back({ 0.14, juce::MidiMessage::noteOn(1, 64, 0.86f) });
        ev.push_back({ 0.16, juce::MidiMessage::noteOn(1, 68, 0.88f) });
        ev.push_back({ 0.18, juce::MidiMessage::noteOn(1, 71, 0.90f) });

        addCcRamp(ev, 1, 66, 1.25, 1.45, 0.0f, 1.0f); // Engage Pedal B -> Esus4
        addCcRamp(ev, 1, 65, 2.55, 2.75, 0.0f, 1.0f); // Add Pedal A    -> A Major (IV)
        addCcRamp(ev, 1, 66, 4.05, 4.25, 1.0f, 0.0f); // Release B      -> C#m7 (vi)
        addCcRamp(ev, 1, 65, 5.45, 5.65, 1.0f, 0.0f); // Release A      -> E Major (I)

        ev.push_back({ 6.35, juce::MidiMessage::noteOff(1, 52) });
        ev.push_back({ 6.35, juce::MidiMessage::noteOff(1, 59) });
        ev.push_back({ 6.35, juce::MidiMessage::noteOff(1, 64) });
        ev.push_back({ 6.35, juce::MidiMessage::noteOff(1, 68) });
        ev.push_back({ 6.35, juce::MidiMessage::noteOff(1, 71) });

        // Re-strike grip & engage LKR (CC 69: E -> D#) for V7/I resolution into open E Major!
        ev.push_back({ 6.50, juce::MidiMessage::noteOn(1, 59, 0.84f) });
        ev.push_back({ 6.52, juce::MidiMessage::noteOn(1, 64, 0.86f) });
        ev.push_back({ 6.54, juce::MidiMessage::noteOn(1, 68, 0.88f) });
        ev.push_back({ 6.56, juce::MidiMessage::noteOn(1, 71, 0.90f) });
        addCcRamp(ev, 1, 69, 6.60, 6.80, 0.0f, 1.0f); // Knee LKR (E -> D#)
        addCcRamp(ev, 1, 69, 7.90, 8.10, 1.0f, 0.0f); // Resolve back to pure E Major!

        ev.push_back({ 9.70, juce::MidiMessage::noteOff(1, 59) });
        ev.push_back({ 9.70, juce::MidiMessage::noteOff(1, 64) });
        ev.push_back({ 9.70, juce::MidiMessage::noteOff(1, 68) });
        ev.push_back({ 9.70, juce::MidiMessage::noteOff(1, 71) });

        renderSequenceToWav(10, 11.0, 86.0, ev, presetsDir.getChildFile("11_manual_e9th_workstation.wav"));
    }

    // =========================================================================
    // Preset 12: Studio Phrase Lick Machine (Chromatic, 86 BPM)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        // Play Lick 1 in E Major (MIDI 68 = root), then transpose Lick 1 to A Major (+5 semis = MIDI 73),
        // then transpose to F#m / B (+2 semis = MIDI 70), and resolve via Keyswitch D1 (Weeping Slide) home to E Major!
        ev.push_back({ 0.05, juce::MidiMessage::noteOn(1, 68, 0.94f) }); // Lick 1 in E Major (I)
        ev.push_back({ 1.92, juce::MidiMessage::noteOff(1, 68) });

        ev.push_back({ 2.25, juce::MidiMessage::noteOn(1, 73, 0.95f) }); // Lick 1 transposed +5 semis to A Major (IV)!
        ev.push_back({ 3.72, juce::MidiMessage::noteOff(1, 73) });

        ev.push_back({ 4.05, juce::MidiMessage::noteOn(1, 75, 0.95f) }); // Lick 1 transposed +7 semis to B Major (V)!
        ev.push_back({ 5.35, juce::MidiMessage::noteOff(1, 75) });

        ev.push_back({ 5.70, juce::MidiMessage::noteOn(1, 38, 0.96f) }); // Keyswitch D1: Lick 2 Weeping Slide home to E Major!
        ev.push_back({ 8.55, juce::MidiMessage::noteOff(1, 38) });

        ev.push_back({ 8.65, juce::MidiMessage::noteOn(1, 40, 0.96f) }); // Keyswitch E1: Lick 3 Harmonic Chime tag!
        ev.push_back({ 11.20, juce::MidiMessage::noteOff(1, 40) });

        renderSequenceToWav(11, 11.5, 86.0, ev, presetsDir.getChildFile("12_studio_phrase_lick_machine.wav"));
    }

    // =========================================================================
    // Combined Channel-1 Live Keyboard Showcase (05_live_keyboard_showcase.wav)
    // =========================================================================
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

        {
            std::vector<ScheduledMidiEvent> ev = {
                { 0.05, juce::MidiMessage::noteOn(1, 64, 0.84f) },
                { 0.06, juce::MidiMessage::noteOn(1, 68, 0.86f) },
                { 0.07, juce::MidiMessage::noteOn(1, 71, 0.88f) },
                { 0.65, juce::MidiMessage::noteOn(1, 28, 1.00f) },
                { 1.65, juce::MidiMessage::noteOff(1, 28) },
                { 2.35, juce::MidiMessage::noteOn(1, 24, 1.00f) },
                { 3.60, juce::MidiMessage::noteOff(1, 24) },
                { 3.85, juce::MidiMessage::noteOff(1, 64) },
                { 3.85, juce::MidiMessage::noteOff(1, 68) },
                { 3.85, juce::MidiMessage::noteOff(1, 71) }
            };
            renderSegmentInto(4, 0.0, 5.0, 92.0, ev);
        }
        {
            std::vector<ScheduledMidiEvent> ev = {
                { 0.10, juce::MidiMessage::noteOn(1, 67, 0.88f) },
                { 0.58, juce::MidiMessage::noteOff(1, 67) },
                { 0.59, juce::MidiMessage::noteOn(1, 71, 0.90f) },
                { 1.35, juce::MidiMessage::noteOff(1, 71) },
                { 1.36, juce::MidiMessage::noteOn(1, 74, 0.92f) },
                { 2.20, juce::MidiMessage::noteOff(1, 74) },
                { 2.40, juce::MidiMessage::noteOn(1, 62, 0.80f) },
                { 2.42, juce::MidiMessage::noteOn(1, 69, 0.85f) },
                { 2.44, juce::MidiMessage::noteOn(1, 71, 0.88f) },
                { 3.60, juce::MidiMessage::noteOff(1, 62) },
                { 3.60, juce::MidiMessage::noteOff(1, 69) },
                { 3.60, juce::MidiMessage::noteOff(1, 71) }
            };
            renderSegmentInto(6, 4.6, 10.2, 96.0, ev);
        }
        {
            std::vector<ScheduledMidiEvent> ev = {
                { 0.10, juce::MidiMessage::noteOn(1, 52, 0.82f) },
                { 0.11, juce::MidiMessage::noteOn(1, 59, 0.84f) },
                { 0.12, juce::MidiMessage::noteOn(1, 62, 0.85f) },
                { 0.13, juce::MidiMessage::noteOn(1, 67, 0.88f) },
                { 2.30, juce::MidiMessage::noteOff(1, 52) },
                { 2.30, juce::MidiMessage::noteOff(1, 59) },
                { 2.30, juce::MidiMessage::noteOff(1, 62) },
                { 2.30, juce::MidiMessage::noteOff(1, 67) },
                { 2.50, juce::MidiMessage::noteOn(1, 60, 0.84f) },
                { 2.51, juce::MidiMessage::noteOn(1, 64, 0.85f) },
                { 2.52, juce::MidiMessage::noteOn(1, 67, 0.86f) },
                { 2.53, juce::MidiMessage::noteOn(1, 71, 0.88f) },
                { 4.70, juce::MidiMessage::noteOff(1, 60) },
                { 4.70, juce::MidiMessage::noteOff(1, 64) },
                { 4.70, juce::MidiMessage::noteOff(1, 67) },
                { 4.70, juce::MidiMessage::noteOff(1, 71) }
            };
            renderSegmentInto(3, 9.8, 15.5, 120.0, ev);
        }

        float pk = showcaseBuf.getMagnitude(0, totalSamples);
        if (pk > 1e-5f) showcaseBuf.applyGain(juce::Decibels::decibelsToGain(-2.5f) / pk);
        writeWavFile(outDir.getChildFile("05_live_keyboard_showcase.wav"), showcaseBuf, kSr);
    }

    return 0;
}
