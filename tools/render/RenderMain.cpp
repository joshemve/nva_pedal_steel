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
    float rawRms = 0.5f * (outBuf.getRMSLevel(0, 0, totalSamples) + outBuf.getRMSLevel(1, 0, totalSamples));
    if (peak > 1e-5f) {
        const float targetDb = juce::jlimit(-10.0f, -1.5f, -2.5f + p.outputLevel);
        float targetPeak = juce::Decibels::decibelsToGain(targetDb);
        outBuf.applyGain(targetPeak / peak);
    }

    if (writeWavFile(outFile, outBuf, kSr)) {
        std::cout << "Rendered preset " << (presetIndex + 1)
                  << " [rawPeak=" << juce::Decibels::gainToDecibels(peak)
                  << " dB, rawRMS=" << juce::Decibels::gainToDecibels(rawRms) << " dB] -> "
                  << outFile.getFullPathName() << std::endl;
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
        // Raw phrase reference render
        std::vector<ScheduledMidiEvent> evRef = {
            { 0.00, juce::MidiMessage::noteOn(1, 43, 0.96f) },
            { 10.60, juce::MidiMessage::noteOff(1, 43) }
        };
        renderSequenceToWav(1, 11.2, 90.0, evRef, outDir.getChildFile("02_hedonist_bootgaze_GMaj.wav"));

        // Preset 02 Audition Showcase: Pure live Indie Americana / Bootgaze performance in G Major (90 BPM):
        // Raked G Major E9th chord -> oblique A+B pedal swell to Cmaj9 -> soaring crying high-bar lead (D5 -> G5 -> F#5 -> D5 -> B4) -> glowing Em9 -> G6/9 resolution!
        std::vector<ScheduledMidiEvent> evShow;
        // 1. Shimmering raked G Major open shell (G3, D4, G4, B4) -> A+B pedal pull into Cadd9/G!
        evShow.push_back({ 0.08, juce::MidiMessage::noteOn(1, 55, 0.82f) }); // G3
        evShow.push_back({ 0.11, juce::MidiMessage::noteOn(1, 62, 0.85f) }); // D4
        evShow.push_back({ 0.14, juce::MidiMessage::noteOn(1, 67, 0.88f) }); // G4
        evShow.push_back({ 0.17, juce::MidiMessage::noteOn(1, 71, 0.91f) }); // B4
        evShow.push_back({ 0.85, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // Oblique A+B lift -> C/G!
        evShow.push_back({ 2.15, juce::MidiMessage::noteOff(1, 28) });       // Melt back to G Major
        evShow.push_back({ 3.35, juce::MidiMessage::noteOff(1, 55) });
        evShow.push_back({ 3.35, juce::MidiMessage::noteOff(1, 62) });
        evShow.push_back({ 3.35, juce::MidiMessage::noteOff(1, 67) });
        evShow.push_back({ 3.35, juce::MidiMessage::noteOff(1, 71) });

        // 2. Soaring high-bar Bootgaze crying melody (A4 -> B4 -> D5 -> G5 -> weeping bar slide -> F#5 -> D5)
        evShow.push_back({ 3.55, juce::MidiMessage::noteOn(1, 69, 0.88f) }); // A4
        evShow.push_back({ 4.15, juce::MidiMessage::noteOff(1, 69) });
        evShow.push_back({ 4.16, juce::MidiMessage::noteOn(1, 71, 0.91f) }); // Legato bar glide A4 -> B4
        evShow.push_back({ 4.88, juce::MidiMessage::noteOff(1, 71) });
        evShow.push_back({ 4.89, juce::MidiMessage::noteOn(1, 74, 0.93f) }); // Legato bar glide B4 -> D5
        evShow.push_back({ 5.65, juce::MidiMessage::noteOff(1, 74) });
        evShow.push_back({ 5.66, juce::MidiMessage::noteOn(1, 79, 0.95f) }); // Soar up to high G5!
        addPitchWheelRamp(evShow, 1, 6.25, 6.75, 0.0f, -1.0f / 7.0f);        // Crying bar slide G5 -> F#5
        addPitchWheelRamp(evShow, 1, 7.15, 7.60, -1.0f / 7.0f, -5.0f / 7.0f);// Weeping glide down to D5!
        evShow.push_back({ 7.85, juce::MidiMessage::noteOff(1, 79) });
        evShow.push_back({ 7.88, juce::MidiMessage::pitchWheel(1, 8192) });

        // 3. Final glowing Gmaj7 / G6 open Americana chord with gentle B-pedal suspension resolving home
        evShow.push_back({ 7.95, juce::MidiMessage::noteOn(1, 26, 1.00f) }); // Pre-hold B-pedal (Gsus4)
        evShow.push_back({ 7.98, juce::MidiMessage::noteOn(1, 55, 0.80f) }); // G3
        evShow.push_back({ 8.01, juce::MidiMessage::noteOn(1, 62, 0.84f) }); // D4
        evShow.push_back({ 8.04, juce::MidiMessage::noteOn(1, 71, 0.88f) }); // B4 (-> C5 sus4 -> B4!)
        evShow.push_back({ 8.07, juce::MidiMessage::noteOn(1, 78, 0.90f) }); // F#5 maj7 shimmer!
        evShow.push_back({ 8.70, juce::MidiMessage::noteOff(1, 26) });       // Resolve Gsus4 -> Gmaj7!
        evShow.push_back({ 10.60, juce::MidiMessage::noteOff(1, 55) });
        evShow.push_back({ 10.60, juce::MidiMessage::noteOff(1, 62) });
        evShow.push_back({ 10.60, juce::MidiMessage::noteOff(1, 71) });
        evShow.push_back({ 10.60, juce::MidiMessage::noteOff(1, 78) });

        renderSequenceToWav(1, 12.0, 90.0, evShow, presetsDir.getChildFile("02_hedonist_bootgaze.wav"));
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
    // Preset 07: Cosmic Bootgaze Dotted-8th (104 BPM, G Major)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        const double beat = 60.0 / 104.0; // 0.5769s (1/8D = 432.7ms Echoplex waterfall!)

        // Phrase 1: Shimmering Bootgaze E9th bell-harp cascade (G3 anchor + staggered D4 -> B4 -> F#5 -> G5 bells)
        // Spaced on quarter beats so every high bell note spawns 3 interlocking 1/8D stereo Echoplex repeats!
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 55, 0.82f) });                  // G3 warm bass string
        ev.push_back({ 0.10 + 0.5 * beat, juce::MidiMessage::noteOn(1, 62, 0.85f) });     // D4
        ev.push_back({ 0.10 + 1.0 * beat, juce::MidiMessage::noteOn(1, 71, 0.89f) });     // B4 bell
        ev.push_back({ 0.10 + 2.0 * beat, juce::MidiMessage::noteOn(1, 78, 0.92f) });     // F#5 maj7 chime!
        ev.push_back({ 0.10 + 2.75 * beat, juce::MidiMessage::noteOff(1, 78) });
        ev.push_back({ 0.10 + 3.0 * beat, juce::MidiMessage::noteOn(1, 79, 0.94f) });     // G5 high shimmer!
        ev.push_back({ 0.10 + 3.65 * beat, juce::MidiMessage::noteOn(1, 28, 1.00f) });    // Oblique A+B -> Cadd9/G bloom!
        ev.push_back({ 0.10 + 4.85 * beat, juce::MidiMessage::noteOff(1, 28) });
        for (int n : { 55, 62, 71, 79 }) ev.push_back({ 0.10 + 5.20 * beat, juce::MidiMessage::noteOff(1, n) });

        // Phrase 2: Dreamy Em9 asynchronous string plucks (E4 -> B4 -> F#5) weeping down 2 frets (F#5 -> E5) into 1/8D space!
        const double t2 = 0.10 + 6.0 * beat; // ~3.56s
        ev.push_back({ t2, juce::MidiMessage::noteOn(1, 64, 0.83f) });                    // E4
        ev.push_back({ t2 + 0.5 * beat, juce::MidiMessage::noteOn(1, 71, 0.87f) });       // B4
        ev.push_back({ t2 + 1.0 * beat, juce::MidiMessage::noteOn(1, 78, 0.93f) });       // F#5 singing high string!
        addPitchWheelRamp(ev, 1, t2 + 1.8 * beat, t2 + 2.6 * beat, 0.0f, -2.0f / 7.0f);   // Liquid 2-fret bar cry (F#5->E5, B4->A4, E4->D4 = Dadd9/G6!)
        for (int n : { 64, 71, 78 }) ev.push_back({ t2 + 3.6 * beat, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ t2 + 3.65 * beat, juce::MidiMessage::pitchWheel(1, 8192) });       // Reset 0.85s BEFORE Phrase 3 (100% settled!)

        // Phrase 3: Radiant 100%-in-key Gsus4 -> Gmaj7 harp roll + final crystalline high D5->G5 cosmic echo tail!
        const double t3 = t2 + 5.0 * beat; // ~6.45s
        ev.push_back({ t3 - 0.04, juce::MidiMessage::noteOn(1, 26, 1.00f) });             // Pre-hold Pedal B (C5 sus4)
        ev.push_back({ t3 + 0.00, juce::MidiMessage::noteOn(1, 55, 0.83f) });             // G3
        ev.push_back({ t3 + 0.04, juce::MidiMessage::noteOn(1, 62, 0.86f) });             // D4
        ev.push_back({ t3 + 0.08, juce::MidiMessage::noteOn(1, 71, 0.90f) });             // B4 (starts at C5!)
        ev.push_back({ t3 + 0.12, juce::MidiMessage::noteOn(1, 78, 0.92f) });             // F#5 maj7 (stationary!)
        ev.push_back({ t3 + 1.15 * beat, juce::MidiMessage::noteOff(1, 26) });            // Melt C5 -> B4 into pure Gmaj7!
        ev.push_back({ t3 + 2.00 * beat, juce::MidiMessage::noteOn(1, 74, 0.91f) });      // Final D5 bell...
        ev.push_back({ t3 + 2.50 * beat, juce::MidiMessage::noteOn(1, 79, 0.93f) });      // ...into high G5 echoing into the 1/8D horizon!
        ev.push_back({ t3 + 3.30 * beat, juce::MidiMessage::noteOff(1, 74) });
        ev.push_back({ t3 + 3.30 * beat, juce::MidiMessage::noteOff(1, 79) });
        for (int n : { 55, 62, 71, 78 }) ev.push_back({ t3 + 4.30 * beat, juce::MidiMessage::noteOff(1, n) });

        renderSequenceToWav(6, 11.2, 104.0, ev, presetsDir.getChildFile("07_cosmic_bootgaze_dotted_8th.wav"));
    }

    // =========================================================================
    // Preset 08: Slow-Gear Dream Steel (Ambient, 82 BPM, D Major)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        // Luminous E9th Goodrich volume-pedal swells with oblique B-pedal blooms AND gentle tonebar movement inside each swell!
        // Swell 1: Dadd9 (A3, D4, F#4, E5) -> oblique third scoop blooms into A+B G6/D suspension!
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 57, 0.84f) });
        ev.push_back({ 0.12, juce::MidiMessage::noteOn(1, 62, 0.86f) });
        ev.push_back({ 0.14, juce::MidiMessage::noteOn(1, 66, 0.89f) });
        ev.push_back({ 0.16, juce::MidiMessage::noteOn(1, 76, 0.91f) });
        ev.push_back({ 1.15, juce::MidiMessage::noteOn(1, 28, 1.00f) });  // Crest of volume swell -> oblique A+B lift!
        ev.push_back({ 2.15, juce::MidiMessage::noteOff(1, 28) });        // Melt back to Dadd9
        for (int n : { 57, 62, 66, 76 }) ev.push_back({ 2.95, juce::MidiMessage::noteOff(1, n) });

        // Swell 2: Em7 (E4, G4, B4, D5) -> liquid 2-fret chrome-bar glide up into F#m7 (100% diatonic D Major!)
        ev.push_back({ 3.20, juce::MidiMessage::noteOn(1, 64, 0.85f) });
        ev.push_back({ 3.22, juce::MidiMessage::noteOn(1, 67, 0.88f) });
        ev.push_back({ 3.24, juce::MidiMessage::noteOn(1, 71, 0.90f) });
        ev.push_back({ 3.26, juce::MidiMessage::noteOn(1, 74, 0.92f) });
        addPitchWheelRamp(ev, 1, 4.35, 5.05, 0.0f, +2.0f / 7.0f);         // Liquid bar glide up 2 frets Em7 -> F#m7!
        for (int n : { 64, 67, 71, 74 }) ev.push_back({ 6.00, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 6.12, juce::MidiMessage::pitchWheel(1, 8192) });

        // Swell 3: Gmaj7 (G3, D4, B4, F#5) -> gentle Gsus4 B-pedal shimmer -> glowing Dadd9 cathedral finale!
        ev.push_back({ 6.20, juce::MidiMessage::noteOn(1, 55, 0.84f) });
        ev.push_back({ 6.22, juce::MidiMessage::noteOn(1, 62, 0.86f) });
        ev.push_back({ 6.24, juce::MidiMessage::noteOn(1, 71, 0.89f) });
        ev.push_back({ 6.26, juce::MidiMessage::noteOn(1, 78, 0.91f) });
        ev.push_back({ 7.25, juce::MidiMessage::noteOn(1, 26, 1.00f) });  // Gentle B-pedal B4->C5 Lydian/Gsus4 shimmer
        ev.push_back({ 8.05, juce::MidiMessage::noteOff(1, 26) });
        for (int n : { 55, 62, 71, 78 }) ev.push_back({ 8.65, juce::MidiMessage::noteOff(1, n) });

        ev.push_back({ 8.85, juce::MidiMessage::noteOn(1, 57, 0.84f) });
        ev.push_back({ 8.87, juce::MidiMessage::noteOn(1, 62, 0.87f) });
        ev.push_back({ 8.89, juce::MidiMessage::noteOn(1, 66, 0.90f) });
        ev.push_back({ 8.91, juce::MidiMessage::noteOn(1, 74, 0.92f) });
        for (int n : { 57, 62, 66, 74 }) ev.push_back({ 10.95, juce::MidiMessage::noteOff(1, n) });

        renderSequenceToWav(7, 12.4, 82.0, ev, presetsDir.getChildFile("08_slow_gear_dream_steel.wav"));
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
    // Preset 10: Hot Tweed Roadhouse Grit (114 BPM, A Mixolydian / Southern Rock)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        // Authentic E9th roadhouse phrasing grounded in the warm middle strings + pine-cab thumb root:
        // asynchronous string plucks ringing into chewy 6V6 Tweed double-stops and weeping downward on the bar!

        // Phrase 1 (0.10s - 2.95s): Warm A3 thumb anchor + E5 -> C#5 asynchronous sixth/third double-stop
        // weeping the chrome bar down 2 frets (C#5+E5 -> B4+D5 Mixolydian cry) with breathing room!
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 57, 0.78f) }); // A3 woody 5E3 pine-cab thumb root
        ev.push_back({ 0.13, juce::MidiMessage::noteOn(1, 76, 0.87f) }); // E5 singing middle-top string
        ev.push_back({ 0.56, juce::MidiMessage::noteOn(1, 73, 0.85f) }); // Add C#5 -> chewy 6V6 intermodulation!
        addPitchWheelRamp(ev, 1, 0.98, 1.52, 0.0f, -2.0f / 7.0f);        // Weeping 2-fret bar cry!
        ev.push_back({ 2.30, juce::MidiMessage::noteOff(1, 57) });
        ev.push_back({ 2.30, juce::MidiMessage::noteOff(1, 76) });
        ev.push_back({ 2.30, juce::MidiMessage::noteOff(1, 73) });
        ev.push_back({ 2.36, juce::MidiMessage::pitchWheel(1, 8192) });  // Clean reset in the breathing gap!

        // Phrase 2 (3.30s - 6.10s): Throaty D4 + F#5 -> D5 roadhouse sixth/tenth weeping down 2 frets & curling bluesily!
        ev.push_back({ 3.30, juce::MidiMessage::noteOn(1, 62, 0.79f) }); // D4 warm body anchor
        ev.push_back({ 3.33, juce::MidiMessage::noteOn(1, 78, 0.86f) }); // F#5 sweet upper voice
        ev.push_back({ 3.74, juce::MidiMessage::noteOn(1, 74, 0.84f) }); // Add D5 -> rich Tweed Deluxe growl!
        addPitchWheelRamp(ev, 1, 4.14, 4.68, 0.0f, -2.0f / 7.0f);        // Greasy whole-step bar cry down to C5+E5!
        addPitchWheelRamp(ev, 1, 4.96, 5.32, -2.0f / 7.0f, -1.0f / 7.0f);// Bluesy curl into C#5!
        ev.push_back({ 5.95, juce::MidiMessage::noteOff(1, 62) });
        ev.push_back({ 5.95, juce::MidiMessage::noteOff(1, 78) });
        ev.push_back({ 5.95, juce::MidiMessage::noteOff(1, 74) });
        ev.push_back({ 6.02, juce::MidiMessage::pitchWheel(1, 8192) });

        // Phrase 3 (6.80s - 10.6s): Throaty raked A Major roadhouse grip (A3, E4, A4, C#5) whipping A+B -> D/A -> A Major!
        ev.push_back({ 6.80, juce::MidiMessage::noteOn(1, 57, 0.82f) }); // A3 woody pine-cab root
        ev.push_back({ 6.83, juce::MidiMessage::noteOn(1, 64, 0.85f) }); // E4 fifth
        ev.push_back({ 6.86, juce::MidiMessage::noteOn(1, 69, 0.87f) }); // A4 octave body
        ev.push_back({ 6.89, juce::MidiMessage::noteOn(1, 73, 0.89f) }); // C#5 major third bark
        ev.push_back({ 7.55, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // Roadhouse A+B slant -> D/A!
        ev.push_back({ 8.35, juce::MidiMessage::noteOff(1, 28) });       // Melt home into singing Tweed A Major!
        for (int n : { 57, 64, 69, 73 }) ev.push_back({ 9.85, juce::MidiMessage::noteOff(1, n) });

        renderSequenceToWav(9, 11.0, 114.0, ev, presetsDir.getChildFile("10_hot_tweed_roadhouse_grit.wav"));
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
    // Preset 13: 4-Track Cassette Steel (88 BPM, E Major — Dijon / Mk.gee Bedroom Soul)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 1: Intimate soft thumb-picked Eadd9 bedroom pocket + quick gospel A/E sigh
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 52, 0.64f) }); // E3 soft warm tape head-bump
        ev.push_back({ 0.13, juce::MidiMessage::noteOn(1, 59, 0.66f) }); // B3
        ev.push_back({ 0.16, juce::MidiMessage::noteOn(1, 68, 0.68f) }); // G#4
        ev.push_back({ 0.62, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // Quick gospel A/E slant
        ev.push_back({ 1.12, juce::MidiMessage::noteOff(1, 28) });
        for (int n : { 52, 59, 68 }) ev.push_back({ 1.62, juce::MidiMessage::noteOff(1, n) });

        // Phrase 2: Syncopated C#m7 pocket + soulful Ch-3 grace-note bar slide (F#4 -> G#4)
        ev.push_back({ 1.78, juce::MidiMessage::noteOn(1, 49, 0.70f) }); // C#3
        ev.push_back({ 1.81, juce::MidiMessage::noteOn(1, 56, 0.72f) }); // G#3
        ev.push_back({ 1.84, juce::MidiMessage::noteOn(1, 64, 0.74f) }); // E4
        ev.push_back({ 1.88, juce::MidiMessage::noteOn(3, 66, 0.80f) }); // F#4 -> G#4 grace slide!
        addPitchWheelRamp(ev, 3, 1.94, 2.18, 0.0f, +2.0f / 12.0f);
        for (int n : { 49, 56, 64 }) ev.push_back({ 3.28, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 3.28, juce::MidiMessage::noteOff(3, 66) });
        ev.push_back({ 3.34, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 3: HARD Tascam 424 Channel-1 Preamp Blast Rake on Amaj7 (vel=0.98 triggers warm oxide sag!)
        ev.push_back({ 3.50, juce::MidiMessage::noteOn(1, 45, 0.96f) }); // A2 deep wound 10th-string bark!
        ev.push_back({ 3.53, juce::MidiMessage::noteOn(1, 52, 0.97f) }); // E3
        ev.push_back({ 3.56, juce::MidiMessage::noteOn(1, 61, 0.98f) }); // C#4
        ev.push_back({ 3.59, juce::MidiMessage::noteOn(3, 71, 0.99f) }); // B4 -> G#4 crying R&B sigh
        addPitchWheelRamp(ev, 3, 4.28, 4.78, 0.0f, -3.0f / 12.0f);       // 71 - 3 = 68 (G#4 = Amaj7!)
        for (int n : { 45, 52, 61 }) ev.push_back({ 5.55, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 5.55, juce::MidiMessage::noteOff(3, 71) });
        ev.push_back({ 5.61, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 4: Tight bedroom F#m7 -> B6 pocket walk into warm cassette E6/9
        ev.push_back({ 5.78, juce::MidiMessage::noteOn(1, 54, 0.72f) }); // F#3
        ev.push_back({ 5.81, juce::MidiMessage::noteOn(1, 61, 0.74f) }); // C#4
        ev.push_back({ 5.84, juce::MidiMessage::noteOn(1, 69, 0.76f) }); // A4
        for (int n : { 54, 61, 69 }) ev.push_back({ 6.45, juce::MidiMessage::noteOff(1, n) });

        ev.push_back({ 6.58, juce::MidiMessage::noteOn(1, 47, 0.76f) }); // B2
        ev.push_back({ 6.61, juce::MidiMessage::noteOn(1, 54, 0.78f) }); // F#3
        ev.push_back({ 6.64, juce::MidiMessage::noteOn(1, 63, 0.80f) }); // D#4
        ev.push_back({ 6.67, juce::MidiMessage::noteOn(1, 68, 0.82f) }); // G#4 (B6)
        for (int n : { 47, 54, 63, 68 }) ev.push_back({ 7.25, juce::MidiMessage::noteOff(1, n) });

        ev.push_back({ 7.40, juce::MidiMessage::noteOn(1, 52, 0.80f) }); // E3
        ev.push_back({ 7.43, juce::MidiMessage::noteOn(1, 59, 0.82f) }); // B3
        ev.push_back({ 7.46, juce::MidiMessage::noteOn(1, 68, 0.84f) }); // G#4
        ev.push_back({ 7.49, juce::MidiMessage::noteOn(3, 66, 0.86f) }); // F#4 -> G#4 double-stop shimmer
        addPitchWheelRamp(ev, 3, 7.60, 7.92, 0.0f, +2.0f / 12.0f);
        ev.push_back({ 7.52, juce::MidiMessage::noteOn(1, 71, 0.85f) }); // B4
        for (int n : { 52, 59, 68, 71 }) ev.push_back({ 9.85, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 9.85, juce::MidiMessage::noteOff(3, 66) });

        renderSequenceToWav(12, 10.6, 88.0, ev, presetsDir.getChildFile("13_4track_cassette_steel.wav"));
    }

    // =========================================================================
    // Preset 14: Golden Hour Velvet (98 BPM, G Major — Kacey Musgraves / Pop-Folk)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });

        // Bar 1: Sparkling studio-condenser fingerpicked Gadd9 harp arpeggio + C/G lift
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 55, 0.80f) }); // G3
        ev.push_back({ 0.36, juce::MidiMessage::noteOn(1, 62, 0.82f) }); // D4
        ev.push_back({ 0.62, juce::MidiMessage::noteOn(1, 71, 0.85f) }); // B4
        ev.push_back({ 0.88, juce::MidiMessage::noteOn(1, 69, 0.84f) }); // A4 (add9 chime!)
        ev.push_back({ 1.14, juce::MidiMessage::noteOn(1, 74, 0.88f) }); // D5
        ev.push_back({ 1.48, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // Crisp C/G pedal bloom
        ev.push_back({ 2.02, juce::MidiMessage::noteOff(1, 28) });
        for (int n : { 55, 62, 69, 71, 74 }) ev.push_back({ 2.42, juce::MidiMessage::noteOff(1, n) });

        // Bar 2: Shimmering Em7 arpeggio -> luminous Dadd4 country-pop turnaround
        ev.push_back({ 2.54, juce::MidiMessage::noteOn(1, 52, 0.80f) }); // E3
        ev.push_back({ 2.78, juce::MidiMessage::noteOn(1, 59, 0.82f) }); // B3
        ev.push_back({ 3.02, juce::MidiMessage::noteOn(1, 67, 0.85f) }); // G4
        ev.push_back({ 3.26, juce::MidiMessage::noteOn(1, 74, 0.87f) }); // D5
        for (int n : { 52, 59, 67, 74 }) ev.push_back({ 3.74, juce::MidiMessage::noteOff(1, n) });

        ev.push_back({ 3.84, juce::MidiMessage::noteOn(1, 50, 0.81f) }); // D3
        ev.push_back({ 3.88, juce::MidiMessage::noteOn(1, 57, 0.83f) }); // A3
        ev.push_back({ 3.92, juce::MidiMessage::noteOn(1, 66, 0.86f) }); // F#4
        ev.push_back({ 4.18, juce::MidiMessage::noteOn(1, 67, 0.85f) }); // G4 (Dadd4 bell rub!)
        ev.push_back({ 4.44, juce::MidiMessage::noteOn(1, 74, 0.88f) }); // D5
        for (int n : { 50, 57, 66, 67, 74 }) ev.push_back({ 4.92, juce::MidiMessage::noteOff(1, n) });

        // Bar 3-4: Cmaj9 vocal-pocket hook -> sparkling Gmaj9 6.2 kHz condenser finale
        ev.push_back({ 5.04, juce::MidiMessage::noteOn(1, 48, 0.80f) }); // C3
        ev.push_back({ 5.08, juce::MidiMessage::noteOn(1, 55, 0.82f) }); // G3
        ev.push_back({ 5.12, juce::MidiMessage::noteOn(1, 64, 0.84f) }); // E4
        ev.push_back({ 5.18, juce::MidiMessage::noteOn(3, 69, 0.89f) }); // A4 -> B4 -> D5 -> B4 hook!
        addPitchWheelRamp(ev, 3, 5.38, 5.68, 0.0f, +2.0f / 12.0f);       // 69 + 2 = 71 (B4, Cmaj7!)
        addPitchWheelRamp(ev, 3, 5.95, 6.28, +2.0f / 12.0f, +5.0f / 12.0f); // 69 + 5 = 74 (D5, Cmaj9!)
        addPitchWheelRamp(ev, 3, 6.55, 6.88, +5.0f / 12.0f, +2.0f / 12.0f); // Back to B4
        for (int n : { 48, 55, 64 }) ev.push_back({ 7.22, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 7.22, juce::MidiMessage::noteOff(3, 69) });
        ev.push_back({ 7.28, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 7.38, juce::MidiMessage::noteOn(1, 55, 0.80f) }); // G3
        ev.push_back({ 7.44, juce::MidiMessage::noteOn(1, 62, 0.82f) }); // D4
        ev.push_back({ 7.50, juce::MidiMessage::noteOn(1, 69, 0.84f) }); // A4
        ev.push_back({ 7.56, juce::MidiMessage::noteOn(1, 71, 0.86f) }); // B4
        ev.push_back({ 7.62, juce::MidiMessage::noteOn(1, 78, 0.87f) }); // F#5 luminous Gmaj9 bell!
        for (int n : { 55, 62, 69, 71, 78 }) ev.push_back({ 9.85, juce::MidiMessage::noteOff(1, n) });

        renderSequenceToWav(13, 11.2, 98.0, ev, presetsDir.getChildFile("14_golden_hour_velvet.wav"));
    }

    // =========================================================================
    // Preset 15: Apollo Deep Space (60 BPM, B Major — Eno & Lanois Cello-Register Ascent)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });

        // Swell 1: Warm cello/French-horn register B Major (B2 -> B1 61.7 Hz sub-cello!) -> E/B cathedral lift
        ev.push_back({ 0.12, juce::MidiMessage::noteOn(1, 47, 0.86f) }); // B2 (-12st sub-cello = B1!)
        ev.push_back({ 0.16, juce::MidiMessage::noteOn(1, 54, 0.88f) }); // F#3
        ev.push_back({ 0.20, juce::MidiMessage::noteOn(1, 63, 0.90f) }); // D#4
        ev.push_back({ 0.24, juce::MidiMessage::noteOn(1, 66, 0.90f) }); // F#4 (capped warm — zero high screech!)
        ev.push_back({ 1.60, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // Weightless E/B suspension
        ev.push_back({ 2.85, juce::MidiMessage::noteOff(1, 28) });
        for (int n : { 47, 54, 63, 66 }) ev.push_back({ 3.95, juce::MidiMessage::noteOff(1, n) });

        // Swell 2: Deep Eadd9 (E3 -> E2 82.4 Hz sub-cello!) with slow orbital Ch-3 melody (D#4 -> F#4)
        ev.push_back({ 4.20, juce::MidiMessage::noteOn(1, 52, 0.86f) }); // E3
        ev.push_back({ 4.24, juce::MidiMessage::noteOn(1, 59, 0.88f) }); // B3
        ev.push_back({ 4.28, juce::MidiMessage::noteOn(1, 68, 0.89f) }); // G#4
        ev.push_back({ 4.32, juce::MidiMessage::noteOn(3, 63, 0.91f) }); // D#4 (Emaj7) -> F#4 (Eadd9)
        addPitchWheelRamp(ev, 3, 5.20, 6.35, 0.0f, +3.0f / 12.0f);       // 63 + 3 = 66 (F#4!)
        for (int n : { 52, 59, 68 }) ev.push_back({ 7.65, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 7.65, juce::MidiMessage::noteOff(3, 63) });
        ev.push_back({ 7.72, juce::MidiMessage::pitchWheel(3, 8192) });

        // Swell 3: Monumental Bmaj9 (B2, F#3, C#4, D#4, A#4) blooming into dark velvet space
        ev.push_back({ 8.05, juce::MidiMessage::noteOn(1, 47, 0.88f) }); // B2
        ev.push_back({ 8.09, juce::MidiMessage::noteOn(1, 54, 0.89f) }); // F#3
        ev.push_back({ 8.13, juce::MidiMessage::noteOn(1, 61, 0.90f) }); // C#4
        ev.push_back({ 8.17, juce::MidiMessage::noteOn(1, 63, 0.91f) }); // D#4
        ev.push_back({ 8.21, juce::MidiMessage::noteOn(1, 70, 0.91f) }); // A#4 warm maj7 crown
        for (int n : { 47, 54, 61, 63, 70 }) ev.push_back({ 11.40, juce::MidiMessage::noteOff(1, n) });

        renderSequenceToWav(14, 13.2, 60.0, ev, presetsDir.getChildFile("15_apollo_deep_space.wav"));
    }

    // =========================================================================
    // Preset 16: Desert Mirage Spring (96 BPM, A Minor — Khruangbin / Hermanos Gutiérrez)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        // Phrase 1: Low A2 desert thumb anchor + dripping spring-tank minor sixths (E4+C5 -> F#4+D5 Dorian mirage -> E4+C5)
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 45, 0.86f) }); // A2 smoky bass note
        ev.push_back({ 0.42, juce::MidiMessage::noteOn(1, 64, 0.90f) }); // E4
        ev.push_back({ 0.45, juce::MidiMessage::noteOn(1, 72, 0.92f) }); // C5 minor sixth drip!
        addPitchWheelRamp(ev, 1, 0.95, 1.40, 0.0f, +2.0f / 7.0f);        // Sultry slide up to F#4 + D5
        addPitchWheelRamp(ev, 1, 1.75, 2.20, +2.0f / 7.0f, 0.0f);        // Weep back to E4 + C5!
        for (int n : { 45, 64, 72 }) ev.push_back({ 3.10, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 3.16, juce::MidiMessage::pitchWheel(1, 8192) });

        // Phrase 2: Dmadd9 desert pulse -> Morricone/Gutiérrez E7 staggered spring-tank arpeggio!
        ev.push_back({ 3.55, juce::MidiMessage::noteOn(1, 50, 0.86f) }); // D3
        ev.push_back({ 3.58, juce::MidiMessage::noteOn(1, 65, 0.88f) }); // F4
        ev.push_back({ 3.61, juce::MidiMessage::noteOn(1, 69, 0.90f) }); // A4
        ev.push_back({ 3.64, juce::MidiMessage::noteOn(1, 76, 0.92f) }); // E5
        for (int n : { 50, 65, 69, 76 }) ev.push_back({ 4.95, juce::MidiMessage::noteOff(1, n) });

        ev.push_back({ 5.10, juce::MidiMessage::noteOn(1, 52, 0.87f) }); // E3
        ev.push_back({ 5.38, juce::MidiMessage::noteOn(1, 68, 0.89f) }); // G#4
        ev.push_back({ 5.66, juce::MidiMessage::noteOn(1, 71, 0.91f) }); // B4
        ev.push_back({ 5.94, juce::MidiMessage::noteOn(1, 74, 0.93f) }); // D5 E7 tension!
        for (int n : { 52, 68, 71, 74 }) ev.push_back({ 6.55, juce::MidiMessage::noteOff(1, n) });

        // Phrase 3: Hypnotic deep bias-tremolo Amadd9 finale (A2, E3, C4, E4, B4)
        ev.push_back({ 6.80, juce::MidiMessage::noteOn(1, 45, 0.86f) });
        ev.push_back({ 6.83, juce::MidiMessage::noteOn(1, 52, 0.88f) });
        ev.push_back({ 6.86, juce::MidiMessage::noteOn(1, 60, 0.90f) });
        ev.push_back({ 6.89, juce::MidiMessage::noteOn(1, 64, 0.91f) });
        ev.push_back({ 6.92, juce::MidiMessage::noteOn(1, 71, 0.92f) });
        for (int n : { 45, 52, 60, 64, 71 }) ev.push_back({ 9.85, juce::MidiMessage::noteOff(1, n) });

        renderSequenceToWav(15, 11.2, 96.0, ev, presetsDir.getChildFile("16_desert_mirage_spring.wav"));
    }

    // =========================================================================
    // Preset 17: Late-Night R&B Sample Bed (74 BPM, Bbm9 / Db Major — Frank Ocean / 40)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        // Chord 1: Ultra-dark 1.35 kHz 4-pole underwater Bbm9 (Bb2, F3, Db4, Ab4, C5)
        ev.push_back({ 0.12, juce::MidiMessage::noteOn(1, 46, 0.82f) }); // Bb2 deep soul root
        ev.push_back({ 0.15, juce::MidiMessage::noteOn(1, 53, 0.84f) }); // F3
        ev.push_back({ 0.18, juce::MidiMessage::noteOn(1, 61, 0.86f) }); // Db4
        ev.push_back({ 0.21, juce::MidiMessage::noteOn(1, 68, 0.88f) }); // Ab4
        ev.push_back({ 0.24, juce::MidiMessage::noteOn(1, 72, 0.90f) }); // C5 min9 velvet!
        for (int n : { 46, 53, 61, 68, 72 }) ev.push_back({ 2.85, juce::MidiMessage::noteOff(1, n) });

        // Chord 2: Pillowy Ebm9 nocturnal shift (Eb3, Bb3, Gb4, Db5, F5)
        ev.push_back({ 3.05, juce::MidiMessage::noteOn(1, 51, 0.82f) }); // Eb3
        ev.push_back({ 3.08, juce::MidiMessage::noteOn(1, 58, 0.84f) }); // Bb3
        ev.push_back({ 3.11, juce::MidiMessage::noteOn(1, 66, 0.86f) }); // Gb4
        ev.push_back({ 3.14, juce::MidiMessage::noteOn(1, 73, 0.88f) }); // Db5
        ev.push_back({ 3.17, juce::MidiMessage::noteOn(1, 77, 0.89f) }); // F5
        for (int n : { 51, 58, 66, 73, 77 }) ev.push_back({ 5.65, juce::MidiMessage::noteOff(1, n) });

        // Chord 3: Gbmaj9 + weeping upper R&B sixth slide (Ab4+F5 -> Gb4+Eb5)
        ev.push_back({ 5.85, juce::MidiMessage::noteOn(1, 54, 0.82f) }); // Gb3
        ev.push_back({ 5.88, juce::MidiMessage::noteOn(1, 68, 0.87f) }); // Ab4
        ev.push_back({ 5.91, juce::MidiMessage::noteOn(1, 77, 0.90f) }); // F5
        addPitchWheelRamp(ev, 1, 6.40, 7.05, 0.0f, -2.0f / 7.0f);        // Liquid bar glide into Gbmaj13!
        for (int n : { 54, 68, 77 }) ev.push_back({ 7.75, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 7.82, juce::MidiMessage::pitchWheel(1, 8192) });

        // Chord 4: Plush Dbmaj9 2-AM R&B loop resolution (Db3, Ab3, F4, C5, Eb5)
        ev.push_back({ 8.00, juce::MidiMessage::noteOn(1, 49, 0.83f) }); // Db3
        ev.push_back({ 8.03, juce::MidiMessage::noteOn(1, 56, 0.85f) }); // Ab3
        ev.push_back({ 8.06, juce::MidiMessage::noteOn(1, 65, 0.87f) }); // F4
        ev.push_back({ 8.09, juce::MidiMessage::noteOn(1, 72, 0.89f) }); // C5
        ev.push_back({ 8.12, juce::MidiMessage::noteOn(1, 75, 0.90f) }); // Eb5
        for (int n : { 49, 56, 65, 72, 75 }) ev.push_back({ 10.45, juce::MidiMessage::noteOff(1, n) });

        renderSequenceToWav(16, 11.8, 74.0, ev, presetsDir.getChildFile("17_latenight_rnb_sample_bed.wav"));
    }

    // =========================================================================
    // Preset 18: Sunset Slack-Key Lounge (76 BPM, C Major — Sleep Walk / Mac DeMarco / Faye Webster)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(4, 8192) });
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(5, 8192) });

        // Phrase 1 (C6/maj9): Warm C3+G3+E4 cushion while singing lead glides A4 -> C5, then D5 -> E5 -> D5!
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 48, 0.74f) }); // C3 warm tube-combo root
        ev.push_back({ 0.14, juce::MidiMessage::noteOn(1, 55, 0.76f) }); // G3
        ev.push_back({ 0.18, juce::MidiMessage::noteOn(1, 64, 0.78f) }); // E4
        ev.push_back({ 0.24, juce::MidiMessage::noteOn(3, 69, 0.89f) }); // A4 (C6) -> C5 (+3 frets!)
        addPitchWheelRamp(ev, 3, 0.42, 0.78, 0.0f, +3.0f / 12.0f);       // 69 + 3 = 72 (C5)
        ev.push_back({ 1.00, juce::MidiMessage::noteOff(3, 69) });
        ev.push_back({ 1.03, juce::MidiMessage::noteOn(5, 74, 0.91f) }); // D5 -> E5 (+2 frets pure singing formant!)
        addPitchWheelRamp(ev, 5, 1.15, 1.56, 0.0f, +2.0f / 12.0f);       // 74 + 2 = 76 (E5 soaring Sleep Walk peak!)
        addPitchWheelRamp(ev, 5, 2.05, 2.48, +2.0f / 12.0f, 0.0f);       // 76 -> 74 (E5 -> D5 Cadd9 sigh!)
        for (int n : { 48, 55, 64 }) ev.push_back({ 2.85, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 2.85, juce::MidiMessage::noteOff(5, 74) });
        ev.push_back({ 2.92, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 2.92, juce::MidiMessage::pitchWheel(5, 8192) });

        // Phrase 2 (Am9): Lush A2+E3+C4+G4 cushion + weeping Ch-3 slide E5 -> D5 -> C5
        ev.push_back({ 3.05, juce::MidiMessage::noteOn(1, 45, 0.74f) }); // A2
        ev.push_back({ 3.09, juce::MidiMessage::noteOn(1, 52, 0.76f) }); // E3
        ev.push_back({ 3.13, juce::MidiMessage::noteOn(1, 60, 0.78f) }); // C4
        ev.push_back({ 3.17, juce::MidiMessage::noteOn(1, 67, 0.78f) }); // G4 (Am7)
        ev.push_back({ 3.24, juce::MidiMessage::noteOn(3, 76, 0.90f) }); // E5 singing lead
        addPitchWheelRamp(ev, 3, 3.62, 4.05, 0.0f, -2.0f / 12.0f);       // 76 - 2 = 74 (D5)
        addPitchWheelRamp(ev, 3, 4.35, 4.88, -2.0f / 12.0f, -4.0f / 12.0f); // 76 - 4 = 72 (C5 minor 3rd sigh!)
        for (int n : { 45, 52, 60, 67 }) ev.push_back({ 5.65, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 5.65, juce::MidiMessage::noteOff(3, 76) });
        ev.push_back({ 5.72, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 3 (Fmaj7 -> Fm6): THE ICONIC SLEEP WALK / MAC DEMARCO MINOR-IV HEARTBREAK MELT!
        ev.push_back({ 5.85, juce::MidiMessage::noteOn(1, 53, 0.76f) }); // F3
        ev.push_back({ 5.89, juce::MidiMessage::noteOn(1, 60, 0.78f) }); // C4
        ev.push_back({ 5.93, juce::MidiMessage::noteOn(4, 69, 0.80f) }); // A4 (Fmaj7 3rd) -> melts to Ab4 (Fm6)!
        ev.push_back({ 5.98, juce::MidiMessage::noteOn(3, 76, 0.91f) }); // E5 (Fmaj7) -> D5 (Fm6 6th!) -> C5
        addPitchWheelRamp(ev, 3, 6.28, 6.68, 0.0f, -2.0f / 12.0f);       // 76 - 2 = 74 (D5, F6/maj13!)
        addPitchWheelRamp(ev, 4, 7.15, 7.55, 0.0f, -1.0f / 12.0f);       // 69 - 1 = 68 (Ab4 — Fm6 minor-IV melt!!)
        addPitchWheelRamp(ev, 3, 7.58, 8.18, -2.0f / 12.0f, -4.0f / 12.0f); // 74 -> 72 (D5 -> C5 weeping over Fm6!)
        for (int n : { 53, 60 }) ev.push_back({ 8.85, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 8.85, juce::MidiMessage::noteOff(4, 69) });
        ev.push_back({ 8.85, juce::MidiMessage::noteOff(3, 76) });
        ev.push_back({ 8.92, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 8.92, juce::MidiMessage::pitchWheel(4, 8192) });

        // Phrase 4 (Cmaj9/6 -> C6): Sun-drenched VB-2 warble & 1959 sunset chamber resolution
        ev.push_back({ 9.05, juce::MidiMessage::noteOn(1, 48, 0.76f) }); // C3
        ev.push_back({ 9.09, juce::MidiMessage::noteOn(1, 55, 0.78f) }); // G3
        ev.push_back({ 9.13, juce::MidiMessage::noteOn(1, 64, 0.80f) }); // E4
        ev.push_back({ 9.17, juce::MidiMessage::noteOn(1, 69, 0.82f) }); // A4 (6th!)
        ev.push_back({ 9.22, juce::MidiMessage::noteOn(3, 74, 0.90f) }); // D5 (9th) -> glides into E5!
        addPitchWheelRamp(ev, 3, 9.50, 10.00, 0.0f, +2.0f / 12.0f);      // 74 + 2 = 76 (E5 golden resolution!)
        for (int n : { 48, 55, 64, 69 }) ev.push_back({ 11.45, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 11.45, juce::MidiMessage::noteOff(3, 74) });

        renderSequenceToWav(17, 12.6, 76.0, ev, presetsDir.getChildFile("18_sunset_slackkey_lounge.wav"));
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
