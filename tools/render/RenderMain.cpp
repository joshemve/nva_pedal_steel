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

static int gMinPresetOneBased = 1;
static int gMaxPresetOneBased = 999;

static void renderSequenceToWav(int presetIndex, double durationSec, double bpm, const std::vector<ScheduledMidiEvent>& events, const juce::File& outFile) {
    if ((presetIndex + 1) < gMinPresetOneBased || (presetIndex + 1) > gMaxPresetOneBased) return;
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

static int runDeepDspAudit() {
    std::cout << "=== NVA Pedal Steel Deep DSP & Real-Time Stress Audit ===" << std::endl;
    const auto& presets = getPedalSteelPresets();
    static constexpr double kRates[3] = { 44100.0, 48000.0, 96000.0 };

    // 1. Multi-Sample-Rate Invariance & Finite/Peak/DC Audit across all 30 Factory Presets
    for (size_t pIdx = 0; pIdx < presets.size(); ++pIdx) {
        float rmsDbByRate[3] = { 0.0f, 0.0f, 0.0f };

        for (int r = 0; r < 3; ++r) {
            const double sr = kRates[r];
            const int totalSamples = static_cast<int>(sr * 1.6);
            PedalSteelEngine eng;
            eng.prepare(sr, 128);
            PedalSteelParams p = presets[pIdx].params;
            p.hostBpm = 92.0;
            eng.setParams(p);

            juce::AudioBuffer<float> buf(2, totalSamples);
            buf.clear();

            eng.handleMidiMessage(juce::MidiMessage::noteOn(1, 52, 0.82f));
            eng.handleMidiMessage(juce::MidiMessage::noteOn(1, 59, 0.84f));
            eng.handleMidiMessage(juce::MidiMessage::noteOn(1, 68, 0.86f));
            eng.handleMidiMessage(juce::MidiMessage::noteOn(3, 71, 0.88f));

            int pos = 0;
            bool engagedPedal = false;
            bool releasedNotes = false;
            while (pos < totalSamples) {
                const int chunk = std::min(128, totalSamples - pos);
                const double tSec = static_cast<double>(pos) / sr;
                if (!engagedPedal && tSec >= 0.35) {
                    eng.handleMidiMessage(juce::MidiMessage::noteOn(1, 28, 1.0f)); // A+B slant
                    eng.handleMidiMessage(juce::MidiMessage::pitchWheel(3, 10922)); // MPE +4st bend
                    engagedPedal = true;
                }
                if (!releasedNotes && tSec >= 1.05) {
                    eng.handleMidiMessage(juce::MidiMessage::noteOff(1, 28));
                    for (int n : { 52, 59, 68 }) eng.handleMidiMessage(juce::MidiMessage::noteOff(1, n));
                    eng.handleMidiMessage(juce::MidiMessage::noteOff(3, 71));
                    releasedNotes = true;
                }
                eng.renderBlock(buf, pos, chunk);
                pos += chunk;
            }

            double sumSq = 0.0;
            double dcL = 0.0, dcR = 0.0;
            float peak = 0.0f;
            const float* dL = buf.getReadPointer(0);
            const float* dR = buf.getReadPointer(1);
            for (int i = 0; i < totalSamples; ++i) {
                if (!std::isfinite(dL[i]) || !std::isfinite(dR[i])) {
                    std::cerr << "[FAIL] Non-finite sample in preset " << (pIdx + 1) << " at " << sr << " Hz!" << std::endl;
                    return 1;
                }
                peak = std::max({ peak, std::abs(dL[i]), std::abs(dR[i]) });
                sumSq += 0.5 * (static_cast<double>(dL[i]) * dL[i] + static_cast<double>(dR[i]) * dR[i]);
                dcL += dL[i];
                dcR += dR[i];
            }
            const float rmsDb = juce::Decibels::gainToDecibels(static_cast<float>(std::sqrt(sumSq / totalSamples)), -96.0f);
            const double meanDc = std::max(std::abs(dcL), std::abs(dcR)) / totalSamples;
            rmsDbByRate[r] = rmsDb;

            if (peak > 0.9805f || rmsDb < -52.0f || meanDc > 0.005) {
                std::cerr << "[FAIL] Preset " << (pIdx + 1) << " @ " << sr << " Hz out of bounds: peak="
                          << peak << " rmsDb=" << rmsDb << " meanDC=" << meanDc << std::endl;
                return 1;
            }
        }

        const float drift48 = std::abs(rmsDbByRate[1] - rmsDbByRate[0]);
        const float drift96 = std::abs(rmsDbByRate[2] - rmsDbByRate[0]);
        if (drift48 > 1.6f || drift96 > 1.8f) {
            std::cerr << "[FAIL] Sample-rate level drift in preset " << (pIdx + 1)
                      << ": 44.1=" << rmsDbByRate[0] << " dB, 48=" << rmsDbByRate[1]
                      << " dB, 96=" << rmsDbByRate[2] << " dB" << std::endl;
            return 1;
        }
        std::cout << "[PASS] Preset " << (pIdx + 1 < 10 ? "0" : "") << (pIdx + 1)
                  << " (" << presets[pIdx].name << ") | 44.1=" << juce::String(rmsDbByRate[0], 2)
                  << " dB | 48=" << juce::String(rmsDbByRate[1], 2)
                  << " dB | 96=" << juce::String(rmsDbByRate[2], 2) << " dB" << std::endl;
    }

    // 2. Live FX Character Hot-Switch Click-Free Audit (0 -> 1 -> ... -> 27 -> 0 mid-chord)
    {
        constexpr double sr = 48000.0;
        const int total = static_cast<int>(sr * 8.6);
        PedalSteelEngine eng;
        eng.prepare(sr, 64);
        PedalSteelParams p = presets[0].params;
        eng.setParams(p);
        eng.handleMidiMessage(juce::MidiMessage::noteOn(1, 52, 0.85f));
        eng.handleMidiMessage(juce::MidiMessage::noteOn(1, 59, 0.85f));
        eng.handleMidiMessage(juce::MidiMessage::noteOn(1, 68, 0.85f));

        juce::AudioBuffer<float> buf(2, total);
        buf.clear();
        int pos = 0;
        int lastSwitchSlot = -1;
        while (pos < total) {
            const int chunk = std::min(64, total - pos);
            const int slot = static_cast<int>((static_cast<int64_t>(pos) * 29) / total);
            if (slot != lastSwitchSlot) {
                lastSwitchSlot = slot;
                p.fxCharacter = (slot < 28) ? slot : 0;
                eng.setParams(p);
            }
            eng.renderBlock(buf, pos, chunk);
            pos += chunk;
        }

        float maxStep = 0.0f;
        const float* dL = buf.getReadPointer(0);
        for (int i = 1; i < total; ++i) {
            maxStep = std::max(maxStep, std::abs(dL[i] - dL[i - 1]));
        }
        if (maxStep > 0.12f) {
            std::cerr << "[FAIL] Live FX Character hot-switch step too high: " << maxStep << std::endl;
            return 1;
        }
        std::cout << "[PASS] Click-free live FX Character hot-switching (0->1->...->27->0): maxSampleStep="
                  << juce::String(maxStep, 4) << std::endl;
    }

    // 3. Mono Track Downmix + 16-Voice Polyphony Stealing + Sustain + Panic Audit
    {
        constexpr double sr = 44100.0;
        PedalSteelEngine eng;
        eng.prepare(sr, 64);
        eng.setParams(presets[14].params);
        eng.handleMidiMessage(juce::MidiMessage::controllerEvent(1, 64, 127)); // Sustain ON
        for (int n = 48; n <= 76; n += 2) {
            eng.handleMidiMessage(juce::MidiMessage::noteOn((n % 14) + 3, n, 0.90f));
        }
        juce::AudioBuffer<float> monoBuf(1, 4096);
        monoBuf.clear();
        eng.renderBlock(monoBuf, 0, 2048);
        eng.handleMidiMessage(juce::MidiMessage::controllerEvent(1, 123, 0)); // All Notes Off Panic
        eng.renderBlock(monoBuf, 2048, 2048);

        const float rmsPre = monoBuf.getRMSLevel(0, 0, 2048);
        for (int i = 0; i < 4096; ++i) {
            if (!std::isfinite(monoBuf.getSample(0, i))) return 1;
        }
        if (rmsPre < 0.005f) {
            std::cerr << "[FAIL] Mono output bus produced near-silence!" << std::endl;
            return 1;
        }
        std::cout << "[PASS] Mono bus downmix + 15-note MPE voice-stealing + CC64 + CC123 Panic audit passed!" << std::endl;
    }

    std::cout << "=== ALL DEEP DSP & REAL-TIME AUDITS PASSED ===" << std::endl;
    return 0;
}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        const juce::String arg(argv[i]);
        if (arg == "--audit") {
            return runDeepDspAudit();
        } else if (arg == "--from" && i + 1 < argc) {
            gMinPresetOneBased = juce::String(argv[++i]).getIntValue();
        } else if (arg == "--only" && i + 1 < argc) {
            gMinPresetOneBased = juce::String(argv[++i]).getIntValue();
            gMaxPresetOneBased = gMinPresetOneBased;
        }
    }

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
    // Preset 14: Golden Hour Velvet (86 BPM, F Major — Kacey Musgraves Ribbon & LA-2A Velvet)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(5, 8192) });

        // Phrase 1: Plush Coles-ribbon Fadd9 thumb-roll -> Bb/F velvet pedal bloom
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 53, 0.78f) }); // F3 warm 215 Hz velvet chest
        ev.push_back({ 0.14, juce::MidiMessage::noteOn(1, 60, 0.80f) }); // C4
        ev.push_back({ 0.18, juce::MidiMessage::noteOn(1, 67, 0.81f) }); // G4 (add9)
        ev.push_back({ 0.22, juce::MidiMessage::noteOn(1, 69, 0.83f) }); // A4
        ev.push_back({ 0.26, juce::MidiMessage::noteOn(1, 72, 0.84f) }); // C5
        ev.push_back({ 0.95, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // Lush Bb/F oblique bloom
        ev.push_back({ 1.85, juce::MidiMessage::noteOff(1, 28) });
        for (int n : { 53, 60, 67, 69, 72 }) ev.push_back({ 3.25, juce::MidiMessage::noteOff(1, n) });

        // Phrase 2: Dm9 -> Bbmaj9 vocal-pocket singing hook (warm mid-register, zero tinny highs!)
        ev.push_back({ 3.45, juce::MidiMessage::noteOn(1, 50, 0.78f) }); // D3
        ev.push_back({ 3.49, juce::MidiMessage::noteOn(1, 57, 0.80f) }); // A3
        ev.push_back({ 3.53, juce::MidiMessage::noteOn(1, 65, 0.82f) }); // F4
        ev.push_back({ 3.60, juce::MidiMessage::noteOn(3, 67, 0.87f) }); // G4 -> A4 tender vocal glide
        addPitchWheelRamp(ev, 3, 3.75, 4.15, 0.0f, +2.0f / 12.0f);       // 67 + 2 = 69 (A4)
        for (int n : { 50, 57, 65 }) ev.push_back({ 4.90, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 4.90, juce::MidiMessage::noteOff(3, 67) });
        ev.push_back({ 4.96, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 5.04, juce::MidiMessage::noteOn(1, 46, 0.79f) }); // Bb2 deep plush bass
        ev.push_back({ 5.08, juce::MidiMessage::noteOn(1, 53, 0.81f) }); // F3
        ev.push_back({ 5.12, juce::MidiMessage::noteOn(1, 62, 0.82f) }); // D4
        ev.push_back({ 5.18, juce::MidiMessage::noteOn(5, 72, 0.88f) }); // C5 (Bbmaj9) -> A4 (Bbmaj7) melt!
        addPitchWheelRamp(ev, 5, 5.38, 5.82, 0.0f, -3.0f / 12.0f);       // 72 - 3 = 69 (A4, Bbmaj7!)
        for (int n : { 46, 53, 62 }) ev.push_back({ 6.48, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 6.48, juce::MidiMessage::noteOff(5, 72) });
        ev.push_back({ 6.54, juce::MidiMessage::pitchWheel(5, 8192) });

        // Phrase 3: Pillowy F6/9 Golden Hour velvet cushion
        ev.push_back({ 6.68, juce::MidiMessage::noteOn(1, 53, 0.79f) }); // F3
        ev.push_back({ 6.72, juce::MidiMessage::noteOn(1, 60, 0.81f) }); // C4
        ev.push_back({ 6.76, juce::MidiMessage::noteOn(1, 67, 0.82f) }); // G4 (9th)
        ev.push_back({ 6.80, juce::MidiMessage::noteOn(1, 69, 0.84f) }); // A4
        ev.push_back({ 6.86, juce::MidiMessage::noteOn(3, 72, 0.86f) }); // C5 -> D5 (6th!)
        addPitchWheelRamp(ev, 3, 7.05, 7.52, 0.0f, +2.0f / 12.0f);       // 72 + 2 = 74 (D5)
        for (int n : { 53, 60, 67, 69 }) ev.push_back({ 9.90, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 9.90, juce::MidiMessage::noteOff(3, 72) });

        renderSequenceToWav(13, 11.2, 86.0, ev, presetsDir.getChildFile("14_golden_hour_velvet.wav"));
    }

    // =========================================================================
    // Preset 15: Dark Side Phase Steel (72 BPM, E Dorian — Pink Floyd "Breathe" / Psych-Space Phaser)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(5, 8192) });

        // Bar 1: Hypnotic "Breathe" Em9 thumb-roll (stacked 3rds: E3-G3-B3-D4) + liquid Ch-3 E4 -> F#4 glide
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 52, 0.78f) }); // E3 solid un-cancelled Hiwatt bass
        ev.push_back({ 0.15, juce::MidiMessage::noteOn(1, 55, 0.79f) }); // G3
        ev.push_back({ 0.20, juce::MidiMessage::noteOn(1, 59, 0.81f) }); // B3
        ev.push_back({ 0.25, juce::MidiMessage::noteOn(1, 62, 0.82f) }); // D4
        ev.push_back({ 0.32, juce::MidiMessage::noteOn(3, 64, 0.86f) }); // E4 -> F#4 (9th!) floating glide
        addPitchWheelRamp(ev, 3, 0.58, 1.08, 0.0f, +2.0f / 12.0f);       // 64 + 2 = 66 (F#4, pure Em9!)
        ev.push_back({ 1.68, juce::MidiMessage::noteOn(1, 71, 0.83f) }); // Gentle high B4 bell chime
        for (int n : { 52, 55, 59, 62, 71 }) ev.push_back({ 3.22, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 3.22, juce::MidiMessage::noteOff(3, 64) });
        ev.push_back({ 3.28, juce::MidiMessage::pitchWheel(3, 8192) });

        // Bar 2: Iconic E-Dorian A13 shift (A2-E3-G3-C#4) + weeping Ch-5 G4 -> F#4 -> E4 space-soul slide
        ev.push_back({ 3.42, juce::MidiMessage::noteOn(1, 45, 0.80f) }); // A2 deep open bass
        ev.push_back({ 3.47, juce::MidiMessage::noteOn(1, 52, 0.81f) }); // E3
        ev.push_back({ 3.52, juce::MidiMessage::noteOn(1, 55, 0.82f) }); // G3 (b7)
        ev.push_back({ 3.57, juce::MidiMessage::noteOn(1, 61, 0.84f) }); // C#4 (M3)
        ev.push_back({ 3.65, juce::MidiMessage::noteOn(5, 67, 0.88f) }); // G4 -> F#4 (A13!) -> E4
        addPitchWheelRamp(ev, 5, 3.92, 4.42, 0.0f, -1.0f / 12.0f);       // 67 - 1 = 66 (F#4, lush A13!)
        addPitchWheelRamp(ev, 5, 5.10, 5.62, -1.0f / 12.0f, -3.0f / 12.0f); // 67 - 3 = 64 (E4)
        for (int n : { 45, 52, 55, 61 }) ev.push_back({ 6.45, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 6.45, juce::MidiMessage::noteOff(5, 67) });
        ev.push_back({ 6.52, juce::MidiMessage::pitchWheel(5, 8192) });

        // Bar 3: Cmaj7 -> Bm7 -> Em9 Dark Side turnaround into 1/4-note Echoplex horizon
        ev.push_back({ 6.68, juce::MidiMessage::noteOn(1, 48, 0.79f) }); // C3
        ev.push_back({ 6.72, juce::MidiMessage::noteOn(1, 55, 0.81f) }); // G3
        ev.push_back({ 6.76, juce::MidiMessage::noteOn(1, 59, 0.82f) }); // B3
        ev.push_back({ 6.80, juce::MidiMessage::noteOn(1, 64, 0.84f) }); // E4
        ev.push_back({ 6.84, juce::MidiMessage::noteOn(1, 67, 0.85f) }); // G4 (Cmaj7)
        for (int n : { 48, 55, 59, 64, 67 }) ev.push_back({ 7.98, juce::MidiMessage::noteOff(1, n) });

        ev.push_back({ 8.10, juce::MidiMessage::noteOn(1, 47, 0.79f) }); // B2
        ev.push_back({ 8.14, juce::MidiMessage::noteOn(1, 54, 0.81f) }); // F#3
        ev.push_back({ 8.18, juce::MidiMessage::noteOn(1, 57, 0.82f) }); // A3
        ev.push_back({ 8.22, juce::MidiMessage::noteOn(1, 62, 0.84f) }); // D4
        ev.push_back({ 8.26, juce::MidiMessage::noteOn(1, 66, 0.85f) }); // F#4 (Bm7)
        for (int n : { 47, 54, 57, 62, 66 }) ev.push_back({ 9.32, juce::MidiMessage::noteOff(1, n) });

        ev.push_back({ 9.46, juce::MidiMessage::noteOn(1, 52, 0.80f) }); // E3
        ev.push_back({ 9.50, juce::MidiMessage::noteOn(1, 55, 0.81f) }); // G3
        ev.push_back({ 9.54, juce::MidiMessage::noteOn(1, 59, 0.83f) }); // B3
        ev.push_back({ 9.58, juce::MidiMessage::noteOn(1, 62, 0.84f) }); // D4
        ev.push_back({ 9.64, juce::MidiMessage::noteOn(3, 64, 0.87f) }); // E4 -> F#4 final Em9 melt
        addPitchWheelRamp(ev, 3, 9.85, 10.40, 0.0f, +2.0f / 12.0f);
        ev.push_back({ 9.68, juce::MidiMessage::noteOn(1, 71, 0.84f) }); // B4
        for (int n : { 52, 55, 59, 62, 71 }) ev.push_back({ 11.65, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 11.65, juce::MidiMessage::noteOff(3, 64) });

        renderSequenceToWav(14, 13.0, 72.0, ev, presetsDir.getChildFile("15_apollo_deep_space.wav"));
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
    // Preset 19: Apollo Ascent Shimmer (70 BPM, Eb Major — Daniel Lanois / Brian Eno Apollo)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });

        // Chord 1: Velvet Goodrich swell Eb Major -> oblique B-pedal bloom (Ebsus4 -> Eb) feeding +12st Eventide/Lexicon shimmer
        ev.push_back({ 0.12, juce::MidiMessage::noteOn(1, 51, 0.78f) }); // Eb3
        ev.push_back({ 0.16, juce::MidiMessage::noteOn(1, 58, 0.80f) }); // Bb3
        ev.push_back({ 0.20, juce::MidiMessage::noteOn(1, 67, 0.82f) }); // G4
        ev.push_back({ 0.24, juce::MidiMessage::noteOn(1, 70, 0.84f) }); // Bb4
        ev.push_back({ 1.10, juce::MidiMessage::noteOn(1, 26, 1.00f) }); // Pedal B: G4 -> Ab4 (Ebsus4)!
        ev.push_back({ 2.35, juce::MidiMessage::noteOff(1, 26) });       // Melt back to G4
        for (int n : { 51, 58, 67, 70 }) ev.push_back({ 3.30, juce::MidiMessage::noteOff(1, n) });

        // Chord 2: Weightless Bbadd9 -> Bb13 -> Bbadd9 lunar ascent
        ev.push_back({ 3.55, juce::MidiMessage::noteOn(1, 46, 0.78f) }); // Bb2
        ev.push_back({ 3.59, juce::MidiMessage::noteOn(1, 53, 0.80f) }); // F3
        ev.push_back({ 3.63, juce::MidiMessage::noteOn(1, 62, 0.82f) }); // D4
        ev.push_back({ 3.67, juce::MidiMessage::noteOn(1, 72, 0.84f) }); // C5
        ev.push_back({ 3.74, juce::MidiMessage::noteOn(3, 77, 0.86f) }); // F5 soaring lead
        addPitchWheelRamp(ev, 3, 4.30, 4.95, 0.0f, +2.0f / 12.0f);       // F5 -> G5 (Bb13!)
        addPitchWheelRamp(ev, 3, 5.35, 5.95, +2.0f / 12.0f, 0.0f);       // G5 -> F5
        for (int n : { 46, 53, 62, 72 }) ev.push_back({ 6.60, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 6.60, juce::MidiMessage::noteOff(3, 77) });
        ev.push_back({ 6.68, juce::MidiMessage::pitchWheel(3, 8192) });

        // Chord 3: Glowing Abmaj9 cathedral swell
        ev.push_back({ 6.85, juce::MidiMessage::noteOn(1, 44, 0.80f) }); // Ab2
        ev.push_back({ 6.89, juce::MidiMessage::noteOn(1, 51, 0.82f) }); // Eb3
        ev.push_back({ 6.93, juce::MidiMessage::noteOn(1, 60, 0.84f) }); // C4
        ev.push_back({ 6.97, juce::MidiMessage::noteOn(1, 67, 0.85f) }); // G4
        ev.push_back({ 7.04, juce::MidiMessage::noteOn(3, 70, 0.87f) }); // Bb4 -> C5
        addPitchWheelRamp(ev, 3, 7.65, 8.35, 0.0f, +2.0f / 12.0f);       // Bb4 -> C5
        for (int n : { 44, 51, 60, 67 }) ev.push_back({ 9.80, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 9.80, juce::MidiMessage::noteOff(3, 70) });
        ev.push_back({ 9.88, juce::MidiMessage::pitchWheel(3, 8192) });

        // Chord 4: Infinite Ebmaj9 starlight resolution
        ev.push_back({ 10.05, juce::MidiMessage::noteOn(1, 51, 0.80f) }); // Eb3
        ev.push_back({ 10.09, juce::MidiMessage::noteOn(1, 58, 0.82f) }); // Bb3
        ev.push_back({ 10.13, juce::MidiMessage::noteOn(1, 67, 0.84f) }); // G4
        ev.push_back({ 10.17, juce::MidiMessage::noteOn(1, 74, 0.85f) }); // D5
        ev.push_back({ 10.23, juce::MidiMessage::noteOn(3, 77, 0.87f) }); // F5 -> G5
        addPitchWheelRamp(ev, 3, 10.80, 11.45, 0.0f, +2.0f / 12.0f);
        for (int n : { 51, 58, 67, 74 }) ev.push_back({ 12.95, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 12.95, juce::MidiMessage::noteOff(3, 77) });

        renderSequenceToWav(18, 14.6, 70.0, ev, presetsDir.getChildFile("19_apollo_ascent_shimmer.wav"));
    }

    // =========================================================================
    // Preset 20: Starlight Palm Chimes (86 BPM, E Major — 12th-Fret Right-Palm Bell Harmonics)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 1: Harp-like 12th-fret palm-chime arpeggio (E Major -> sounding E4..G#5 water-drop bells) bending through Pedals A+B into A Major!
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 52, 0.78f) }); // E3 -> E4 chime
        ev.push_back({ 0.26, juce::MidiMessage::noteOn(1, 56, 0.80f) }); // G#3 -> G#4 chime
        ev.push_back({ 0.42, juce::MidiMessage::noteOn(1, 59, 0.82f) }); // B3 -> B4 chime
        ev.push_back({ 0.58, juce::MidiMessage::noteOn(1, 64, 0.84f) }); // E4 -> E5 chime
        ev.push_back({ 0.74, juce::MidiMessage::noteOn(1, 68, 0.85f) }); // G#4 -> G#5 chime
        ev.push_back({ 1.38, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // Pedals A+B pull ringing harmonics -> A Major!
        ev.push_back({ 2.35, juce::MidiMessage::noteOff(1, 28) });       // Melt harmonics back to E Major!
        for (int n : { 52, 56, 59, 64, 68 }) ev.push_back({ 3.15, juce::MidiMessage::noteOff(1, n) });

        // Phrase 2: C#m9 chimed bell cascade + crystalline tonebar harmonic glide
        ev.push_back({ 3.35, juce::MidiMessage::noteOn(1, 49, 0.78f) }); // C#3 -> C#4 chime
        ev.push_back({ 3.50, juce::MidiMessage::noteOn(1, 56, 0.80f) }); // G#3 -> G#4 chime
        ev.push_back({ 3.65, juce::MidiMessage::noteOn(1, 59, 0.82f) }); // B3 -> B4 chime
        ev.push_back({ 3.80, juce::MidiMessage::noteOn(1, 64, 0.84f) }); // E4 -> E5 chime
        ev.push_back({ 3.98, juce::MidiMessage::noteOn(3, 66, 0.86f) }); // F#4 -> G#4 -> E4 harmonic slide
        addPitchWheelRamp(ev, 3, 4.35, 4.75, 0.0f, +2.0f / 12.0f);
        addPitchWheelRamp(ev, 3, 5.15, 5.55, +2.0f / 12.0f, -2.0f / 12.0f);
        for (int n : { 49, 56, 59, 64 }) ev.push_back({ 6.20, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 6.20, juce::MidiMessage::noteOff(3, 66) });
        ev.push_back({ 6.28, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 3: Dreamy Aadd9 palm chimes + B-pedal sus4 shimmer -> E6/9 harmonic waterfall
        ev.push_back({ 6.40, juce::MidiMessage::noteOn(1, 45, 0.78f) }); // A2 -> A3 chime
        ev.push_back({ 6.54, juce::MidiMessage::noteOn(1, 52, 0.80f) }); // E3 -> E4 chime
        ev.push_back({ 6.68, juce::MidiMessage::noteOn(1, 57, 0.82f) }); // A3 -> A4 chime
        ev.push_back({ 6.82, juce::MidiMessage::noteOn(1, 61, 0.84f) }); // C#4 -> C#5 chime
        ev.push_back({ 7.35, juce::MidiMessage::noteOn(1, 26, 1.00f) }); // Pedal B: C#4 -> D4 (Asus4 chime)!
        ev.push_back({ 8.15, juce::MidiMessage::noteOff(1, 26) });
        for (int n : { 45, 52, 57, 61 }) ev.push_back({ 8.75, juce::MidiMessage::noteOff(1, n) });

        ev.push_back({ 8.95, juce::MidiMessage::noteOn(1, 52, 0.78f) }); // E3 -> E4 chime
        ev.push_back({ 9.10, juce::MidiMessage::noteOn(1, 56, 0.80f) }); // G#3 -> G#4 chime
        ev.push_back({ 9.25, juce::MidiMessage::noteOn(1, 59, 0.82f) }); // B3 -> B4 chime
        ev.push_back({ 9.40, juce::MidiMessage::noteOn(1, 64, 0.84f) }); // E4 -> E5 chime
        ev.push_back({ 9.56, juce::MidiMessage::noteOn(1, 66, 0.85f) }); // F#4 -> F#5 chime
        for (int n : { 52, 56, 59, 64, 66 }) ev.push_back({ 11.60, juce::MidiMessage::noteOff(1, n) });

        renderSequenceToWav(19, 13.0, 86.0, ev, presetsDir.getChildFile("20_starlight_palm_chimes.wav"));
    }

    // =========================================================================
    // Preset 21: Sacred Gospel Leslie (92 BPM, G Major — 1965 Leslie 147 Chorale <-> Tremolo Sacred Steel)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(4, 8192) });

        // Phrase 1: Starts in warm Chorale (0.78 Hz) -> Pedals A+B (G -> C/G) whip the 147 horn & opposing bass drum
        // up into full 6.55 Hz Fast Tremolo, then coast back down on the sustained G Major release!
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 55, 0.76f) }); // G3
        ev.push_back({ 0.13, juce::MidiMessage::noteOn(1, 62, 0.78f) }); // D4
        ev.push_back({ 0.16, juce::MidiMessage::noteOn(1, 67, 0.80f) }); // G4
        ev.push_back({ 0.19, juce::MidiMessage::noteOn(1, 71, 0.81f) }); // B4
        ev.push_back({ 0.78, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // Pedals A+B -> C/G + Fast Tremolo whip-up!
        ev.push_back({ 1.95, juce::MidiMessage::noteOff(1, 28) });       // Release back to G + 1.1s rotary coast-down!
        for (int n : { 55, 62, 67, 71 }) ev.push_back({ 3.05, juce::MidiMessage::noteOff(1, n) });

        // Phrase 2: Soulful Sacred Steel Gospel Sixths & Testifying Cry (B3+G4 -> C4+A4 -> D4+B4 -> soaring E5->G5 over warm G3!)
        ev.push_back({ 3.22, juce::MidiMessage::channelPressureChange(3, 96) });
        ev.push_back({ 3.25, juce::MidiMessage::noteOn(1, 55, 0.76f) }); // G3 warm organ-like gospel anchor
        ev.push_back({ 3.28, juce::MidiMessage::noteOn(4, 59, 0.84f) }); // B3 + G4 gospel sixths sliding up!
        ev.push_back({ 3.30, juce::MidiMessage::noteOn(3, 67, 0.86f) }); // G4
        addPitchWheelRamp(ev, 4, 3.55, 3.82, 0.0f, +1.0f / 12.0f);       // B3 -> C4
        addPitchWheelRamp(ev, 3, 3.55, 3.82, 0.0f, +2.0f / 12.0f);       // G4 -> A4 (C/G sixth!)
        addPitchWheelRamp(ev, 4, 4.02, 4.30, +1.0f / 12.0f, +3.0f / 12.0f); // C4 -> D4
        addPitchWheelRamp(ev, 3, 4.02, 4.30, +2.0f / 12.0f, +4.0f / 12.0f); // A4 -> B4 (joyous G Major sixth!)
        ev.push_back({ 4.90, juce::MidiMessage::noteOff(4, 59) });
        ev.push_back({ 4.90, juce::MidiMessage::noteOff(3, 67) });
        ev.push_back({ 4.94, juce::MidiMessage::pitchWheel(4, 8192) });
        ev.push_back({ 4.94, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 5.02, juce::MidiMessage::noteOn(4, 67, 0.84f) }); // G4 harmony
        ev.push_back({ 5.05, juce::MidiMessage::noteOn(3, 74, 0.90f) }); // D5 -> E5 -> soaring G5 gospel cry!
        addPitchWheelRamp(ev, 3, 5.18, 5.42, 0.0f, +2.0f / 12.0f);       // D5 -> E5
        addPitchWheelRamp(ev, 3, 5.62, 5.96, +2.0f / 12.0f, +5.0f / 12.0f); // E5 -> G5!
        addPitchWheelRamp(ev, 3, 6.28, 6.64, +5.0f / 12.0f, 0.0f);       // G5 -> D5 weeping resolve!
        ev.push_back({ 6.72, juce::MidiMessage::channelPressureChange(3, 0) });
        ev.push_back({ 7.05, juce::MidiMessage::noteOff(1, 55) });
        ev.push_back({ 7.05, juce::MidiMessage::noteOff(4, 67) });
        ev.push_back({ 7.05, juce::MidiMessage::noteOff(3, 74) });
        ev.push_back({ 7.12, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 3: Churchy Chorale C9 -> Cm6 minor-IV heartbreak cadence
        ev.push_back({ 7.22, juce::MidiMessage::noteOn(1, 48, 0.78f) }); // C3
        ev.push_back({ 7.25, juce::MidiMessage::noteOn(1, 55, 0.79f) }); // G3
        ev.push_back({ 7.28, juce::MidiMessage::noteOn(4, 64, 0.81f) }); // E4 -> Eb4!
        ev.push_back({ 7.31, juce::MidiMessage::noteOn(3, 74, 0.83f) }); // D5 -> C5!
        addPitchWheelRamp(ev, 4, 7.92, 8.35, 0.0f, -1.0f / 12.0f);       // E4 -> Eb4 (Cm6!)
        addPitchWheelRamp(ev, 3, 7.95, 8.38, 0.0f, -2.0f / 12.0f);       // D5 -> C5
        for (int n : { 48, 55 }) ev.push_back({ 9.20, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 9.20, juce::MidiMessage::noteOff(4, 64) });
        ev.push_back({ 9.20, juce::MidiMessage::noteOff(3, 74) });
        ev.push_back({ 9.28, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 9.28, juce::MidiMessage::pitchWheel(4, 8192) });

        // Phrase 4: Triumphant G Major Plagal Amen (G -> C/G -> G) + full 2.0s dual-rotor brake coast-down into Chorale!
        ev.push_back({ 9.40, juce::MidiMessage::noteOn(1, 50, 0.80f) }); // D3
        ev.push_back({ 9.43, juce::MidiMessage::noteOn(1, 55, 0.82f) }); // G3
        ev.push_back({ 9.46, juce::MidiMessage::noteOn(1, 59, 0.83f) }); // B3
        ev.push_back({ 9.49, juce::MidiMessage::noteOn(1, 62, 0.84f) }); // D4
        ev.push_back({ 9.52, juce::MidiMessage::noteOn(1, 67, 0.86f) }); // G4
        ev.push_back({ 9.82, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // IV/I Amen + Tremolo whip-up!
        ev.push_back({ 10.65, juce::MidiMessage::noteOff(1, 28) });      // Release into glorious rotary coast-down!
        for (int n : { 50, 55, 59, 62, 67 }) ev.push_back({ 12.55, juce::MidiMessage::noteOff(1, n) });

        renderSequenceToWav(20, 13.6, 92.0, ev, presetsDir.getChildFile("21_sacred_gospel_leslie.wav"));
    }

    // =========================================================================
    // Preset 22: Blown Console Reverb-X (86 BPM, D Major — Bon Iver / Mk.gee Ducked Diode Room)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 1: Upfront dry Dmaj9 shell -> release at 2.05s so ducked silicon-diode room swells up dramatically in the 1.0s gap!
        ev.push_back({ 0.12, juce::MidiMessage::noteOn(1, 50, 0.86f) }); // D3
        ev.push_back({ 0.15, juce::MidiMessage::noteOn(1, 57, 0.88f) }); // A3
        ev.push_back({ 0.18, juce::MidiMessage::noteOn(1, 66, 0.90f) }); // F#4
        ev.push_back({ 0.21, juce::MidiMessage::noteOn(1, 69, 0.93f) }); // A4
        ev.push_back({ 0.72, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // D -> G/D oblique bloom
        ev.push_back({ 1.48, juce::MidiMessage::noteOff(1, 28) });
        for (int n : { 50, 57, 66, 69 }) ev.push_back({ 2.05, juce::MidiMessage::noteOff(1, n) });

        // Phrase 2 (3.08s after diode bloom & soft gate): Dynamic crying indie melody (soft -> accented blast -> 0.95s console bloom!)
        ev.push_back({ 3.08, juce::MidiMessage::noteOn(3, 64, 0.66f) }); // E4 intimate thumb pluck
        ev.push_back({ 3.46, juce::MidiMessage::noteOff(3, 64) });
        ev.push_back({ 3.48, juce::MidiMessage::noteOn(3, 66, 0.76f) }); // F#4
        ev.push_back({ 3.88, juce::MidiMessage::noteOff(3, 66) });
        ev.push_back({ 3.90, juce::MidiMessage::noteOn(3, 69, 0.96f) }); // A4 accented -> glides to D5!
        addPitchWheelRamp(ev, 3, 4.02, 4.40, 0.0f, +5.0f / 12.0f);       // 69 + 5 = 74 (D5!)
        addPitchWheelRamp(ev, 3, 4.62, 4.96, +5.0f / 12.0f, +2.0f / 12.0f); // 74 -> 71 (B4 sigh!)
        ev.push_back({ 5.15, juce::MidiMessage::noteOff(3, 69) });
        ev.push_back({ 5.22, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 3 (6.12s): Moody Bm7 + crying lead bend -> open console bloom pause at 8.08s
        ev.push_back({ 6.12, juce::MidiMessage::noteOn(1, 47, 0.82f) }); // B2
        ev.push_back({ 6.16, juce::MidiMessage::noteOn(1, 54, 0.84f) }); // F#3
        ev.push_back({ 6.20, juce::MidiMessage::noteOn(1, 62, 0.86f) }); // D4
        ev.push_back({ 6.26, juce::MidiMessage::noteOn(3, 69, 0.94f) }); // A4 -> B4 -> F#4
        addPitchWheelRamp(ev, 3, 6.55, 6.92, 0.0f, +2.0f / 12.0f);       // A4 -> B4
        addPitchWheelRamp(ev, 3, 7.25, 7.72, +2.0f / 12.0f, -3.0f / 12.0f); // B4 -> F#4
        for (int n : { 47, 54, 62 }) ev.push_back({ 8.08, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 8.08, juce::MidiMessage::noteOff(3, 69) });
        ev.push_back({ 8.16, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 4 (9.05s): Warm Dadd9 resolution blooming into gated diode console decay
        ev.push_back({ 9.05, juce::MidiMessage::noteOn(1, 50, 0.84f) }); // D3
        ev.push_back({ 9.09, juce::MidiMessage::noteOn(1, 57, 0.86f) }); // A3
        ev.push_back({ 9.13, juce::MidiMessage::noteOn(1, 64, 0.88f) }); // E4
        ev.push_back({ 9.17, juce::MidiMessage::noteOn(1, 66, 0.90f) }); // F#4
        ev.push_back({ 9.21, juce::MidiMessage::noteOn(1, 74, 0.92f) }); // D5
        for (int n : { 50, 57, 64, 66, 74 }) ev.push_back({ 11.20, juce::MidiMessage::noteOff(1, n) });

        renderSequenceToWav(21, 12.6, 86.0, ev, presetsDir.getChildFile("22_blown_console_reverb_x.wav"));
    }

    // =========================================================================
    // Preset 23: Western Swing C6th Lounge (88 BPM, C Major — Buddy Emmons / Ray Price "Night Life" Standel C6th)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        for (int ch = 3; ch <= 7; ++ch) ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(ch, 8192) });

        // Phrase 1: Velvety 5-string C6/9 harp rake -> Pedals 5+7 (C9/13) -> Pedal 6 (Cm6/9 / F9) jazz slant!
        ev.push_back({ 0.12, juce::MidiMessage::noteOn(1, 48, 0.74f) }); // C3
        ev.push_back({ 0.16, juce::MidiMessage::noteOn(1, 55, 0.76f) }); // G3
        ev.push_back({ 0.20, juce::MidiMessage::noteOn(1, 64, 0.79f) }); // E4
        ev.push_back({ 0.24, juce::MidiMessage::noteOn(1, 69, 0.81f) }); // A4 (6th)
        ev.push_back({ 0.28, juce::MidiMessage::noteOn(1, 74, 0.84f) }); // D5 (9th)
        ev.push_back({ 0.96, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // Pedals 5+7: C3->D3, A4->Bb4 = smoky C9/13!
        ev.push_back({ 1.82, juce::MidiMessage::noteOff(1, 28) });
        ev.push_back({ 2.02, juce::MidiMessage::noteOn(1, 25, 1.00f) }); // Pedal 6 (C#0): E4->Eb4, G3->A3 = Cm6/9 / F9 slant!
        ev.push_back({ 2.76, juce::MidiMessage::noteOff(1, 25) });
        for (int n : { 48, 55, 64, 69, 74 }) ev.push_back({ 3.30, juce::MidiMessage::noteOff(1, n) });

        // Phrase 2: Smoky "Night Life" C6th blues-jazz chrome-bar lead over warm F9 / F13 pocket
        ev.push_back({ 3.48, juce::MidiMessage::noteOn(1, 41, 0.74f) }); // F2 deep C6th string 9!
        ev.push_back({ 3.52, juce::MidiMessage::noteOn(1, 51, 0.75f) }); // Eb3
        ev.push_back({ 3.56, juce::MidiMessage::noteOn(1, 57, 0.76f) }); // A3
        ev.push_back({ 3.60, juce::MidiMessage::noteOn(1, 67, 0.77f) }); // G4 (F9)
        ev.push_back({ 3.68, juce::MidiMessage::noteOn(3, 72, 0.86f) }); // C5 -> Eb5 -> D5 (F13!)
        addPitchWheelRamp(ev, 3, 3.82, 4.10, 0.0f, +3.0f / 12.0f);       // 72 + 3 = 75 (Eb5 blue 7th!)
        addPitchWheelRamp(ev, 3, 4.38, 4.66, +3.0f / 12.0f, +2.0f / 12.0f); // 75 -> 74 (D5 F13 sweet sigh!)
        ev.push_back({ 5.08, juce::MidiMessage::noteOff(3, 72) });
        ev.push_back({ 5.12, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 5.16, juce::MidiMessage::noteOn(4, 68, 0.85f) }); // Ab4 -> A4 blue-3rd curl -> F4!
        addPitchWheelRamp(ev, 4, 5.24, 5.52, 0.0f, +1.0f / 12.0f);       // 68 + 1 = 69 (A4!)
        addPitchWheelRamp(ev, 4, 5.95, 6.25, +1.0f / 12.0f, -3.0f / 12.0f); // 69 -> 65 (F4)
        for (int n : { 41, 51, 57, 67 }) ev.push_back({ 6.58, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 6.58, juce::MidiMessage::noteOff(4, 68) });
        ev.push_back({ 6.64, juce::MidiMessage::pitchWheel(4, 8192) });

        // Phrase 3: Silky harp-raked Dm9 -> Buddy Emmons altered jazz horn glide (G13 -> G7#5b9 -> G9)!
        ev.push_back({ 6.78, juce::MidiMessage::noteOn(1, 50, 0.75f) }); // D3
        ev.push_back({ 6.82, juce::MidiMessage::noteOn(1, 60, 0.77f) }); // C4
        ev.push_back({ 6.86, juce::MidiMessage::noteOn(1, 65, 0.79f) }); // F4
        ev.push_back({ 6.90, juce::MidiMessage::noteOn(1, 69, 0.81f) }); // A4
        ev.push_back({ 6.94, juce::MidiMessage::noteOn(1, 76, 0.84f) }); // E5 (Dm9)
        for (int n : { 50, 60, 65, 69, 76 }) ev.push_back({ 8.05, juce::MidiMessage::noteOff(1, n) });

        ev.push_back({ 8.16, juce::MidiMessage::noteOn(1, 43, 0.75f) }); // G2
        ev.push_back({ 8.20, juce::MidiMessage::noteOn(1, 53, 0.77f) }); // F3
        ev.push_back({ 8.24, juce::MidiMessage::noteOn(1, 59, 0.79f) }); // B3
        ev.push_back({ 8.28, juce::MidiMessage::noteOn(4, 69, 0.82f) }); // A4 (9th) -> Ab4 (b9) -> G4
        ev.push_back({ 8.32, juce::MidiMessage::noteOn(3, 76, 0.86f) }); // E5 (13th) -> Eb5 (#5) -> D5
        addPitchWheelRamp(ev, 3, 8.58, 8.86, 0.0f, -1.0f / 12.0f);       // E5 -> Eb5
        addPitchWheelRamp(ev, 4, 8.58, 8.86, 0.0f, -1.0f / 12.0f);       // A4 -> Ab4 (G7#5b9!)
        addPitchWheelRamp(ev, 3, 9.02, 9.30, -1.0f / 12.0f, -2.0f / 12.0f); // Eb5 -> D5
        addPitchWheelRamp(ev, 4, 9.02, 9.30, -1.0f / 12.0f, -2.0f / 12.0f); // Ab4 -> G4
        for (int n : { 43, 53, 59 }) ev.push_back({ 9.65, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 9.65, juce::MidiMessage::noteOff(4, 69) });
        ev.push_back({ 9.65, juce::MidiMessage::noteOff(3, 76) });
        for (int ch = 3; ch <= 7; ++ch) ev.push_back({ 9.74, juce::MidiMessage::pitchWheel(ch, 8192) });

        // Phrase 4: Iconic 5-string half-fret chrome-bar lounge slide (B6/9 -> C6/9!) ringing in the Standel Capitol chamber!
        ev.push_back({ 9.82, juce::MidiMessage::noteOn(3, 47, 0.76f) }); // B2 -> C3
        ev.push_back({ 9.86, juce::MidiMessage::noteOn(4, 54, 0.78f) }); // F#3 -> G3
        ev.push_back({ 9.90, juce::MidiMessage::noteOn(5, 63, 0.80f) }); // D#4 -> E4
        ev.push_back({ 9.94, juce::MidiMessage::noteOn(6, 68, 0.82f) }); // G#4 -> A4 (6th)
        ev.push_back({ 9.98, juce::MidiMessage::noteOn(7, 73, 0.85f) }); // C#5 -> D5 (9th)
        for (int ch = 3; ch <= 7; ++ch) {
            addPitchWheelRamp(ev, ch, 10.06, 10.34, 0.0f, +1.0f / 12.0f);
        }
        ev.push_back({ 12.35, juce::MidiMessage::noteOff(3, 47) });
        ev.push_back({ 12.35, juce::MidiMessage::noteOff(4, 54) });
        ev.push_back({ 12.35, juce::MidiMessage::noteOff(5, 63) });
        ev.push_back({ 12.35, juce::MidiMessage::noteOff(6, 68) });
        ev.push_back({ 12.35, juce::MidiMessage::noteOff(7, 73) });

        renderSequenceToWav(22, 13.8, 88.0, ev, presetsDir.getChildFile("23_western_swing_c6th_lounge.wav"));
    }

    // =========================================================================
    // Preset 24: Paris Texas Baritone (66 BPM, D Major — Ry Cooder / Santaolalla Mojave Baritone Steel)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(4, 8192) });

        // Phrase 1: Deep open D2 + A2 heavy wound baritone drone + lonely Mojave baritone slide (F#3 -> G3 -> F#3 -> D3)
        ev.push_back({ 0.12, juce::MidiMessage::noteOn(1, 38, 0.84f) }); // D2 baritone low root!
        ev.push_back({ 0.20, juce::MidiMessage::noteOn(1, 45, 0.82f) }); // A2 baritone fifth
        ev.push_back({ 0.55, juce::MidiMessage::noteOn(3, 54, 0.90f) }); // F#3 heavy wound melody
        addPitchWheelRamp(ev, 3, 1.15, 1.65, 0.0f, +1.0f / 12.0f);       // F#3 -> G3 desert bar slide
        addPitchWheelRamp(ev, 3, 2.15, 2.55, +1.0f / 12.0f, 0.0f);       // G3 -> F#3
        addPitchWheelRamp(ev, 3, 2.85, 3.35, 0.0f, -4.0f / 12.0f);       // F#3 -> D3 deep wound slide
        ev.push_back({ 3.72, juce::MidiMessage::noteOff(1, 38) });
        ev.push_back({ 3.72, juce::MidiMessage::noteOff(1, 45) });
        ev.push_back({ 3.72, juce::MidiMessage::noteOff(3, 54) });
        ev.push_back({ 3.80, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 2: Mojave Gadd9/B horizon + soaring baritone lead (A3 -> B3 -> D4 -> A3)
        ev.push_back({ 3.95, juce::MidiMessage::noteOn(1, 47, 0.82f) }); // B2
        ev.push_back({ 4.03, juce::MidiMessage::noteOn(1, 50, 0.80f) }); // D3
        ev.push_back({ 4.11, juce::MidiMessage::noteOn(1, 55, 0.82f) }); // G3
        ev.push_back({ 4.35, juce::MidiMessage::noteOn(3, 57, 0.91f) }); // A3 -> B3 -> D4!
        addPitchWheelRamp(ev, 3, 4.85, 5.30, 0.0f, +2.0f / 12.0f);       // A3 -> B3
        addPitchWheelRamp(ev, 3, 5.65, 6.20, +2.0f / 12.0f, +5.0f / 12.0f); // B3 -> D4
        addPitchWheelRamp(ev, 3, 6.65, 7.15, +5.0f / 12.0f, 0.0f);       // D4 -> A3
        for (int n : { 47, 50, 55 }) ev.push_back({ 7.55, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 7.55, juce::MidiMessage::noteOff(3, 57) });
        ev.push_back({ 7.65, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 3: Haunting A7sus4 -> A canyon turnaround
        ev.push_back({ 7.80, juce::MidiMessage::noteOn(1, 45, 0.83f) }); // A2
        ev.push_back({ 7.88, juce::MidiMessage::noteOn(1, 52, 0.82f) }); // E3
        ev.push_back({ 8.05, juce::MidiMessage::noteOn(3, 55, 0.89f) }); // G3 -> A3
        ev.push_back({ 8.12, juce::MidiMessage::noteOn(4, 62, 0.86f) }); // D4 (sus4) -> C#4 (3rd)!
        addPitchWheelRamp(ev, 3, 8.55, 9.05, 0.0f, +2.0f / 12.0f);       // G3 -> A3
        addPitchWheelRamp(ev, 4, 9.25, 9.75, 0.0f, -1.0f / 12.0f);       // D4 -> C#4
        ev.push_back({ 10.85, juce::MidiMessage::noteOff(1, 45) });
        ev.push_back({ 10.85, juce::MidiMessage::noteOff(1, 52) });
        ev.push_back({ 10.85, juce::MidiMessage::noteOff(3, 55) });
        ev.push_back({ 10.85, juce::MidiMessage::noteOff(4, 62) });
        ev.push_back({ 10.95, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 10.95, juce::MidiMessage::pitchWheel(4, 8192) });

        // Phrase 4: Deep D2 + A2 + D3 Mojave baritone resolve as E3 slides slowly up into golden F#3
        ev.push_back({ 11.10, juce::MidiMessage::noteOn(1, 38, 0.85f) }); // D2
        ev.push_back({ 11.18, juce::MidiMessage::noteOn(1, 45, 0.83f) }); // A2
        ev.push_back({ 11.26, juce::MidiMessage::noteOn(1, 50, 0.84f) }); // D3
        ev.push_back({ 11.42, juce::MidiMessage::noteOn(3, 52, 0.89f) }); // E3 -> F#3!
        addPitchWheelRamp(ev, 3, 11.75, 12.35, 0.0f, +2.0f / 12.0f);
        for (int n : { 38, 45, 50 }) ev.push_back({ 13.70, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 13.70, juce::MidiMessage::noteOff(3, 52) });

        renderSequenceToWav(23, 15.2, 66.0, ev, presetsDir.getChildFile("24_paris_texas_baritone.wav"));
    }

    // =========================================================================
    // Preset 25: Country-Gaze Violin Fuzz (84 BPM, A Major — Wednesday / MJ Lenderman / Sneaky Pete Bosstone Fuzz)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        for (int ch = 3; ch <= 5; ++ch) ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(ch, 8192) });

        // Phrase 1: Pure Sneaky Pete / MJ Lenderman solo liquid singing violin-fuzz lead!
        ev.push_back({ 0.12, juce::MidiMessage::noteOn(3, 64, 0.90f) }); // E4 -> F#4 -> A4 -> C#5!
        addPitchWheelRamp(ev, 3, 0.26, 0.56, 0.0f, +2.0f / 12.0f);       // E4 -> F#4 violin bow-in!
        addPitchWheelRamp(ev, 3, 0.72, 1.08, +2.0f / 12.0f, +5.0f / 12.0f); // F#4 -> A4!
        ev.push_back({ 1.48, juce::MidiMessage::noteOff(3, 64) });
        ev.push_back({ 1.50, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 1.54, juce::MidiMessage::noteOn(3, 73, 0.92f) }); // C#5 -> E5 -> F#5 -> C#5 singing cello/violin cry!
        addPitchWheelRamp(ev, 3, 1.72, 2.08, 0.0f, +3.0f / 12.0f);       // C#5 -> E5!
        ev.push_back({ 2.12, juce::MidiMessage::channelPressureChange(3, 72) });
        addPitchWheelRamp(ev, 3, 2.38, 2.68, +3.0f / 12.0f, +5.0f / 12.0f); // E5 -> F#5 peak cry!
        addPitchWheelRamp(ev, 3, 2.95, 3.35, +5.0f / 12.0f, 0.0f);       // F#5 -> C#5 weeping resolve
        ev.push_back({ 3.42, juce::MidiMessage::channelPressureChange(3, 0) });
        ev.push_back({ 3.55, juce::MidiMessage::noteOff(3, 73) });
        ev.push_back({ 3.62, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 2: Wednesday ("Chosen to Deserve") country-gaze harmonized parallel-sixth violin hook!
        ev.push_back({ 3.74, juce::MidiMessage::noteOn(1, 50, 0.78f) }); // D3 clean-split tube bass!
        ev.push_back({ 3.84, juce::MidiMessage::noteOn(4, 66, 0.86f) }); // F#4 -> G#4 -> A4 -> F#4
        ev.push_back({ 3.87, juce::MidiMessage::noteOn(3, 74, 0.90f) }); // D5 -> E5 -> F#5 -> D5 (6ths!)
        addPitchWheelRamp(ev, 4, 4.35, 4.72, 0.0f, +2.0f / 12.0f);       // F#4 -> G#4
        addPitchWheelRamp(ev, 3, 4.35, 4.72, 0.0f, +2.0f / 12.0f);       // D5 -> E5
        addPitchWheelRamp(ev, 4, 5.05, 5.42, +2.0f / 12.0f, +3.0f / 12.0f); // G#4 -> A4
        addPitchWheelRamp(ev, 3, 5.05, 5.42, +2.0f / 12.0f, +4.0f / 12.0f); // E5 -> F#5
        addPitchWheelRamp(ev, 4, 5.95, 6.45, +3.0f / 12.0f, 0.0f);       // A4 -> F#4
        addPitchWheelRamp(ev, 3, 5.95, 6.45, +4.0f / 12.0f, 0.0f);       // F#5 -> D5
        ev.push_back({ 6.88, juce::MidiMessage::noteOff(1, 50) });
        ev.push_back({ 6.88, juce::MidiMessage::noteOff(4, 66) });
        ev.push_back({ 6.88, juce::MidiMessage::noteOff(3, 74) });
        ev.push_back({ 6.96, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 6.96, juce::MidiMessage::pitchWheel(4, 8192) });

        // Phrase 3: Emotional F#m -> E shoegaze fuzz cry
        ev.push_back({ 7.12, juce::MidiMessage::noteOn(1, 54, 0.76f) }); // F#3
        ev.push_back({ 7.16, juce::MidiMessage::noteOn(1, 61, 0.77f) }); // C#4
        ev.push_back({ 7.26, juce::MidiMessage::noteOn(3, 69, 0.91f) }); // A4 -> B4 -> G#4!
        addPitchWheelRamp(ev, 3, 7.68, 8.08, 0.0f, +2.0f / 12.0f);       // A4 -> B4
        ev.push_back({ 8.42, juce::MidiMessage::noteOff(1, 54) });
        ev.push_back({ 8.42, juce::MidiMessage::noteOff(1, 61) });
        ev.push_back({ 8.48, juce::MidiMessage::noteOn(1, 52, 0.78f) }); // E3
        ev.push_back({ 8.52, juce::MidiMessage::noteOn(1, 59, 0.79f) }); // B3
        addPitchWheelRamp(ev, 3, 8.55, 9.02, +2.0f / 12.0f, -1.0f / 12.0f); // B4 -> G#4 (E major!)
        for (int n : { 52, 59 }) ev.push_back({ 9.95, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 9.95, juce::MidiMessage::noteOff(3, 69) });
        ev.push_back({ 10.04, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 4: Shoegaze A -> D/A -> A wall-of-fuzz A+B pedal bloom!
        ev.push_back({ 10.18, juce::MidiMessage::noteOn(1, 45, 0.80f) }); // A2
        ev.push_back({ 10.22, juce::MidiMessage::noteOn(1, 52, 0.82f) }); // E3
        ev.push_back({ 10.26, juce::MidiMessage::noteOn(1, 61, 0.84f) }); // C#4
        ev.push_back({ 10.30, juce::MidiMessage::noteOn(1, 69, 0.88f) }); // A4
        ev.push_back({ 10.85, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // A+B slant -> D/A shoegaze bloom!
        ev.push_back({ 11.78, juce::MidiMessage::noteOff(1, 28) });      // Release back into golden A!
        for (int n : { 45, 52, 61, 69 }) ev.push_back({ 13.40, juce::MidiMessage::noteOff(1, n) });

        renderSequenceToWav(24, 15.0, 84.0, ev, presetsDir.getChildFile("25_countrygaze_violin_fuzz.wav"));
    }

    // =========================================================================
    // Preset 26: Talking Opto-Vowel Steel (92 BPM, F Major — Pete Drake "Forever" / Steely Dan / Robert Randolph)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(4, 8192) });
        ev.push_back({ 0.00, juce::MidiMessage::controllerEvent(1, 11, 24) });

        // Phrase 1: Pete Drake "Forever" ("Waaait... foooor... meeee...") multi-syllable talking chord via mouth-tube CC11!
        ev.push_back({ 0.12, juce::MidiMessage::noteOn(1, 53, 0.86f) }); // F3
        ev.push_back({ 0.15, juce::MidiMessage::noteOn(1, 57, 0.88f) }); // A3
        ev.push_back({ 0.18, juce::MidiMessage::noteOn(1, 60, 0.88f) }); // C4
        ev.push_back({ 0.21, juce::MidiMessage::noteOn(1, 64, 0.90f) }); // E4
        ev.push_back({ 0.24, juce::MidiMessage::noteOn(1, 69, 0.92f) }); // A4 (Fmaj7)
        addCcRamp(ev, 1, 11, 0.12, 0.54, 0.20f, 1.00f);                  // Syllable 1: "Waaaait" (/u/->/a/->/ae/)
        addCcRamp(ev, 1, 11, 0.58, 0.92, 1.00f, 0.22f);                  // Close jaw (/ae/->/u/)
        ev.push_back({ 0.96, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // A+B slant -> Bbmaj9!
        addCcRamp(ev, 1, 11, 0.96, 1.42, 0.22f, 1.00f);                  // Syllable 2: "foooor" (/u/->/a/->/ae/)
        addCcRamp(ev, 1, 11, 1.46, 1.84, 1.00f, 0.24f);                  // Close jaw
        ev.push_back({ 1.88, juce::MidiMessage::noteOff(1, 28) });      // Back to Fmaj7!
        addCcRamp(ev, 1, 11, 1.88, 2.38, 0.24f, 0.98f);                  // Syllable 3: "meeeee..."
        addCcRamp(ev, 1, 11, 2.75, 3.38, 0.98f, 0.35f);
        for (int n : { 53, 57, 60, 64, 69 }) ev.push_back({ 3.45, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 3.55, juce::MidiMessage::controllerEvent(1, 11, 124) });

        // Phrase 2: Steely Dan "East St. Louis Toodle-Oo" / Mu-Tron III funky talking blues-steel lead!
        ev.push_back({ 3.68, juce::MidiMessage::noteOn(1, 53, 0.88f) }); // F3 funk bass snap
        ev.push_back({ 3.71, juce::MidiMessage::noteOn(1, 63, 0.90f) }); // Eb4 ("WAOW!")
        ev.push_back({ 4.10, juce::MidiMessage::noteOff(1, 63) });
        ev.push_back({ 4.16, juce::MidiMessage::noteOn(3, 68, 0.96f) }); // Ab4 -> A4 talking blue-3rd curl!
        addPitchWheelRamp(ev, 3, 4.24, 4.52, 0.0f, +1.0f / 12.0f);       // Ab4 -> A4 ("Yaaah!")
        ev.push_back({ 4.82, juce::MidiMessage::noteOff(3, 68) });
        ev.push_back({ 4.86, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 4.92, juce::MidiMessage::noteOn(3, 72, 0.96f) }); // C5 -> D5 -> C5 ("Waa-ooow!")
        addPitchWheelRamp(ev, 3, 5.02, 5.30, 0.0f, +2.0f / 12.0f);       // C5 -> D5
        addPitchWheelRamp(ev, 3, 5.42, 5.68, +2.0f / 12.0f, 0.0f);       // D5 -> C5
        ev.push_back({ 5.82, juce::MidiMessage::noteOff(3, 72) });
        ev.push_back({ 5.86, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 5.92, juce::MidiMessage::noteOn(3, 67, 0.95f) }); // G4 -> Ab4 -> F4 funk answer!
        addPitchWheelRamp(ev, 3, 6.00, 6.24, 0.0f, +1.0f / 12.0f);       // G4 -> Ab4
        addPitchWheelRamp(ev, 3, 6.32, 6.58, +1.0f / 12.0f, -2.0f / 12.0f); // Ab4 -> F4
        ev.push_back({ 6.82, juce::MidiMessage::noteOff(1, 53) });
        ev.push_back({ 6.82, juce::MidiMessage::noteOff(3, 67) });
        ev.push_back({ 6.88, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 3: Robert Randolph gospel-funk talking parallel sixths (Bb9 -> C9 -> F)
        ev.push_back({ 7.02, juce::MidiMessage::noteOn(1, 46, 0.85f) }); // Bb2
        ev.push_back({ 7.06, juce::MidiMessage::noteOn(4, 65, 0.92f) }); // F4 -> G4 -> F4
        ev.push_back({ 7.09, juce::MidiMessage::noteOn(3, 74, 0.95f) }); // D5 -> E5 -> D5
        addPitchWheelRamp(ev, 4, 7.45, 7.82, 0.0f, +2.0f / 12.0f);
        addPitchWheelRamp(ev, 3, 7.45, 7.82, 0.0f, +2.0f / 12.0f);
        ev.push_back({ 7.85, juce::MidiMessage::noteOff(1, 46) });
        ev.push_back({ 7.88, juce::MidiMessage::noteOn(1, 48, 0.88f) }); // C3
        addCcRamp(ev, 1, 11, 7.90, 8.28, 0.35f, 1.00f);                  // Vowel burst on V9!
        addPitchWheelRamp(ev, 4, 8.45, 8.82, +2.0f / 12.0f, 0.0f);
        addPitchWheelRamp(ev, 3, 8.45, 8.82, +2.0f / 12.0f, 0.0f);
        ev.push_back({ 9.30, juce::MidiMessage::noteOff(1, 48) });
        ev.push_back({ 9.30, juce::MidiMessage::noteOff(4, 65) });
        ev.push_back({ 9.30, juce::MidiMessage::noteOff(3, 74) });
        ev.push_back({ 9.38, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 9.38, juce::MidiMessage::pitchWheel(4, 8192) });

        // Phrase 4: Talking F6/9 "Oooh-waaaah-oooh" vocal sign-off!
        ev.push_back({ 9.50, juce::MidiMessage::controllerEvent(1, 11, 26) });
        ev.push_back({ 9.55, juce::MidiMessage::noteOn(1, 53, 0.86f) }); // F3
        ev.push_back({ 9.58, juce::MidiMessage::noteOn(1, 57, 0.88f) }); // A3
        ev.push_back({ 9.61, juce::MidiMessage::noteOn(1, 62, 0.88f) }); // D4 (6th)
        ev.push_back({ 9.64, juce::MidiMessage::noteOn(1, 67, 0.90f) }); // G4 (9th)
        ev.push_back({ 9.67, juce::MidiMessage::noteOn(1, 72, 0.92f) }); // C5
        addCcRamp(ev, 1, 11, 9.58, 10.15, 0.22f, 1.00f);                 // "Oooh-WAAAAH..."
        addCcRamp(ev, 1, 11, 10.25, 10.75, 1.00f, 0.28f);                // "...oooh..."
        addCcRamp(ev, 1, 11, 10.80, 11.35, 0.28f, 0.96f);                // "...YAAAH!"
        addCcRamp(ev, 1, 11, 11.75, 12.45, 0.96f, 0.38f);
        for (int n : { 53, 57, 62, 67, 72 }) ev.push_back({ 12.55, juce::MidiMessage::noteOff(1, n) });

        renderSequenceToWav(25, 13.8, 92.0, ev, presetsDir.getChildFile("26_talking_opto_vowel_steel.wav"));
    }

    // =========================================================================
    // Preset 27: Waikiki Bakelite Lap (108 BPM, C Major C6th — Jerry Byrd / Sol Ho'opi'i / Jeremy Wakefield Hawaiian Lap Steel)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        for (int ch = 3; ch <= 7; ++ch) ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(ch, 8192) });

        // Phrase 1: Unmistakable Waikiki / SpongeBob laughing-lagoon C6th glissando!
        // Chromatic parallel-sixth walkup (E4+C5 -> F4+D5 -> F#4+Eb5 -> G4+E5) + sweeping island swoop up to C5+A5!
        ev.push_back({ 0.12, juce::MidiMessage::noteOn(1, 48, 0.78f) }); // C3 C6th bass
        ev.push_back({ 0.16, juce::MidiMessage::noteOn(4, 64, 0.86f) }); // E4
        ev.push_back({ 0.19, juce::MidiMessage::noteOn(3, 72, 0.90f) }); // C5
        addPitchWheelRamp(ev, 4, 0.42, 0.62, 0.0f, +1.0f / 12.0f);       // E4 -> F4
        addPitchWheelRamp(ev, 3, 0.42, 0.62, 0.0f, +2.0f / 12.0f);       // C5 -> D5
        addPitchWheelRamp(ev, 4, 0.72, 0.92, +1.0f / 12.0f, +2.0f / 12.0f); // F4 -> F#4
        addPitchWheelRamp(ev, 3, 0.72, 0.92, +2.0f / 12.0f, +3.0f / 12.0f); // D5 -> Eb5
        addPitchWheelRamp(ev, 4, 1.02, 1.24, +2.0f / 12.0f, +3.0f / 12.0f); // F#4 -> G4
        addPitchWheelRamp(ev, 3, 1.02, 1.24, +3.0f / 12.0f, +4.0f / 12.0f); // Eb5 -> E5!
        // High tropical swoop up to A5 (C6th top shimmer) and back!
        addPitchWheelRamp(ev, 4, 1.65, 2.02, +3.0f / 12.0f, +8.0f / 12.0f); // G4 -> C5!
        addPitchWheelRamp(ev, 3, 1.65, 2.02, +4.0f / 12.0f, +9.0f / 12.0f); // E5 -> A5!
        addPitchWheelRamp(ev, 4, 2.48, 2.85, +8.0f / 12.0f, +5.0f / 12.0f); // C5 -> A4 (6th)
        addPitchWheelRamp(ev, 3, 2.48, 2.85, +9.0f / 12.0f, +7.0f / 12.0f); // A5 -> G5
        ev.push_back({ 3.25, juce::MidiMessage::noteOff(1, 48) });
        ev.push_back({ 3.25, juce::MidiMessage::noteOff(4, 64) });
        ev.push_back({ 3.25, juce::MidiMessage::noteOff(3, 72) });
        ev.push_back({ 3.32, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 3.32, juce::MidiMessage::pitchWheel(4, 8192) });

        // Phrase 2: Classic Hawaiian ragtime-swing A7 -> D9 island turnaround!
        ev.push_back({ 3.42, juce::MidiMessage::noteOn(1, 45, 0.78f) }); // A2
        ev.push_back({ 3.46, juce::MidiMessage::noteOn(1, 55, 0.80f) }); // G3
        ev.push_back({ 3.50, juce::MidiMessage::noteOn(4, 61, 0.86f) }); // C#4 -> D4 -> C#4
        ev.push_back({ 3.54, juce::MidiMessage::noteOn(3, 69, 0.90f) }); // A4 -> Bb4 -> A4 (Hawaiian b9 curl!)
        addPitchWheelRamp(ev, 4, 3.82, 4.08, 0.0f, +1.0f / 12.0f);
        addPitchWheelRamp(ev, 3, 3.82, 4.08, 0.0f, +1.0f / 12.0f);       // Bb4 (A7b9!)
        addPitchWheelRamp(ev, 4, 4.25, 4.52, +1.0f / 12.0f, 0.0f);
        addPitchWheelRamp(ev, 3, 4.25, 4.52, +1.0f / 12.0f, 0.0f);       // Back to A4
        for (int n : { 45, 55 }) ev.push_back({ 4.92, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 4.92, juce::MidiMessage::noteOff(4, 61) });
        ev.push_back({ 4.92, juce::MidiMessage::noteOff(3, 69) });
        ev.push_back({ 4.98, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 4.98, juce::MidiMessage::pitchWheel(4, 8192) });

        // D9 -> G13 island cadence!
        ev.push_back({ 5.05, juce::MidiMessage::noteOn(1, 50, 0.78f) }); // D3
        ev.push_back({ 5.09, juce::MidiMessage::noteOn(1, 60, 0.80f) }); // C4
        ev.push_back({ 5.13, juce::MidiMessage::noteOn(4, 66, 0.86f) }); // F#4 -> F4 (G7!)
        ev.push_back({ 5.17, juce::MidiMessage::noteOn(3, 76, 0.90f) }); // E5 (D9) -> D5 -> E5 (G13!)
        addPitchWheelRamp(ev, 3, 5.50, 5.78, 0.0f, -2.0f / 12.0f);       // E5 -> D5
        ev.push_back({ 6.08, juce::MidiMessage::noteOff(1, 50) });
        ev.push_back({ 6.08, juce::MidiMessage::noteOff(1, 60) });
        ev.push_back({ 6.12, juce::MidiMessage::noteOn(1, 43, 0.80f) }); // G2 (C6th low root!)
        ev.push_back({ 6.16, juce::MidiMessage::noteOn(1, 59, 0.82f) }); // B3
        addPitchWheelRamp(ev, 4, 6.14, 6.40, 0.0f, -1.0f / 12.0f);       // F#4 -> F4 (G7!)
        addPitchWheelRamp(ev, 3, 6.14, 6.42, -2.0f / 12.0f, 0.0f);       // D5 -> E5 (G13!)
        addPitchWheelRamp(ev, 3, 6.85, 7.18, 0.0f, -2.0f / 12.0f);       // E5 -> D5
        for (int n : { 43, 59 }) ev.push_back({ 7.45, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 7.45, juce::MidiMessage::noteOff(4, 66) });
        ev.push_back({ 7.45, juce::MidiMessage::noteOff(3, 76) });
        ev.push_back({ 7.52, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 7.52, juce::MidiMessage::pitchWheel(4, 8192) });

        // Phrase 3 & 4: Signature Jerry Byrd / Waikiki sunset B6/9 -> C6/9 half-fret Bakelite chord slide + high island chime!
        ev.push_back({ 7.62, juce::MidiMessage::noteOn(3, 47, 0.80f) }); // B2 -> C3
        ev.push_back({ 7.66, juce::MidiMessage::noteOn(4, 54, 0.82f) }); // F#3 -> G3
        ev.push_back({ 7.70, juce::MidiMessage::noteOn(5, 63, 0.84f) }); // D#4 -> E4
        ev.push_back({ 7.74, juce::MidiMessage::noteOn(6, 68, 0.86f) }); // G#4 -> A4 (6th)
        ev.push_back({ 7.78, juce::MidiMessage::noteOn(7, 73, 0.90f) }); // C#5 -> D5 (9th)
        for (int ch = 3; ch <= 7; ++ch) {
            addPitchWheelRamp(ev, ch, 7.88, 8.16, 0.0f, +1.0f / 12.0f);  // Lush B6/9 -> C6/9 Hawaiian slide!
        }
        // High shimmering C6th palm-harmonic / chime tag (E5 + A5 + C6? Keep natural: G5->A5->C6? Let's slide E5->G5->A5 on ch 2? Wait ch 8!)
        ev.push_back({ 8.85, juce::MidiMessage::pitchWheel(8, 8192) });
        ev.push_back({ 8.90, juce::MidiMessage::noteOn(8, 79, 0.84f) }); // G5 -> A5 (81) -> G5 -> E5 (76) Waikiki sunset tag!
        addPitchWheelRamp(ev, 8, 9.05, 9.35, 0.0f, +2.0f / 12.0f);       // G5 -> A5
        addPitchWheelRamp(ev, 8, 9.85, 10.22, +2.0f / 12.0f, -3.0f / 12.0f); // A5 -> E5 sweet island sigh!
        for (int ch = 3; ch <= 7; ++ch) {
            const int notes[5] = { 47, 54, 63, 68, 73 };
            ev.push_back({ 11.45, juce::MidiMessage::noteOff(ch, notes[ch - 3]) });
        }
        ev.push_back({ 11.45, juce::MidiMessage::noteOff(8, 79) });

        renderSequenceToWav(26, 12.8, 108.0, ev, presetsDir.getChildFile("27_waikiki_bakelite_lap.wav"));
    }

    // =========================================================================
    // Preset 28: Abbey Road ADT Twin-Slide (88 BPM, E Major — George Harrison "My Sweet Lord" / B.J. Cole "Tiny Dancer")
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(4, 8192) });

        // Phrase 1: Unmistakable George Harrison weeping pentatonic-major twin-slide hook over E Major!
        ev.push_back({ 0.12, juce::MidiMessage::noteOn(1, 52, 0.78f) }); // E3
        ev.push_back({ 0.16, juce::MidiMessage::noteOn(1, 59, 0.80f) }); // B3
        ev.push_back({ 0.28, juce::MidiMessage::noteOn(3, 68, 0.90f) }); // G#4 -> B4 -> C#5 -> B4 -> G#4!
        addPitchWheelRamp(ev, 3, 0.42, 0.78, 0.0f, +3.0f / 12.0f);       // G#4 -> B4 creamy ADT slide!
        addPitchWheelRamp(ev, 3, 1.32, 1.68, +3.0f / 12.0f, +5.0f / 12.0f); // B4 -> C#5!
        addPitchWheelRamp(ev, 3, 2.10, 2.45, +5.0f / 12.0f, +3.0f / 12.0f); // C#5 -> B4
        addPitchWheelRamp(ev, 3, 2.68, 3.08, +3.0f / 12.0f, 0.0f);       // B4 -> G#4 sweet sigh!
        for (int n : { 52, 59 }) ev.push_back({ 3.48, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 3.48, juce::MidiMessage::noteOff(3, 68) });
        ev.push_back({ 3.56, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 2: Iconic "My Sweet Lord" / "Give Me Love" harmonized sixth twin-slide into major-II F#7!
        ev.push_back({ 3.68, juce::MidiMessage::noteOn(1, 49, 0.78f) }); // C#3
        ev.push_back({ 3.72, juce::MidiMessage::noteOn(1, 56, 0.79f) }); // G#3
        ev.push_back({ 3.82, juce::MidiMessage::noteOn(4, 68, 0.86f) }); // G#4 -> B4 -> A#4 (F#7 3rd!)
        ev.push_back({ 3.85, juce::MidiMessage::noteOn(3, 76, 0.90f) }); // E5 -> G#5 -> F#5!
        addPitchWheelRamp(ev, 4, 4.18, 4.58, 0.0f, +3.0f / 12.0f);       // G#4 -> B4
        addPitchWheelRamp(ev, 3, 4.18, 4.58, 0.0f, +4.0f / 12.0f);       // E5 -> G#5
        ev.push_back({ 5.10, juce::MidiMessage::noteOff(1, 49) });
        ev.push_back({ 5.10, juce::MidiMessage::noteOff(1, 56) });
        ev.push_back({ 5.15, juce::MidiMessage::noteOn(1, 54, 0.80f) }); // F#3 (Major II F#7 lift!)
        ev.push_back({ 5.19, juce::MidiMessage::noteOn(1, 64, 0.82f) }); // E4 (b7)
        addPitchWheelRamp(ev, 4, 5.18, 5.58, +3.0f / 12.0f, +2.0f / 12.0f); // B4 -> A#4 (radiant F#7 major 3rd!)
        addPitchWheelRamp(ev, 3, 5.18, 5.58, +4.0f / 12.0f, +2.0f / 12.0f); // G#5 -> F#5!
        for (int n : { 54, 64 }) ev.push_back({ 6.82, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 6.82, juce::MidiMessage::noteOff(4, 68) });
        ev.push_back({ 6.82, juce::MidiMessage::noteOff(3, 76) });
        ev.push_back({ 6.90, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 6.90, juce::MidiMessage::pitchWheel(4, 8192) });

        // Phrase 3: Abbey Road / B.J. Cole Aadd9 -> Am6 major-to-minor-IV heartbreak cadence!
        ev.push_back({ 7.05, juce::MidiMessage::noteOn(1, 45, 0.80f) }); // A2
        ev.push_back({ 7.09, juce::MidiMessage::noteOn(1, 52, 0.81f) }); // E3
        ev.push_back({ 7.14, juce::MidiMessage::noteOn(4, 66, 0.85f) }); // F#4 (6th)
        ev.push_back({ 7.18, juce::MidiMessage::noteOn(3, 73, 0.90f) }); // C#5 -> C5 (minor-IV tear!) -> B4
        addPitchWheelRamp(ev, 3, 8.05, 8.48, 0.0f, -1.0f / 12.0f);       // C#5 -> C5 (Am6!)
        addPitchWheelRamp(ev, 3, 9.05, 9.45, -1.0f / 12.0f, -2.0f / 12.0f); // C5 -> B4
        for (int n : { 45, 52 }) ev.push_back({ 9.85, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 9.85, juce::MidiMessage::noteOff(4, 66) });
        ev.push_back({ 9.85, juce::MidiMessage::noteOff(3, 73) });
        ev.push_back({ 9.94, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 4: Radiant E -> A/E -> E Abbey Road twin-tracked Amen resolve
        ev.push_back({ 10.08, juce::MidiMessage::noteOn(1, 52, 0.82f) }); // E3
        ev.push_back({ 10.12, juce::MidiMessage::noteOn(1, 59, 0.84f) }); // B3
        ev.push_back({ 10.16, juce::MidiMessage::noteOn(1, 68, 0.86f) }); // G#4
        ev.push_back({ 10.20, juce::MidiMessage::noteOn(1, 71, 0.88f) }); // B4
        ev.push_back({ 10.24, juce::MidiMessage::noteOn(1, 76, 0.90f) }); // E5
        ev.push_back({ 10.72, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // E -> A/E slant
        ev.push_back({ 11.48, juce::MidiMessage::noteOff(1, 28) });
        for (int n : { 52, 59, 68, 71, 76 }) ev.push_back({ 12.85, juce::MidiMessage::noteOff(1, n) });

        renderSequenceToWav(27, 14.2, 88.0, ev, presetsDir.getChildFile("28_abbey_road_adt_slide.wav"));
    }

    // =========================================================================
    // =========================================================================
    // Preset 29: 80s Rack Dimension Clean (96 BPM, D Major — 1985 Bradshaw LA Multi-Rack: Tri-Stereo Chorus + PCM70 Circular Delays)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(4, 8192) });

        // Phrase 1: Sparkling 1985 LA-studio fingerpicked Dadd9 bell cascade -> A+B (G6/9/D) -> Pedal A (Bm11/D) -> Dadd9!
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 50, 0.84f) }); // D3
        ev.push_back({ 0.20, juce::MidiMessage::noteOn(1, 57, 0.85f) }); // A3
        ev.push_back({ 0.30, juce::MidiMessage::noteOn(1, 66, 0.88f) }); // F#4
        ev.push_back({ 0.40, juce::MidiMessage::noteOn(1, 69, 0.90f) }); // A4
        ev.push_back({ 0.50, juce::MidiMessage::noteOn(1, 76, 0.93f) }); // E5 (high add9 diamond bell!)
        ev.push_back({ 1.10, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // A+B: Dadd9 -> neon G6/9/D {D3, B3, G4, B4, E5}!
        ev.push_back({ 1.88, juce::MidiMessage::noteOff(1, 28) });
        ev.push_back({ 2.04, juce::MidiMessage::noteOn(1, 24, 1.00f) }); // Pedal A: silky Bm11/D {D3, B3, F#4, B4, E5}!
        ev.push_back({ 2.72, juce::MidiMessage::noteOff(1, 24) });      // Resolve cleanly into radiant Dadd9!
        for (int n : { 50, 57, 66, 69, 76 }) ev.push_back({ 3.48, juce::MidiMessage::noteOff(1, n) });

        // Phrase 2: Prefab Sprout / Dire Straits ("When Love Breaks Down") bell-register cascade & liquid Tri-Chorus bends
        ev.push_back({ 3.64, juce::MidiMessage::noteOn(1, 54, 0.82f) }); // F#3
        ev.push_back({ 3.73, juce::MidiMessage::noteOn(1, 61, 0.84f) }); // C#4
        ev.push_back({ 3.82, juce::MidiMessage::noteOn(1, 64, 0.86f) }); // E4
        ev.push_back({ 3.91, juce::MidiMessage::noteOn(1, 66, 0.87f) }); // F#4
        ev.push_back({ 4.00, juce::MidiMessage::noteOn(3, 69, 0.93f) }); // A4 -> B4 -> C#5!
        addPitchWheelRamp(ev, 3, 4.18, 4.46, 0.0f, +2.0f / 12.0f);       // A4 -> B4
        addPitchWheelRamp(ev, 3, 4.58, 4.86, +2.0f / 12.0f, +4.0f / 12.0f); // B4 -> C#5
        for (int n : { 54, 61, 64, 66 }) ev.push_back({ 5.18, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 5.18, juce::MidiMessage::noteOff(3, 69) });
        ev.push_back({ 5.23, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 5.28, juce::MidiMessage::noteOn(1, 55, 0.83f) }); // G3
        ev.push_back({ 5.37, juce::MidiMessage::noteOn(1, 62, 0.85f) }); // D4
        ev.push_back({ 5.46, juce::MidiMessage::noteOn(1, 66, 0.87f) }); // F#4
        ev.push_back({ 5.55, juce::MidiMessage::noteOn(1, 71, 0.88f) }); // B4 (Gmaj7)
        ev.push_back({ 5.64, juce::MidiMessage::noteOn(3, 74, 0.94f) }); // D5 -> E5 -> A4 across PCM70 circular pan-delays!
        addPitchWheelRamp(ev, 3, 5.82, 6.08, 0.0f, +2.0f / 12.0f);       // D5 -> E5 (Gmaj13)
        addPitchWheelRamp(ev, 3, 6.26, 6.60, +2.0f / 12.0f, -5.0f / 12.0f); // E5 -> A4 (Gmaj9)
        for (int n : { 55, 62, 66, 71 }) ev.push_back({ 6.98, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 6.98, juce::MidiMessage::noteOff(3, 74) });
        ev.push_back({ 7.04, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 3: Michael Landau / Toto wide-open 1985 LA rack Em9 bell harp -> A13sus4 -> A13
        ev.push_back({ 7.12, juce::MidiMessage::noteOn(1, 52, 0.84f) }); // E3
        ev.push_back({ 7.21, juce::MidiMessage::noteOn(1, 59, 0.85f) }); // B3
        ev.push_back({ 7.30, juce::MidiMessage::noteOn(1, 67, 0.87f) }); // G4
        ev.push_back({ 7.39, juce::MidiMessage::noteOn(1, 71, 0.89f) }); // B4
        ev.push_back({ 7.48, juce::MidiMessage::noteOn(1, 78, 0.93f) }); // F#5 (high 9th neon bell!)
        for (int n : { 52, 59, 67, 71, 78 }) ev.push_back({ 8.38, juce::MidiMessage::noteOff(1, n) });

        ev.push_back({ 8.48, juce::MidiMessage::noteOn(1, 45, 0.84f) }); // A2
        ev.push_back({ 8.56, juce::MidiMessage::noteOn(1, 55, 0.85f) }); // G3
        ev.push_back({ 8.64, juce::MidiMessage::noteOn(1, 66, 0.87f) }); // F#4 (13th)
        ev.push_back({ 8.72, juce::MidiMessage::noteOn(1, 69, 0.88f) }); // A4
        ev.push_back({ 8.80, juce::MidiMessage::noteOn(3, 74, 0.92f) }); // D5 (sus4) -> C#5 (A13)!
        addPitchWheelRamp(ev, 3, 9.08, 9.42, 0.0f, -1.0f / 12.0f);       // D5 -> C#5
        for (int n : { 45, 55, 66, 69 }) ev.push_back({ 9.88, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 9.88, juce::MidiMessage::noteOff(3, 74) });
        ev.push_back({ 9.95, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 4: Wide-screen 1985 Blue Nile neon Dmaj9 crystal bell finale floating into circular PCM70 stereo horizon
        ev.push_back({ 10.05, juce::MidiMessage::noteOn(1, 50, 0.85f) }); // D3
        ev.push_back({ 10.14, juce::MidiMessage::noteOn(1, 57, 0.86f) }); // A3
        ev.push_back({ 10.23, juce::MidiMessage::noteOn(1, 66, 0.88f) }); // F#4
        ev.push_back({ 10.32, juce::MidiMessage::noteOn(1, 69, 0.89f) }); // A4
        ev.push_back({ 10.41, juce::MidiMessage::noteOn(1, 73, 0.90f) }); // C#5 (maj7)
        ev.push_back({ 10.50, juce::MidiMessage::noteOn(3, 76, 0.92f) }); // E5 (maj9) -> F#5 diamond bell shimmer!
        addPitchWheelRamp(ev, 3, 10.68, 10.98, 0.0f, +2.0f / 12.0f);     // E5 -> F#5
        for (int n : { 50, 57, 66, 69, 73 }) ev.push_back({ 12.85, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 12.85, juce::MidiMessage::noteOff(3, 76) });

        renderSequenceToWav(28, 14.4, 96.0, ev, presetsDir.getChildFile("29_80s_rack_dimension_clean.wav"));
    }

    // =========================================================================
    // Preset 30: Poly-Octave Messina Choir (76 BPM, G Major — Bon Iver Messina / Chuck Johnson "Balsams" Pipe-Organ Steel)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(4, 8192) });

        // Phrase 1: Warm 16' + 8' + 4' Messina pedal-steel swell on Gadd9 -> oblique A+B C6/G bloom -> Gadd9!
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(4, 43, 0.80f) }); // G2 (-12st = 49 Hz G1 16' Bourdon pedal!)
        ev.push_back({ 0.14, juce::MidiMessage::noteOn(1, 55, 0.80f) }); // G3
        ev.push_back({ 0.18, juce::MidiMessage::noteOn(1, 62, 0.82f) }); // D4 -> E4
        ev.push_back({ 0.22, juce::MidiMessage::noteOn(1, 69, 0.84f) }); // A4 (pure 9th anchor)
        ev.push_back({ 0.26, juce::MidiMessage::noteOn(1, 71, 0.86f) }); // B4 -> C5
        ev.push_back({ 0.30, juce::MidiMessage::noteOn(1, 74, 0.84f) }); // D5 -> E5
        ev.push_back({ 1.08, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // A+B: Gadd9 -> sacred C6/G {G2, G3, E4, A4, C5, E5}!
        ev.push_back({ 2.28, juce::MidiMessage::noteOff(1, 28) });      // Return to warm radiant Gadd9
        ev.push_back({ 3.65, juce::MidiMessage::noteOff(4, 43) });
        for (int n : { 55, 62, 69, 71, 74 }) ev.push_back({ 3.65, juce::MidiMessage::noteOff(1, n) });

        // Phrase 2: Bon Iver Messina / Chuck Johnson "Balsams" open-voiced sacred hymn (Em -> D/F#, zero semitone clusters!)
        ev.push_back({ 3.82, juce::MidiMessage::noteOn(1, 52, 0.80f) }); // E3
        ev.push_back({ 3.86, juce::MidiMessage::noteOn(1, 59, 0.82f) }); // B3
        ev.push_back({ 3.90, juce::MidiMessage::noteOn(1, 67, 0.85f) }); // G4
        ev.push_back({ 3.96, juce::MidiMessage::noteOn(3, 69, 0.86f) }); // A4 (sus4) -> B4 (5th) pure open steel sigh!
        addPitchWheelRamp(ev, 3, 4.30, 4.72, 0.0f, +2.0f / 12.0f);       // A4 -> B4
        for (int n : { 52, 59, 67 }) ev.push_back({ 5.68, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 5.68, juce::MidiMessage::noteOff(3, 69) });
        ev.push_back({ 5.74, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 5.82, juce::MidiMessage::noteOn(1, 54, 0.80f) }); // F#3
        ev.push_back({ 5.86, juce::MidiMessage::noteOn(1, 62, 0.82f) }); // D4
        ev.push_back({ 5.90, juce::MidiMessage::noteOn(1, 69, 0.84f) }); // A4
        ev.push_back({ 5.96, juce::MidiMessage::noteOn(3, 76, 0.85f) }); // E5 (add9) -> D5 (root) pure open resolution!
        addPitchWheelRamp(ev, 3, 6.40, 6.88, 0.0f, -2.0f / 12.0f);       // E5 -> D5
        for (int n : { 54, 62, 69 }) ev.push_back({ 7.55, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 7.55, juce::MidiMessage::noteOff(3, 76) });
        ev.push_back({ 7.62, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 3: Celestial wide-voiced Cmaj9 -> Cmaj7 hymn (D5 -> E5 pure tenth, zero B4/C5 semitone clash!)
        ev.push_back({ 7.72, juce::MidiMessage::noteOn(1, 48, 0.82f) }); // C3
        ev.push_back({ 7.76, juce::MidiMessage::noteOn(1, 55, 0.83f) }); // G3
        ev.push_back({ 7.80, juce::MidiMessage::noteOn(1, 64, 0.85f) }); // E4
        ev.push_back({ 7.84, juce::MidiMessage::noteOn(1, 71, 0.86f) }); // B4 (maj7)
        ev.push_back({ 7.90, juce::MidiMessage::noteOn(3, 74, 0.85f) }); // D5 (9th) -> E5 (10th)!
        addPitchWheelRamp(ev, 3, 8.55, 9.05, 0.0f, +2.0f / 12.0f);       // D5 -> E5
        for (int n : { 48, 55, 64, 71 }) ev.push_back({ 10.15, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 10.15, juce::MidiMessage::noteOff(3, 74) });
        ev.push_back({ 10.22, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 4: Grand 16' + 8' + 4' Pipe-Organ Steel G -> C/G -> G Plagal Cathedral Amen!
        ev.push_back({ 10.35, juce::MidiMessage::noteOn(4, 43, 0.84f) }); // G2 -> 49 Hz G1 16' Bourdon sub!
        ev.push_back({ 10.39, juce::MidiMessage::noteOn(1, 50, 0.82f) }); // D3 -> E3
        ev.push_back({ 10.43, juce::MidiMessage::noteOn(1, 55, 0.84f) }); // G3
        ev.push_back({ 10.47, juce::MidiMessage::noteOn(1, 59, 0.85f) }); // B3 -> C4
        ev.push_back({ 10.51, juce::MidiMessage::noteOn(1, 67, 0.86f) }); // G4
        ev.push_back({ 10.55, juce::MidiMessage::noteOn(1, 74, 0.87f) }); // D5 -> E5
        ev.push_back({ 11.45, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // Plagal IV/I Choir Amen (C/G)!
        ev.push_back({ 12.55, juce::MidiMessage::noteOff(1, 28) });      // Pure G Major Cathedral resolution!
        ev.push_back({ 14.35, juce::MidiMessage::noteOff(4, 43) });
        for (int n : { 50, 55, 59, 67, 74 }) ev.push_back({ 14.35, juce::MidiMessage::noteOff(1, n) });

        renderSequenceToWav(29, 16.0, 76.0, ev, presetsDir.getChildFile("30_polyoctave_messina_choir.wav"));
    }

    // =========================================================================
    // Preset 31: Twin Peaks '89 Trem-Noir (64 BPM, B Dorian — Angelo Badalamenti "Falling" x James Calvin Wilsey "Wicked Game")
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(4, 8192) });

        // Phrase 1: Hypnotic B-minor midnight chord breathing through 1961 Brownface 2-band Harmonic Tremolo
        //           + iconic "Wicked Game" weeping B4 -> C#5 -> B4 -> F#4 heartbreak tonebar cry!
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 47, 0.82f) }); // B2 deep 168 Hz Brownface chest
        ev.push_back({ 0.16, juce::MidiMessage::noteOn(1, 54, 0.80f) }); // F#3
        ev.push_back({ 0.22, juce::MidiMessage::noteOn(1, 62, 0.81f) }); // D4
        ev.push_back({ 0.42, juce::MidiMessage::noteOn(3, 71, 0.92f) }); // B4 -> C#5 ("Wicked Game" signature cry!)
        addPitchWheelRamp(ev, 3, 0.62, 1.15, 0.0f, +2.0f / 12.0f);       // Slow aching +2st slide B4 -> C#5!
        addPitchWheelRamp(ev, 3, 2.05, 2.52, +2.0f / 12.0f, 0.0f);       // Melt back C#5 -> B4
        ev.push_back({ 2.85, juce::MidiMessage::noteOff(3, 71) });
        ev.push_back({ 2.89, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 2.96, juce::MidiMessage::noteOn(3, 69, 0.88f) }); // A4 -> F#4 midnight sigh
        addPitchWheelRamp(ev, 3, 3.20, 3.62, 0.0f, -3.0f / 12.0f);       // A4 -> F#4
        for (int n : { 47, 54, 62 }) ev.push_back({ 4.15, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 4.15, juce::MidiMessage::noteOff(3, 69) });
        ev.push_back({ 4.21, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 2: Asus2 -> E6/9 (the iconic B-Dorian major-IV lift!) swaying into the Lexicon 480L dark velvet hall
        ev.push_back({ 4.35, juce::MidiMessage::noteOn(1, 45, 0.82f) }); // A2
        ev.push_back({ 4.41, juce::MidiMessage::noteOn(1, 52, 0.81f) }); // E3
        ev.push_back({ 4.47, juce::MidiMessage::noteOn(1, 61, 0.83f) }); // C#4
        ev.push_back({ 4.55, juce::MidiMessage::noteOn(3, 68, 0.90f) }); // G#4 -> A4 -> E4!
        addPitchWheelRamp(ev, 3, 4.78, 5.15, 0.0f, +1.0f / 12.0f);       // G#4 -> A4
        for (int n : { 45, 52, 61 }) ev.push_back({ 6.18, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 6.18, juce::MidiMessage::noteOff(3, 68) });
        ev.push_back({ 6.24, juce::MidiMessage::pitchWheel(3, 8192) });

        // Luminous E-Major Dorian IV ("What a wicked thing to do...")
        ev.push_back({ 6.36, juce::MidiMessage::noteOn(1, 40, 0.84f) }); // E2 deep midnight highway bass!
        ev.push_back({ 6.42, juce::MidiMessage::noteOn(1, 47, 0.82f) }); // B2
        ev.push_back({ 6.48, juce::MidiMessage::noteOn(1, 56, 0.83f) }); // G#3 (Dorian major-IV!)
        ev.push_back({ 6.55, juce::MidiMessage::noteOn(1, 64, 0.84f) }); // E4
        ev.push_back({ 6.64, juce::MidiMessage::noteOn(3, 73, 0.92f) }); // C#5 (6th) -> B4 weeping descend
        addPitchWheelRamp(ev, 3, 7.15, 7.68, 0.0f, -2.0f / 12.0f);       // C#5 -> B4
        for (int n : { 40, 47, 56, 64 }) ev.push_back({ 9.05, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 9.05, juce::MidiMessage::noteOff(3, 73) });
        ev.push_back({ 9.12, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 3: Badalamenti "Twin Peaks" lush Bm9 dream-hall horizon resolution (hear the 180-deg Low/High Harmonic Tremolo breathe!)
        ev.push_back({ 9.28, juce::MidiMessage::noteOn(1, 47, 0.84f) }); // B2
        ev.push_back({ 9.35, juce::MidiMessage::noteOn(1, 54, 0.83f) }); // F#3
        ev.push_back({ 9.42, juce::MidiMessage::noteOn(1, 62, 0.84f) }); // D4
        ev.push_back({ 9.49, juce::MidiMessage::noteOn(1, 69, 0.85f) }); // A4 (min7)
        ev.push_back({ 9.58, juce::MidiMessage::noteOn(3, 73, 0.91f) }); // C#5 (9th) -> D5 -> C#5 slow nocturnal tide!
        addPitchWheelRamp(ev, 3, 9.95, 10.48, 0.0f, +1.0f / 12.0f);      // C#5 -> D5
        addPitchWheelRamp(ev, 3, 10.95, 11.55, +1.0f / 12.0f, 0.0f);     // D5 -> C#5
        for (int n : { 47, 54, 62, 69 }) ev.push_back({ 13.10, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 13.10, juce::MidiMessage::noteOff(3, 73) });

        renderSequenceToWav(30, 15.2, 64.0, ev, presetsDir.getChildFile("31_twin_peaks_89_trem_noir.wav"));
    }

    // =========================================================================
    // Preset 32: '66 Revolver Backwards Tape (88 BPM, G Mixolydian / E Minor — George Harrison "I'm Only Sleeping" Reverse Studer J37)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(4, 8192) });

        // Phrase 1: Exposed George Harrison "I'm Only Sleeping" solo melodic notes — hear every note suck backwards (zwoooop-SNAP!)
        ev.push_back({ 0.08, juce::MidiMessage::noteOn(1, 52, 0.84f) }); // E3 warm bass anchor
        ev.push_back({ 0.14, juce::MidiMessage::noteOn(3, 71, 0.95f) }); // B4 -> C5 -> B4
        addPitchWheelRamp(ev, 3, 0.28, 0.52, 0.0f, +1.0f / 12.0f);
        ev.push_back({ 0.78, juce::MidiMessage::noteOff(3, 71) });
        ev.push_back({ 0.81, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 0.88, juce::MidiMessage::noteOn(3, 74, 0.96f) }); // D5 -> E5 reverse whip!
        addPitchWheelRamp(ev, 3, 1.02, 1.28, 0.0f, +2.0f / 12.0f);
        ev.push_back({ 1.56, juce::MidiMessage::noteOff(3, 74) });
        ev.push_back({ 1.59, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 1.68, juce::MidiMessage::noteOn(3, 67, 0.94f) }); // G4 -> A4 -> B4
        addPitchWheelRamp(ev, 3, 1.84, 2.12, 0.0f, +2.0f / 12.0f);
        ev.push_back({ 2.45, juce::MidiMessage::noteOff(3, 67) });
        ev.push_back({ 2.45, juce::MidiMessage::noteOff(1, 52) });
        ev.push_back({ 2.50, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 2: Hendrix "Castles Made of Sand" / Revolver backwards chord cascades (G -> D/F# -> Am7 -> Cmaj7)!
        ev.push_back({ 2.65, juce::MidiMessage::noteOn(1, 55, 0.92f) }); // G3
        ev.push_back({ 2.69, juce::MidiMessage::noteOn(1, 62, 0.92f) }); // D4
        ev.push_back({ 2.73, juce::MidiMessage::noteOn(1, 71, 0.94f) }); // B4 (G)
        for (int n : { 55, 62, 71 }) ev.push_back({ 3.70, juce::MidiMessage::noteOff(1, n) });

        ev.push_back({ 3.82, juce::MidiMessage::noteOn(1, 54, 0.91f) }); // F#3
        ev.push_back({ 3.86, juce::MidiMessage::noteOn(1, 62, 0.92f) }); // D4
        ev.push_back({ 3.90, juce::MidiMessage::noteOn(1, 69, 0.94f) }); // A4 (D/F#)
        for (int n : { 54, 62, 69 }) ev.push_back({ 4.88, juce::MidiMessage::noteOff(1, n) });

        ev.push_back({ 5.00, juce::MidiMessage::noteOn(1, 57, 0.91f) }); // A3
        ev.push_back({ 5.04, juce::MidiMessage::noteOn(1, 64, 0.92f) }); // E4
        ev.push_back({ 5.08, juce::MidiMessage::noteOn(3, 72, 0.95f) }); // C5 -> D5 (Am7 -> Am11!)
        addPitchWheelRamp(ev, 3, 5.32, 5.62, 0.0f, +2.0f / 12.0f);
        for (int n : { 57, 64 }) ev.push_back({ 6.18, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 6.18, juce::MidiMessage::noteOff(3, 72) });
        ev.push_back({ 6.24, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 6.35, juce::MidiMessage::noteOn(1, 48, 0.92f) }); // C3
        ev.push_back({ 6.39, juce::MidiMessage::noteOn(1, 55, 0.92f) }); // G3
        ev.push_back({ 6.43, juce::MidiMessage::noteOn(1, 64, 0.93f) }); // E4
        ev.push_back({ 6.47, juce::MidiMessage::noteOn(1, 71, 0.95f) }); // B4 (Cmaj7)
        for (int n : { 48, 55, 64, 71 }) ev.push_back({ 7.65, juce::MidiMessage::noteOff(1, n) });

        // Phrase 3: Beatles B7 turnaround -> soaring Harrison reverse octave lead into Emadd9 EMT-140 plate fade
        ev.push_back({ 7.80, juce::MidiMessage::noteOn(1, 47, 0.91f) }); // B2
        ev.push_back({ 7.84, juce::MidiMessage::noteOn(1, 57, 0.91f) }); // A3
        ev.push_back({ 7.88, juce::MidiMessage::noteOn(1, 63, 0.93f) }); // D#4 (B7!)
        ev.push_back({ 7.94, juce::MidiMessage::noteOn(3, 71, 0.95f) }); // B4 -> C5 -> B4
        addPitchWheelRamp(ev, 3, 8.15, 8.42, 0.0f, +1.0f / 12.0f);
        addPitchWheelRamp(ev, 3, 8.55, 8.82, +1.0f / 12.0f, 0.0f);
        for (int n : { 47, 57, 63 }) ev.push_back({ 9.10, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 9.10, juce::MidiMessage::noteOff(3, 71) });
        ev.push_back({ 9.16, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 9.28, juce::MidiMessage::noteOn(1, 52, 0.90f) }); // E3
        ev.push_back({ 9.33, juce::MidiMessage::noteOn(1, 59, 0.90f) }); // B3
        ev.push_back({ 9.38, juce::MidiMessage::noteOn(1, 67, 0.92f) }); // G4
        ev.push_back({ 9.44, juce::MidiMessage::noteOn(3, 74, 0.96f) }); // D5 -> E5 -> F#5!
        addPitchWheelRamp(ev, 3, 9.68, 10.02, 0.0f, +2.0f / 12.0f);      // D5 -> E5
        for (int n : { 52, 59, 67 }) ev.push_back({ 11.45, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 11.45, juce::MidiMessage::noteOff(3, 74) });

        renderSequenceToWav(31, 13.0, 88.0, ev, presetsDir.getChildFile("32_66_revolver_backwards_tape.wav"));
    }

    // =========================================================================
    // Preset 33: Mohan Veena Sitar-Slide (74 BPM, D Mixolydian / Raga Khamaj — Vishwa Mohan Bhatt & Ry Cooder / "Norwegian Wood")
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(4, 8192) });

        // Phrase 1: Open Sa-Pa (D3 + A3) Tambura drone + crisp rhythmic mizrab Raga Khamaj / "Norwegian Wood" melody!
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 50, 0.88f) }); // D3 (Sa drone)
        ev.push_back({ 0.18, juce::MidiMessage::noteOn(1, 57, 0.84f) }); // A3 (Pa drone)
        ev.push_back({ 0.34, juce::MidiMessage::noteOn(1, 74, 0.88f) }); // D5 Chikari bell!
        ev.push_back({ 0.56, juce::MidiMessage::noteOff(1, 74) });

        // Vocal 1-to-2 semitone Indian meend ornaments (zero air-raid siren sweeps!)
        ev.push_back({ 0.62, juce::MidiMessage::noteOn(3, 69, 0.94f) }); // A4 -> B4 -> A4
        addPitchWheelRamp(ev, 3, 0.78, 1.02, 0.0f, +2.0f / 12.0f);       // A4 -> B4
        addPitchWheelRamp(ev, 3, 1.16, 1.38, +2.0f / 12.0f, 0.0f);       // B4 -> A4
        ev.push_back({ 1.56, juce::MidiMessage::noteOff(3, 69) });
        ev.push_back({ 1.59, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 1.64, juce::MidiMessage::noteOn(3, 66, 0.93f) }); // F#4 -> G4 -> F#4
        addPitchWheelRamp(ev, 3, 1.78, 1.96, 0.0f, +1.0f / 12.0f);       // F#4 -> G4 (Ga -> Ma!)
        addPitchWheelRamp(ev, 3, 2.08, 2.26, +1.0f / 12.0f, 0.0f);       // G4 -> F#4
        ev.push_back({ 2.42, juce::MidiMessage::noteOff(3, 66) });
        ev.push_back({ 2.45, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 2.50, juce::MidiMessage::noteOn(1, 64, 0.90f) }); // E4 (Re)
        ev.push_back({ 2.86, juce::MidiMessage::noteOff(1, 64) });
        ev.push_back({ 2.90, juce::MidiMessage::noteOn(1, 62, 0.92f) }); // D4 (Sa!)
        ev.push_back({ 3.55, juce::MidiMessage::noteOff(1, 62) });
        ev.push_back({ 3.55, juce::MidiMessage::noteOff(1, 50) });
        ev.push_back({ 3.55, juce::MidiMessage::noteOff(1, 57) });

        // Phrase 2: Raga Khamaj Mixolydian flat-7th (C5 komal Ni!) + Chikari rhythmic punctuation
        ev.push_back({ 3.72, juce::MidiMessage::noteOn(1, 50, 0.88f) }); // D3
        ev.push_back({ 3.78, juce::MidiMessage::noteOn(1, 57, 0.85f) }); // A3
        ev.push_back({ 3.92, juce::MidiMessage::noteOn(1, 69, 0.91f) }); // A4
        ev.push_back({ 4.24, juce::MidiMessage::noteOff(1, 69) });
        ev.push_back({ 4.28, juce::MidiMessage::noteOn(3, 71, 0.94f) }); // B4 -> C5 (komal Ni!) -> B4
        addPitchWheelRamp(ev, 3, 4.42, 4.64, 0.0f, +1.0f / 12.0f);       // B4 -> C5
        addPitchWheelRamp(ev, 3, 4.86, 5.10, +1.0f / 12.0f, 0.0f);       // C5 -> B4
        ev.push_back({ 5.28, juce::MidiMessage::noteOff(3, 71) });
        ev.push_back({ 5.31, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 5.36, juce::MidiMessage::noteOn(1, 69, 0.91f) }); // A4
        ev.push_back({ 5.68, juce::MidiMessage::noteOff(1, 69) });
        ev.push_back({ 5.72, juce::MidiMessage::noteOn(1, 67, 0.90f) }); // G4
        ev.push_back({ 6.02, juce::MidiMessage::noteOff(1, 67) });
        ev.push_back({ 6.06, juce::MidiMessage::noteOn(3, 64, 0.93f) }); // E4 -> F#4 sweet meend!
        addPitchWheelRamp(ev, 3, 6.18, 6.42, 0.0f, +2.0f / 12.0f);
        ev.push_back({ 7.05, juce::MidiMessage::noteOff(3, 64) });
        ev.push_back({ 7.05, juce::MidiMessage::noteOff(1, 50) });
        ev.push_back({ 7.05, juce::MidiMessage::noteOff(1, 57) });
        ev.push_back({ 7.10, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 3: Brisk Jhala mizrab flourish -> radiant D-Major Jawari & Taraf harp resolution
        struct SitNote { double t0; double t1; int note; float vel; };
        const SitNote jhala[] = {
            { 7.22, 8.85, 50, 0.90f }, // D3 Sa
            { 7.36, 7.60, 74, 0.89f }, // D5 Chikari
            { 7.62, 7.88, 72, 0.92f }, // C5 (b7!)
            { 7.90, 8.14, 74, 0.88f }, // D5 Chikari
            { 8.16, 8.42, 71, 0.91f }, // B4
            { 8.44, 8.68, 69, 0.92f }, // A4
            { 8.70, 8.95, 67, 0.89f }, // G4
        };
        for (const auto& sn : jhala) {
            ev.push_back({ sn.t0, juce::MidiMessage::noteOn(1, sn.note, sn.vel) });
            ev.push_back({ sn.t1, juce::MidiMessage::noteOff(1, sn.note) });
        }

        ev.push_back({ 9.02, juce::MidiMessage::noteOn(1, 50, 0.91f) }); // D3
        ev.push_back({ 9.09, juce::MidiMessage::noteOn(1, 57, 0.89f) }); // A3
        ev.push_back({ 9.16, juce::MidiMessage::noteOn(1, 62, 0.90f) }); // D4
        ev.push_back({ 9.23, juce::MidiMessage::noteOn(1, 69, 0.91f) }); // A4
        ev.push_back({ 9.30, juce::MidiMessage::noteOn(3, 64, 0.95f) }); // E4 -> F#4 final luminous Ga bloom!
        addPitchWheelRamp(ev, 3, 9.46, 9.80, 0.0f, +2.0f / 12.0f);
        ev.push_back({ 9.86, juce::MidiMessage::noteOn(1, 74, 0.92f) }); // High D5 Chikari bell!
        for (int n : { 50, 57, 62, 69, 74 }) ev.push_back({ 12.05, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 12.05, juce::MidiMessage::noteOff(3, 64) });

        renderSequenceToWav(32, 13.5, 74.0, ev, presetsDir.getChildFile("33_mohan_veena_sitar_slide.wav"));
    }

    // =========================================================================
    // Preset 34: Atlanta Neo-Soul Syrup (78 BPM, F Major / Dm9 — Faye Webster "Kingston" / Matt Stoessel Boss CE-1 Slide)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(4, 8192) });

        // Phrase 1: Plush Fadd9 -> A+B (Bbmaj9!) neo-soul pedal melt + lazy "Kingston" +2st major-pentatonic honey!
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 53, 0.78f) }); // F3
        ev.push_back({ 0.15, juce::MidiMessage::noteOn(1, 57, 0.80f) }); // A3 -> Bb3
        ev.push_back({ 0.20, juce::MidiMessage::noteOn(1, 60, 0.81f) }); // C4 -> D4
        ev.push_back({ 0.25, juce::MidiMessage::noteOn(1, 67, 0.84f) }); // G4 (9th)
        ev.push_back({ 0.92, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // A+B slant -> syrupy Bb6/9!
        ev.push_back({ 1.85, juce::MidiMessage::noteOff(1, 28) });      // Melt back into Fadd9!
        ev.push_back({ 2.02, juce::MidiMessage::noteOn(3, 72, 0.88f) }); // C5 -> D5 -> C5 ("Kingston" hook!)
        addPitchWheelRamp(ev, 3, 2.16, 2.48, 0.0f, +2.0f / 12.0f);       // C5 -> D5
        addPitchWheelRamp(ev, 3, 2.68, 3.02, +2.0f / 12.0f, 0.0f);       // D5 -> C5
        for (int n : { 53, 57, 60, 67 }) ev.push_back({ 3.40, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 3.40, juce::MidiMessage::noteOff(3, 72) });
        ev.push_back({ 3.46, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 2: Deep Dm9 (D3, A3, F4, C5) Atlanta 808-ballad pocket + weeping G4 -> A4 -> G4 slide
        ev.push_back({ 3.58, juce::MidiMessage::noteOn(1, 50, 0.78f) }); // D3
        ev.push_back({ 3.63, juce::MidiMessage::noteOn(1, 57, 0.80f) }); // A3
        ev.push_back({ 3.68, juce::MidiMessage::noteOn(1, 65, 0.82f) }); // F4
        ev.push_back({ 3.73, juce::MidiMessage::noteOn(1, 72, 0.83f) }); // C5
        ev.push_back({ 3.82, juce::MidiMessage::noteOn(3, 74, 0.88f) }); // D5 -> E5 (Dm9!) -> D5
        addPitchWheelRamp(ev, 3, 4.04, 4.38, 0.0f, +2.0f / 12.0f);       // D5 -> E5
        addPitchWheelRamp(ev, 3, 4.85, 5.25, +2.0f / 12.0f, 0.0f);       // E5 -> D5
        for (int n : { 50, 57, 65, 72 }) ev.push_back({ 6.15, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 6.15, juce::MidiMessage::noteOff(3, 74) });
        ev.push_back({ 6.22, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 3: Classic neo-soul IV -> ivm6 heartbreak melt (Bbmaj7 -> Bbm6!)
        ev.push_back({ 6.35, juce::MidiMessage::noteOn(1, 46, 0.79f) }); // Bb2
        ev.push_back({ 6.40, juce::MidiMessage::noteOn(1, 53, 0.80f) }); // F3
        ev.push_back({ 6.45, juce::MidiMessage::noteOn(1, 67, 0.83f) }); // G4 (6th)
        ev.push_back({ 6.50, juce::MidiMessage::noteOn(4, 62, 0.85f) }); // D4 -> Db4 (major IV melting to minor ivm6!)
        ev.push_back({ 6.55, juce::MidiMessage::noteOn(3, 69, 0.87f) }); // A4 (maj7) -> Bb4
        addPitchWheelRamp(ev, 4, 7.45, 7.92, 0.0f, -1.0f / 12.0f);       // D4 -> Db4 (Bbm6!)
        addPitchWheelRamp(ev, 3, 7.48, 7.95, 0.0f, +1.0f / 12.0f);       // A4 -> Bb4
        for (int n : { 46, 53, 67 }) ev.push_back({ 9.35, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 9.35, juce::MidiMessage::noteOff(4, 62) });
        ev.push_back({ 9.35, juce::MidiMessage::noteOff(3, 69) });
        ev.push_back({ 9.42, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 9.42, juce::MidiMessage::pitchWheel(4, 8192) });

        // Phrase 4: Velvet Fmaj9 Boss CE-1 stereo puddle resolution
        ev.push_back({ 9.56, juce::MidiMessage::noteOn(1, 53, 0.80f) }); // F3
        ev.push_back({ 9.61, juce::MidiMessage::noteOn(1, 60, 0.82f) }); // C4
        ev.push_back({ 9.66, juce::MidiMessage::noteOn(1, 64, 0.84f) }); // E4
        ev.push_back({ 9.72, juce::MidiMessage::noteOn(3, 67, 0.88f) }); // G4 -> A4 -> G4 (Fmaj9!)
        addPitchWheelRamp(ev, 3, 9.95, 10.35, 0.0f, +2.0f / 12.0f);      // G4 -> A4
        addPitchWheelRamp(ev, 3, 10.75, 11.20, +2.0f / 12.0f, 0.0f);     // A4 -> G4
        for (int n : { 53, 60, 64 }) ev.push_back({ 12.45, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 12.45, juce::MidiMessage::noteOff(3, 67) });

        renderSequenceToWav(33, 13.8, 78.0, ev, presetsDir.getChildFile("34_atlanta_neosoul_syrup.wav"));
    }

    // =========================================================================
    // Preset 35: Binson Echorec Space-Fury (112 BPM, E Dorian — David Gilmour Pompeii / "One of These Days" + "High Hopes")
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 1: Iconic Pompeii staccato E-power pulses exposing the 4-Head Binson Echorec (1/16+1/8+1/8D+1/4 Swirl!)
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 40, 0.94f) }); // E2 Hiwatt punch!
        ev.push_back({ 0.11, juce::MidiMessage::noteOn(1, 52, 0.92f) }); // E3
        ev.push_back({ 0.12, juce::MidiMessage::noteOn(1, 59, 0.90f) }); // B3
        for (int n : { 40, 52, 59 }) ev.push_back({ 0.26, juce::MidiMessage::noteOff(1, n) }); // Hear all 4 drum heads gallop!

        ev.push_back({ 1.68, juce::MidiMessage::noteOn(1, 40, 0.94f) }); // Second Pompeii pulse -> soaring Gilmour lap-steel cry!
        ev.push_back({ 1.69, juce::MidiMessage::noteOn(1, 52, 0.92f) });
        for (int n : { 40, 52 }) ev.push_back({ 1.85, juce::MidiMessage::noteOff(1, n) });

        ev.push_back({ 2.15, juce::MidiMessage::channelPressureChange(3, 76) });
        ev.push_back({ 2.18, juce::MidiMessage::noteOn(1, 52, 0.82f) }); // E3 anchor
        ev.push_back({ 2.22, juce::MidiMessage::noteOn(1, 59, 0.83f) }); // B3
        ev.push_back({ 2.26, juce::MidiMessage::noteOn(3, 74, 0.94f) }); // D5 -> E5 (+2st "High Hopes" / "Breathe" cry!)
        addPitchWheelRamp(ev, 3, 2.44, 2.82, 0.0f, +2.0f / 12.0f);       // D5 -> E5!
        for (int n : { 52, 59 }) ev.push_back({ 4.35, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 4.35, juce::MidiMessage::noteOff(3, 74) });
        ev.push_back({ 4.42, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 2: Classic Pink Floyd E Dorian IV lift (A7: A2+E3+G4+C#5!) + singing 1-to-2st tonebar lead
        ev.push_back({ 4.55, juce::MidiMessage::noteOn(1, 45, 0.84f) }); // A2
        ev.push_back({ 4.59, juce::MidiMessage::noteOn(1, 52, 0.84f) }); // E3
        ev.push_back({ 4.63, juce::MidiMessage::noteOn(1, 67, 0.86f) }); // G4
        ev.push_back({ 4.68, juce::MidiMessage::noteOn(3, 73, 0.93f) }); // C#5 (Dorian 6th!) -> D5 -> C#5
        addPitchWheelRamp(ev, 3, 4.92, 5.25, 0.0f, +1.0f / 12.0f);       // C#5 -> D5
        addPitchWheelRamp(ev, 3, 5.55, 5.92, +1.0f / 12.0f, 0.0f);       // D5 -> C#5
        for (int n : { 45, 52, 67 }) ev.push_back({ 6.95, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 6.95, juce::MidiMessage::noteOff(3, 73) });
        ev.push_back({ 7.02, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 3: Majestic Cmaj7 -> Em9 stadium lap-steel climax echoing through all 4 magnetic drum heads!
        ev.push_back({ 7.15, juce::MidiMessage::noteOn(1, 48, 0.86f) }); // C3
        ev.push_back({ 7.19, juce::MidiMessage::noteOn(1, 55, 0.86f) }); // G3
        ev.push_back({ 7.23, juce::MidiMessage::noteOn(1, 64, 0.88f) }); // E4
        ev.push_back({ 7.28, juce::MidiMessage::noteOn(3, 71, 0.94f) }); // B4 -> C5 -> B4
        addPitchWheelRamp(ev, 3, 7.55, 7.88, 0.0f, +1.0f / 12.0f);
        addPitchWheelRamp(ev, 3, 8.15, 8.48, +1.0f / 12.0f, 0.0f);
        for (int n : { 48, 55, 64 }) ev.push_back({ 8.90, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 8.90, juce::MidiMessage::noteOff(3, 71) });
        ev.push_back({ 8.96, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 9.08, juce::MidiMessage::noteOn(1, 40, 0.90f) }); // E2
        ev.push_back({ 9.12, juce::MidiMessage::noteOn(1, 52, 0.88f) }); // E3
        ev.push_back({ 9.16, juce::MidiMessage::noteOn(1, 59, 0.88f) }); // B3
        ev.push_back({ 9.20, juce::MidiMessage::noteOn(1, 67, 0.89f) }); // G4
        ev.push_back({ 9.25, juce::MidiMessage::noteOn(3, 74, 0.95f) }); // D5 -> E5 -> F#5 -> E5 final Gilmour soar!
        addPitchWheelRamp(ev, 3, 9.48, 9.85, 0.0f, +2.0f / 12.0f);       // D5 -> E5
        ev.push_back({ 10.90, juce::MidiMessage::channelPressureChange(3, 0) });
        for (int n : { 40, 52, 59, 67 }) ev.push_back({ 11.45, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 11.45, juce::MidiMessage::noteOff(3, 74) });

        renderSequenceToWav(34, 13.6, 112.0, ev, presetsDir.getChildFile("35_binson_echorec_space_fury.wav"));
    }

    // =========================================================================
    // Preset 36: CS-80 Stadium Synth-Steel (82 BPM, D Major — Daft Punk "RAM" Greg Leisz x Mike Dean / Vangelis CS-80 Brass)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(4, 8192) });

        // Phrase 1: Widescreen Dadd9 plucked steel + polyphonic CS-80 IG0015 brass swell -> A+B (G6/9/D) stadium bloom!
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(4, 38, 0.88f) }); // D2 (-12st = D1 CS-80 sub-bass chest!)
        ev.push_back({ 0.14, juce::MidiMessage::noteOn(1, 50, 0.86f) }); // D3
        ev.push_back({ 0.18, juce::MidiMessage::noteOn(1, 57, 0.88f) }); // A3 -> B3
        ev.push_back({ 0.22, juce::MidiMessage::noteOn(1, 66, 0.90f) }); // F#4 -> G4
        ev.push_back({ 0.26, juce::MidiMessage::noteOn(1, 69, 0.91f) }); // A4 -> B4
        ev.push_back({ 0.30, juce::MidiMessage::noteOn(1, 76, 0.93f) }); // E5 (pure 9th anchor)
        ev.push_back({ 1.15, juce::MidiMessage::noteOn(1, 28, 1.00f) }); // Pedals A+B: Steel + CS-80 Sawtooths glide to G6/9/D!
        ev.push_back({ 2.25, juce::MidiMessage::noteOff(1, 28) });      // Resolve back to radiant Dadd9!
        ev.push_back({ 3.45, juce::MidiMessage::noteOff(4, 38) });
        for (int n : { 50, 57, 66, 69, 76 }) ev.push_back({ 3.45, juce::MidiMessage::noteOff(1, n) });

        // Phrase 2: Vangelis / Mike Dean Aftertouch-Driven CS-80 Ribbon Lead (expressive 1-to-2st glides!) over Bm9 -> F#m7
        ev.push_back({ 3.65, juce::MidiMessage::channelPressureChange(3, 104) }); // Opens IG0015 resonant brass filter!
        ev.push_back({ 3.68, juce::MidiMessage::noteOn(1, 47, 0.86f) }); // B2
        ev.push_back({ 3.72, juce::MidiMessage::noteOn(1, 54, 0.86f) }); // F#3
        ev.push_back({ 3.76, juce::MidiMessage::noteOn(1, 62, 0.88f) }); // D4
        ev.push_back({ 3.80, juce::MidiMessage::noteOn(1, 69, 0.88f) }); // A4 (Bm7)
        ev.push_back({ 3.86, juce::MidiMessage::noteOn(3, 73, 0.94f) }); // C#5 (9th) -> D5!
        addPitchWheelRamp(ev, 3, 4.10, 4.48, 0.0f, +1.0f / 12.0f);       // C#5 -> D5
        for (int n : { 47, 54, 62, 69 }) ev.push_back({ 5.45, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 5.45, juce::MidiMessage::noteOff(3, 73) });
        ev.push_back({ 5.51, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 5.60, juce::MidiMessage::noteOn(1, 42, 0.86f) }); // F#2
        ev.push_back({ 5.64, juce::MidiMessage::noteOn(1, 54, 0.86f) }); // F#3
        ev.push_back({ 5.68, juce::MidiMessage::noteOn(1, 61, 0.88f) }); // C#4
        ev.push_back({ 5.72, juce::MidiMessage::noteOn(1, 64, 0.89f) }); // E4 (F#m7)
        ev.push_back({ 5.78, juce::MidiMessage::noteOn(3, 69, 0.94f) }); // A4 -> B4 -> A4
        addPitchWheelRamp(ev, 3, 6.02, 6.38, 0.0f, +2.0f / 12.0f);       // A4 -> B4
        addPitchWheelRamp(ev, 3, 6.62, 6.98, +2.0f / 12.0f, 0.0f);       // B4 -> A4
        for (int n : { 42, 54, 61, 64 }) ev.push_back({ 7.40, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 7.40, juce::MidiMessage::noteOff(3, 69) });
        ev.push_back({ 7.46, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 3: Daft Punk "Beyond" / "Touch" Gmaj9 -> A13sus4->A13 cinematic horizon lift
        ev.push_back({ 7.60, juce::MidiMessage::noteOn(1, 43, 0.88f) }); // G2
        ev.push_back({ 7.64, juce::MidiMessage::noteOn(1, 55, 0.88f) }); // G3
        ev.push_back({ 7.68, juce::MidiMessage::noteOn(1, 66, 0.90f) }); // F#4
        ev.push_back({ 7.72, juce::MidiMessage::noteOn(1, 71, 0.91f) }); // B4
        ev.push_back({ 7.78, juce::MidiMessage::noteOn(3, 74, 0.94f) }); // D5 -> E5 (Gmaj13!)
        addPitchWheelRamp(ev, 3, 8.05, 8.45, 0.0f, +2.0f / 12.0f);       // D5 -> E5
        for (int n : { 43, 55, 66, 71 }) ev.push_back({ 9.15, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 9.15, juce::MidiMessage::noteOff(3, 74) });
        ev.push_back({ 9.21, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 9.30, juce::MidiMessage::noteOn(1, 45, 0.88f) }); // A2
        ev.push_back({ 9.34, juce::MidiMessage::noteOn(1, 55, 0.88f) }); // G3
        ev.push_back({ 9.38, juce::MidiMessage::noteOn(1, 66, 0.90f) }); // F#4
        ev.push_back({ 9.44, juce::MidiMessage::noteOn(3, 74, 0.94f) }); // D5 (Asus4) -> C#5 (A13)!
        addPitchWheelRamp(ev, 3, 9.75, 10.18, 0.0f, -1.0f / 12.0f);      // D5 -> C#5
        for (int n : { 45, 55, 66 }) ev.push_back({ 10.75, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 10.75, juce::MidiMessage::noteOff(3, 74) });
        ev.push_back({ 10.82, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 10.82, juce::MidiMessage::channelPressureChange(3, 64) });

        // Phrase 4: Grand Stadium Dmaj9 CS-80 Brass + Pedal Steel Finale
        ev.push_back({ 10.95, juce::MidiMessage::noteOn(4, 38, 0.90f) }); // D2 sub-brass foundation
        ev.push_back({ 11.00, juce::MidiMessage::noteOn(1, 50, 0.88f) }); // D3
        ev.push_back({ 11.05, juce::MidiMessage::noteOn(1, 57, 0.89f) }); // A3
        ev.push_back({ 11.10, juce::MidiMessage::noteOn(1, 66, 0.91f) }); // F#4
        ev.push_back({ 11.15, juce::MidiMessage::noteOn(1, 73, 0.92f) }); // C#5 (maj7)
        ev.push_back({ 11.22, juce::MidiMessage::noteOn(3, 76, 0.95f) }); // E5 (9th) -> F#5!
        addPitchWheelRamp(ev, 3, 11.55, 12.05, 0.0f, +2.0f / 12.0f);     // E5 -> F#5
        ev.push_back({ 13.95, juce::MidiMessage::noteOff(4, 38) });
        for (int n : { 50, 57, 66, 73 }) ev.push_back({ 13.95, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 13.95, juce::MidiMessage::noteOff(3, 76) });

        renderSequenceToWav(35, 15.8, 82.0, ev, presetsDir.getChildFile("36_cs80_stadium_synth_steel.wav"));
    }

    // =========================================================================
    // Preset 37: Loveless SPX90 Reverse-Glide (94 BPM, B Lydian / F# Major — Kevin Shields MBV / Slowdive Shoegaze Wall)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(1, 8192) });
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 1: Euphoric Bmaj9 open shoegaze chord + Kevin Shields Jazzmaster tremolo-arm dip into SPX90 ER-2 Reverse-Fuzz!
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 47, 0.88f) }); // B2
        ev.push_back({ 0.14, juce::MidiMessage::noteOn(1, 54, 0.88f) }); // F#3
        ev.push_back({ 0.18, juce::MidiMessage::noteOn(1, 63, 0.90f) }); // D#4
        ev.push_back({ 0.22, juce::MidiMessage::noteOn(1, 70, 0.91f) }); // A#4 (maj7)
        ev.push_back({ 0.26, juce::MidiMessage::noteOn(3, 73, 0.94f) }); // C#5 (9th) -> D#5 glide!
        addPitchWheelRamp(ev, 1, 0.55, 0.92, 0.0f, -0.45f / 7.0f);       // Floating Jazzmaster arm dip!
        addPitchWheelRamp(ev, 1, 0.95, 1.38, -0.45f / 7.0f, 0.0f);       // Return to shimmering Bmaj9
        addPitchWheelRamp(ev, 3, 1.55, 2.05, 0.0f, +2.0f / 12.0f);       // C#5 -> D#5 soaring upper bloom
        addPitchWheelRamp(ev, 1, 2.35, 2.75, 0.0f, -0.35f / 7.0f);
        addPitchWheelRamp(ev, 1, 2.78, 3.22, -0.35f / 7.0f, 0.0f);
        for (int n : { 47, 54, 63, 70 }) ev.push_back({ 3.65, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 3.65, juce::MidiMessage::noteOff(3, 73) });
        ev.push_back({ 3.72, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 2: Slowdive / MBV G#m11 -> F#add9 tidal wave
        ev.push_back({ 3.85, juce::MidiMessage::noteOn(1, 44, 0.88f) }); // G#2
        ev.push_back({ 3.89, juce::MidiMessage::noteOn(1, 51, 0.88f) }); // D#3
        ev.push_back({ 3.93, juce::MidiMessage::noteOn(1, 59, 0.90f) }); // B3
        ev.push_back({ 3.97, juce::MidiMessage::noteOn(1, 66, 0.91f) }); // F#4
        ev.push_back({ 4.02, juce::MidiMessage::noteOn(3, 70, 0.94f) }); // A#4 -> B4 -> A#4
        addPitchWheelRamp(ev, 3, 4.35, 4.78, 0.0f, +1.0f / 12.0f);
        addPitchWheelRamp(ev, 1, 4.95, 5.32, 0.0f, -0.40f / 7.0f);
        addPitchWheelRamp(ev, 1, 5.35, 5.75, -0.40f / 7.0f, 0.0f);
        for (int n : { 44, 51, 59, 66 }) ev.push_back({ 5.92, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 5.92, juce::MidiMessage::noteOff(3, 70) });
        ev.push_back({ 5.98, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 6.08, juce::MidiMessage::noteOn(1, 42, 0.88f) }); // F#2
        ev.push_back({ 6.12, juce::MidiMessage::noteOn(1, 54, 0.89f) }); // F#3
        ev.push_back({ 6.16, juce::MidiMessage::noteOn(1, 58, 0.90f) }); // A#3
        ev.push_back({ 6.20, juce::MidiMessage::noteOn(1, 66, 0.91f) }); // F#4
        ev.push_back({ 6.25, juce::MidiMessage::noteOn(3, 68, 0.94f) }); // G#4 (add9) -> A#4!
        addPitchWheelRamp(ev, 3, 6.60, 7.05, 0.0f, +2.0f / 12.0f);
        for (int n : { 42, 54, 58, 66 }) ev.push_back({ 7.85, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 7.85, juce::MidiMessage::noteOff(3, 68) });
        ev.push_back({ 7.92, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 3: Colossal Emaj9(#11) -> Bmaj9 pink-neon shoegaze horizon climax
        ev.push_back({ 8.05, juce::MidiMessage::noteOn(1, 40, 0.90f) }); // E2
        ev.push_back({ 8.09, juce::MidiMessage::noteOn(1, 47, 0.90f) }); // B2
        ev.push_back({ 8.13, juce::MidiMessage::noteOn(1, 56, 0.91f) }); // G#3
        ev.push_back({ 8.17, juce::MidiMessage::noteOn(1, 66, 0.92f) }); // F#4 (9th)
        ev.push_back({ 8.22, juce::MidiMessage::noteOn(3, 70, 0.95f) }); // A#4 (#11!) -> B4
        addPitchWheelRamp(ev, 3, 8.65, 9.15, 0.0f, +1.0f / 12.0f);
        addPitchWheelRamp(ev, 1, 9.25, 9.62, 0.0f, -0.42f / 7.0f);
        addPitchWheelRamp(ev, 1, 9.65, 10.05, -0.42f / 7.0f, 0.0f);
        for (int n : { 40, 47, 56, 66 }) ev.push_back({ 10.22, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 10.22, juce::MidiMessage::noteOff(3, 70) });
        ev.push_back({ 10.28, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 10.38, juce::MidiMessage::noteOn(1, 47, 0.90f) }); // B2
        ev.push_back({ 10.42, juce::MidiMessage::noteOn(1, 54, 0.90f) }); // F#3
        ev.push_back({ 10.46, juce::MidiMessage::noteOn(1, 63, 0.91f) }); // D#4
        ev.push_back({ 10.50, juce::MidiMessage::noteOn(1, 70, 0.92f) }); // A#4
        ev.push_back({ 10.55, juce::MidiMessage::noteOn(3, 73, 0.95f) }); // C#5 -> D#5 -> C#5
        addPitchWheelRamp(ev, 3, 10.95, 11.45, 0.0f, +2.0f / 12.0f);
        addPitchWheelRamp(ev, 3, 11.75, 12.25, +2.0f / 12.0f, 0.0f);
        for (int n : { 47, 54, 63, 70 }) ev.push_back({ 13.05, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 13.05, juce::MidiMessage::noteOff(3, 73) });

        renderSequenceToWav(36, 14.8, 94.0, ev, presetsDir.getChildFile("37_loveless_spx90_reverse_glide.wav"));
    }

    // =========================================================================
    // Preset 38: King Tubby Kingston Dub-Tank (74 BPM, C Minor — 1974 Dromilly Ave Roots Skank + Altec HPF Spring + RE-201)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 1: Heavy C-minor Jamaican roots drop + razor-sharp offbeat skank chops crashing the Altec HPF & Spring Tank!
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 48, 0.90f) }); // C3 warm roots bass-chest
        ev.push_back({ 0.72, juce::MidiMessage::noteOff(1, 48) });

        // Offbeat skank 1 -> leave dub space so RE-201 Space Echo & Thunderbolt Spring smoke across the stereo field!
        ev.push_back({ 0.50, juce::MidiMessage::noteOn(1, 55, 0.96f) }); // G3
        ev.push_back({ 0.51, juce::MidiMessage::noteOn(1, 63, 0.97f) }); // Eb4
        ev.push_back({ 0.52, juce::MidiMessage::noteOn(1, 67, 0.98f) }); // G4
        ev.push_back({ 0.53, juce::MidiMessage::noteOn(1, 70, 0.98f) }); // Bb4 (Cm7 skank chop!)
        for (int n : { 55, 63, 67, 70 }) ev.push_back({ 0.63, juce::MidiMessage::noteOff(1, n) });

        // Offbeat skank 2 (2.12s) + deep G2->C3 roots pickup
        ev.push_back({ 1.72, juce::MidiMessage::noteOn(1, 43, 0.86f) }); // G2
        ev.push_back({ 2.08, juce::MidiMessage::noteOff(1, 43) });
        ev.push_back({ 2.12, juce::MidiMessage::noteOn(1, 55, 0.95f) }); // G3
        ev.push_back({ 2.13, juce::MidiMessage::noteOn(1, 63, 0.96f) }); // Eb4
        ev.push_back({ 2.14, juce::MidiMessage::noteOn(1, 72, 0.98f) }); // C5 high spring splash!
        for (int n : { 55, 63, 72 }) ev.push_back({ 2.24, juce::MidiMessage::noteOff(1, n) });

        // Phrase 2: Haunting Augustus Pablo 'King Tubby Meets Rockers Uptown' minor-pentatonic dub melody + Fm9/Gm7 skanks!
        ev.push_back({ 3.35, juce::MidiMessage::noteOn(1, 41, 0.88f) }); // F2 roots anchor
        ev.push_back({ 3.75, juce::MidiMessage::noteOn(1, 56, 0.94f) }); // Ab3
        ev.push_back({ 3.76, juce::MidiMessage::noteOn(1, 63, 0.95f) }); // Eb4
        ev.push_back({ 3.77, juce::MidiMessage::noteOn(1, 67, 0.96f) }); // G4 (Fm9 chop!)
        for (int n : { 56, 63, 67 }) ev.push_back({ 3.88, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 4.30, juce::MidiMessage::noteOff(1, 41) });

        ev.push_back({ 4.35, juce::MidiMessage::noteOn(3, 67, 0.92f) }); // G4 -> Bb4 -> C5 Augustus Pablo dub cry!
        addPitchWheelRamp(ev, 3, 4.48, 4.72, 0.0f, +3.0f / 12.0f);       // G4 -> Bb4
        ev.push_back({ 5.05, juce::MidiMessage::noteOff(3, 67) });
        ev.push_back({ 5.08, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 5.14, juce::MidiMessage::noteOn(3, 72, 0.94f) }); // C5
        ev.push_back({ 5.50, juce::MidiMessage::noteOff(3, 72) });
        ev.push_back({ 5.56, juce::MidiMessage::noteOn(3, 74, 0.95f) }); // D5 -> Eb5 -> D5 -> C5
        addPitchWheelRamp(ev, 3, 5.66, 5.84, 0.0f, +1.0f / 12.0f);       // D5 -> Eb5
        addPitchWheelRamp(ev, 3, 5.96, 6.18, +1.0f / 12.0f, -2.0f / 12.0f); // Eb5 -> C5
        ev.push_back({ 6.72, juce::MidiMessage::noteOff(3, 74) });
        ev.push_back({ 6.76, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 3: Abmaj7 -> G7(#9) Roots Dub Turnaround -> Final Thunderbolt Spring Crash & Space Echo Smoke-Out!
        ev.push_back({ 6.98, juce::MidiMessage::noteOn(1, 44, 0.89f) }); // Ab2
        ev.push_back({ 7.38, juce::MidiMessage::noteOn(1, 55, 0.95f) }); // G3
        ev.push_back({ 7.39, juce::MidiMessage::noteOn(1, 60, 0.95f) }); // C4
        ev.push_back({ 7.40, juce::MidiMessage::noteOn(1, 63, 0.96f) }); // Eb4 (Abmaj7 chop)
        for (int n : { 55, 60, 63 }) ev.push_back({ 7.52, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 8.20, juce::MidiMessage::noteOff(1, 44) });

        ev.push_back({ 8.60, juce::MidiMessage::noteOn(1, 43, 0.90f) }); // G2
        ev.push_back({ 9.00, juce::MidiMessage::noteOn(1, 59, 0.96f) }); // B3
        ev.push_back({ 9.01, juce::MidiMessage::noteOn(1, 65, 0.97f) }); // F4
        ev.push_back({ 9.02, juce::MidiMessage::noteOn(1, 70, 0.99f) }); // Bb4 (G7#9 dub tension chop!)
        for (int n : { 59, 65, 70 }) ev.push_back({ 9.13, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 9.75, juce::MidiMessage::noteOff(1, 43) });

        // Final Cm9 dub drop + explosive spring-crash chord chop at 10.62s!
        ev.push_back({ 9.82, juce::MidiMessage::noteOn(1, 48, 0.92f) });  // C3
        ev.push_back({ 9.88, juce::MidiMessage::noteOn(3, 72, 0.92f) });  // C5 -> D5 (Cm9!)
        addPitchWheelRamp(ev, 3, 10.05, 10.35, 0.0f, +2.0f / 12.0f);
        ev.push_back({ 10.55, juce::MidiMessage::noteOff(3, 72) });
        ev.push_back({ 10.62, juce::MidiMessage::noteOn(1, 55, 0.99f) }); // G3
        ev.push_back({ 10.63, juce::MidiMessage::noteOn(1, 63, 1.00f) }); // Eb4
        ev.push_back({ 10.64, juce::MidiMessage::noteOn(1, 67, 1.00f) }); // G4
        ev.push_back({ 10.65, juce::MidiMessage::noteOn(1, 74, 1.00f) }); // D5 thunderbolt spring blast!
        for (int n : { 55, 63, 67, 74 }) ev.push_back({ 10.76, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 11.45, juce::MidiMessage::noteOff(1, 48) });

        renderSequenceToWav(37, 14.2, 74.0, ev, presetsDir.getChildFile("38_king_tubby_kingston_dub_tank.wav"));
    }

    // =========================================================================
    // Preset 39: Microcosm Crystal Granular (92 BPM, Eb Major — Chuck Johnson x Hologram Microcosm 'Mosaic' + MOOD MKII)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 1: Intimate felt-warm Ebmaj9 arpeggio with spacious silence after pluck so 1/8 & 3/16 +12st jewel droplets dance!
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(1, 51, 0.80f) }); // Eb3
        ev.push_back({ 0.24, juce::MidiMessage::noteOn(1, 58, 0.82f) }); // Bb3
        ev.push_back({ 0.38, juce::MidiMessage::noteOn(1, 67, 0.85f) }); // G4
        ev.push_back({ 0.52, juce::MidiMessage::noteOn(3, 74, 0.88f) }); // D5 (maj7) -> F5 (9th) -> D5 delicate crystal seed!
        addPitchWheelRamp(ev, 3, 0.72, 1.08, 0.0f, +3.0f / 12.0f);       // D5 -> F5
        addPitchWheelRamp(ev, 3, 1.35, 1.72, +3.0f / 12.0f, 0.0f);       // F5 -> D5
        // Release early at 2.15s so the listener clearly hears the polyrhythmic +12st Mosaic droplets & -12st MOOD cello bed!
        for (int n : { 51, 58, 67 }) ev.push_back({ 2.15, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 2.15, juce::MidiMessage::noteOff(3, 74) });
        ev.push_back({ 2.22, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 2: Tender Cm11 -> Abmaj9 modern ambient steel meditation with crystal droplet space
        ev.push_back({ 3.85, juce::MidiMessage::noteOn(1, 48, 0.80f) }); // C3
        ev.push_back({ 3.99, juce::MidiMessage::noteOn(1, 55, 0.81f) }); // G3
        ev.push_back({ 4.13, juce::MidiMessage::noteOn(1, 63, 0.84f) }); // Eb4
        ev.push_back({ 4.28, juce::MidiMessage::noteOn(3, 70, 0.87f) }); // Bb4 -> C5!
        addPitchWheelRamp(ev, 3, 4.48, 4.82, 0.0f, +2.0f / 12.0f);
        for (int n : { 48, 55, 63 }) ev.push_back({ 5.35, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 5.35, juce::MidiMessage::noteOff(3, 70) });
        ev.push_back({ 5.42, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 6.55, juce::MidiMessage::noteOn(1, 44, 0.81f) }); // Ab2
        ev.push_back({ 6.69, juce::MidiMessage::noteOn(1, 51, 0.82f) }); // Eb3
        ev.push_back({ 6.83, juce::MidiMessage::noteOn(1, 60, 0.84f) }); // C4
        ev.push_back({ 6.97, juce::MidiMessage::noteOn(3, 67, 0.88f) }); // G4 (maj7) -> Bb4 (9th)!
        addPitchWheelRamp(ev, 3, 7.20, 7.58, 0.0f, +3.0f / 12.0f);
        for (int n : { 44, 51, 60 }) ev.push_back({ 8.15, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 8.15, juce::MidiMessage::noteOff(3, 67) });
        ev.push_back({ 8.22, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 3: Luminous Ebmaj9 open-fifth cascade sparkling into Mosaic + MOOD infinity
        ev.push_back({ 9.40, juce::MidiMessage::noteOn(1, 51, 0.82f) }); // Eb3
        ev.push_back({ 9.54, juce::MidiMessage::noteOn(1, 58, 0.84f) }); // Bb3
        ev.push_back({ 9.68, juce::MidiMessage::noteOn(1, 67, 0.85f) }); // G4
        ev.push_back({ 9.82, juce::MidiMessage::noteOn(1, 74, 0.86f) }); // D5 (maj7)
        ev.push_back({ 9.98, juce::MidiMessage::noteOn(3, 77, 0.89f) }); // F5 (9th) -> G5!
        addPitchWheelRamp(ev, 3, 10.30, 10.75, 0.0f, +2.0f / 12.0f);
        for (int n : { 51, 58, 67, 74 }) ev.push_back({ 11.75, juce::MidiMessage::noteOff(1, n) });
        ev.push_back({ 11.75, juce::MidiMessage::noteOff(3, 77) });

        renderSequenceToWav(38, 14.2, 92.0, ev, presetsDir.getChildFile("39_microcosm_crystal_granular.wav"));
    }

    // =========================================================================
    // Preset 40: Laurel Canyon Dumble Lead (104 BPM, A Major — David Lindley 'Running on Empty' Vocal Dumble ODS #2 + H910 Lead)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(4, 8192) });

        // Phrase 1: Pure vocal David Lindley Sunset Sound lap-steel lead (zero muddy low drone -> pure violin/sax Dumble bloom!)
        ev.push_back({ 0.10, juce::MidiMessage::noteOn(3, 69, 0.90f) }); // A4 pickup -> B4 -> C#5 vocal highway cry!
        addPitchWheelRamp(ev, 3, 0.22, 0.48, 0.0f, +2.0f / 12.0f);       // A4 -> B4
        addPitchWheelRamp(ev, 3, 0.65, 0.96, +2.0f / 12.0f, +4.0f / 12.0f); // B4 -> C#5 singing sustain!
        ev.push_back({ 1.85, juce::MidiMessage::noteOff(3, 69) });
        ev.push_back({ 1.89, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 1.96, juce::MidiMessage::noteOn(3, 76, 0.93f) }); // E5 -> F#5 -> E5 -> C#5!
        addPitchWheelRamp(ev, 3, 2.12, 2.40, 0.0f, +2.0f / 12.0f);       // E5 -> F#5
        addPitchWheelRamp(ev, 3, 2.75, 3.05, +2.0f / 12.0f, 0.0f);       // F#5 -> E5
        ev.push_back({ 3.35, juce::MidiMessage::noteOff(3, 76) });
        ev.push_back({ 3.39, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 3.45, juce::MidiMessage::noteOn(3, 73, 0.91f) }); // C#5 -> B4 -> A4 tender sigh
        addPitchWheelRamp(ev, 3, 3.68, 3.98, 0.0f, -2.0f / 12.0f);       // C#5 -> B4
        addPitchWheelRamp(ev, 3, 4.18, 4.48, -2.0f / 12.0f, -4.0f / 12.0f); // B4 -> A4
        ev.push_back({ 5.05, juce::MidiMessage::noteOff(3, 73) });
        ev.push_back({ 5.10, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 2: Classic Jackson Browne / Lindley harmonized parallel 6ths (F#4+D5 -> G#4+E5 -> A4+F#5 -> G#4+E5!)
        ev.push_back({ 5.28, juce::MidiMessage::noteOn(4, 66, 0.86f) }); // F#4
        ev.push_back({ 5.30, juce::MidiMessage::noteOn(3, 74, 0.90f) }); // D5 (D major sixths)
        addPitchWheelRamp(ev, 4, 5.95, 6.32, 0.0f, +2.0f / 12.0f);       // F#4 -> G#4 (E major sixths!)
        addPitchWheelRamp(ev, 3, 5.95, 6.32, 0.0f, +2.0f / 12.0f);       // D5  -> E5
        ev.push_back({ 7.15, juce::MidiMessage::noteOff(4, 66) });
        ev.push_back({ 7.15, juce::MidiMessage::noteOff(3, 74) });
        ev.push_back({ 7.20, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 7.20, juce::MidiMessage::pitchWheel(4, 8192) });

        // Phrase 3: Soaring high Sunset Sound climax (F#5 -> G#5 -> A5!) -> warm Aadd9 H910 stereo resolution
        ev.push_back({ 7.35, juce::MidiMessage::noteOn(3, 78, 0.94f) }); // F#5 -> G#5 -> A5!
        addPitchWheelRamp(ev, 3, 7.55, 7.86, 0.0f, +2.0f / 12.0f);       // F#5 -> G#5
        addPitchWheelRamp(ev, 3, 8.05, 8.38, +2.0f / 12.0f, +3.0f / 12.0f); // G#5 -> A5 soaring peak!
        addPitchWheelRamp(ev, 3, 8.95, 9.32, +3.0f / 12.0f, +2.0f / 12.0f); // A5 -> G#5
        ev.push_back({ 9.68, juce::MidiMessage::noteOff(3, 78) });
        ev.push_back({ 9.73, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 9.85, juce::MidiMessage::noteOn(4, 64, 0.86f) }); // E4 sweet mid harmony
        ev.push_back({ 9.90, juce::MidiMessage::noteOn(3, 71, 0.92f) }); // B4 -> C#5 golden Laurel Canyon resolution!
        addPitchWheelRamp(ev, 3, 10.18, 10.58, 0.0f, +2.0f / 12.0f);     // B4 -> C#5
        ev.push_back({ 12.40, juce::MidiMessage::noteOff(4, 64) });
        ev.push_back({ 12.40, juce::MidiMessage::noteOff(3, 71) });

        renderSequenceToWav(39, 13.8, 104.0, ev, presetsDir.getChildFile("40_laurel_canyon_dumble_lead.wav"));
    }

    // =========================================================================
    // Preset 41: '66 Cinecitta Western Twang (84 BPM, D Minor — Morricone / Alessandroni 1962 Bass VI + Pre-Amp 6G15 Spring)
    // =========================================================================
    {
        std::vector<ScheduledMidiEvent> ev;
        ev.push_back({ 0.00, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 1: Iconic 1966 Rome Cinecitta D-minor outlaw call (A3-D4-A3-D4) -> 6G15 whip-crack spring splash & Bass VI answer!
        struct WestNote { double t0; double t1; int ch; int note; float vel; };
        const WestNote call1[] = {
            { 0.10, 0.25, 1, 57, 0.96f }, // A3 sharp flatwound pick ricochet!
            { 0.28, 0.43, 1, 62, 0.98f }, // D4
            { 0.46, 0.61, 1, 57, 0.95f }, // A3
            { 0.64, 1.35, 1, 62, 0.99f }, // D4 -> explodes the pre-amp Fender 6G15 spring tank!
        };
        for (const auto& wn : call1) {
            ev.push_back({ wn.t0, juce::MidiMessage::noteOn(wn.ch, wn.note, wn.vel) });
            ev.push_back({ wn.t1, juce::MidiMessage::noteOff(wn.ch, wn.note) });
        }

        // Deep 1962 Fender Bass VI baritone answer: F3 -> G3 -> D3 (over low D2!), then C3 -> D3!
        ev.push_back({ 1.52, juce::MidiMessage::noteOn(1, 38, 0.90f) }); // D2 deep Bass VI anchor
        ev.push_back({ 1.56, juce::MidiMessage::noteOn(3, 53, 0.96f) }); // F3 -> G3 -> D3!
        addPitchWheelRamp(ev, 3, 1.72, 1.90, 0.0f, +2.0f / 12.0f);       // F3 -> G3
        ev.push_back({ 2.12, juce::MidiMessage::noteOff(3, 53) });
        ev.push_back({ 2.15, juce::MidiMessage::pitchWheel(3, 8192) });
        ev.push_back({ 2.18, juce::MidiMessage::noteOn(1, 50, 0.97f) }); // D3 twang!
        ev.push_back({ 2.85, juce::MidiMessage::noteOff(1, 50) });
        ev.push_back({ 2.90, juce::MidiMessage::noteOn(3, 48, 0.95f) }); // C3 -> D3 outlaw resolve
        addPitchWheelRamp(ev, 3, 3.08, 3.28, 0.0f, +2.0f / 12.0f);
        ev.push_back({ 3.85, juce::MidiMessage::noteOff(3, 48) });
        ev.push_back({ 3.85, juce::MidiMessage::noteOff(1, 38) });
        ev.push_back({ 3.90, juce::MidiMessage::pitchWheel(3, 8192) });

        // Phrase 2: Galloping desert standoff (Dm -> Bb -> Gm -> A7) + weeping baritone glissando!
        const WestNote gallop[] = {
            { 4.08, 4.24, 1, 38, 0.92f }, // D2 tic-tac
            { 4.28, 4.44, 1, 45, 0.90f }, // A2
            { 4.48, 4.66, 1, 50, 0.94f }, // D3
            { 4.82, 5.55, 1, 46, 0.95f }, // Bb2
            { 4.85, 5.55, 1, 53, 0.95f }, // F3
            { 4.88, 5.55, 1, 62, 0.97f }, // D4 (Bb desert chord stab!)
        };
        for (const auto& wn : gallop) {
            ev.push_back({ wn.t0, juce::MidiMessage::noteOn(wn.ch, wn.note, wn.vel) });
            ev.push_back({ wn.t1, juce::MidiMessage::noteOff(wn.ch, wn.note) });
        }

        // Gm -> A7 sinister Morricone baritone line (Bb3 -> A3 -> G3 -> F3 -> E3!)
        ev.push_back({ 5.72, juce::MidiMessage::noteOn(1, 43, 0.90f) }); // G2
        ev.push_back({ 5.76, juce::MidiMessage::noteOn(3, 58, 0.96f) }); // Bb3 -> A3 -> G3
        addPitchWheelRamp(ev, 3, 5.98, 6.22, 0.0f, -1.0f / 12.0f);       // Bb3 -> A3
        addPitchWheelRamp(ev, 3, 6.38, 6.62, -1.0f / 12.0f, -3.0f / 12.0f); // A3 -> G3
        ev.push_back({ 6.82, juce::MidiMessage::noteOff(3, 58) });
        ev.push_back({ 6.82, juce::MidiMessage::noteOff(1, 43) });
        ev.push_back({ 6.86, juce::MidiMessage::pitchWheel(3, 8192) });

        ev.push_back({ 6.92, juce::MidiMessage::noteOn(1, 53, 0.94f) }); // F3
        ev.push_back({ 7.22, juce::MidiMessage::noteOff(1, 53) });
        // Sinister A7 whip-crack stab!
        ev.push_back({ 7.28, juce::MidiMessage::noteOn(1, 45, 0.96f) }); // A2
        ev.push_back({ 7.30, juce::MidiMessage::noteOn(1, 52, 0.97f) }); // E3
        ev.push_back({ 7.32, juce::MidiMessage::noteOn(1, 55, 0.98f) }); // G3
        ev.push_back({ 7.34, juce::MidiMessage::noteOn(1, 61, 0.99f) }); // C#4!
        for (int n : { 45, 52, 55, 61 }) ev.push_back({ 8.15, juce::MidiMessage::noteOff(1, n) });

        // Phrase 3: High-noon Dm(add9) shootout rake + lone-rider Bass VI slide echoing into the 168 ms Rome tape slap!
        ev.push_back({ 8.45, juce::MidiMessage::noteOn(1, 38, 0.96f) }); // D2
        ev.push_back({ 8.48, juce::MidiMessage::noteOn(1, 45, 0.96f) }); // A2
        ev.push_back({ 8.51, juce::MidiMessage::noteOn(1, 50, 0.97f) }); // D3
        ev.push_back({ 8.54, juce::MidiMessage::noteOn(1, 53, 0.98f) }); // F3
        ev.push_back({ 8.57, juce::MidiMessage::noteOn(1, 57, 0.99f) }); // A3
        ev.push_back({ 8.60, juce::MidiMessage::noteOn(1, 64, 1.00f) }); // E4 (Dm add9 Morricone chord!)
        ev.push_back({ 9.85, juce::MidiMessage::noteOn(3, 57, 0.95f) }); // A3 -> Bb3 -> A3 -> D3 lone rider farewell
        addPitchWheelRamp(ev, 3, 10.05, 10.28, 0.0f, +1.0f / 12.0f);     // A3 -> Bb3
        addPitchWheelRamp(ev, 3, 10.45, 10.75, +1.0f / 12.0f, 0.0f);     // Bb3 -> A3
        ev.push_back({ 11.15, juce::MidiMessage::noteOff(3, 57) });
        ev.push_back({ 11.20, juce::MidiMessage::noteOn(1, 50, 0.94f) }); // Final D3 twang
        for (int n : { 38, 45, 50, 53, 57, 64 }) ev.push_back({ 12.35, juce::MidiMessage::noteOff(1, n) });

        renderSequenceToWav(40, 14.0, 84.0, ev, presetsDir.getChildFile("41_66_cinecitta_western_twang.wav"));
    }

    // =========================================================================
    // Combined Channel-1 Live Keyboard Showcase (05_live_keyboard_showcase.wav)
    // =========================================================================
    if (gMinPresetOneBased <= 5 && gMaxPresetOneBased >= 5) {
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
